#include "wsd_Flat.h"
#include "wsd_cgy.h"
#include "wsd_1376_2.h"
#include "wsd_1376_2_fun.h"
#if(MD_DEV_PLC)

static TRouterInfo*  pRouterInfo;
static TRouter *pAppRouter;
static TPort *pPortRouter;
static TTask *pTKRouter;
static TRx *pRxRouter;
static TTx *pTxRouter;
TGy *pGyRouter;
DWORD gdwFreshTime;
BYTE *pTransBuf; //透明转发缓冲区
BYTE gbyDeamonFlag;
static BYTE byNowDay = 0;
static BOOL RouterInit(void);
static BOOL Init(void);
void ResetCollectEx2(BYTE byType);
static BYTE byOldStep;

static void Proxy_TxMonitor(void);
static void DealAfnF1_F1(BYTE *pBuf);
static void ClearConInfo(BYTE byID);

//////////////////////////////////////////////////////////////
//	函 数 名 : TK_376_2
//	函数功能 : 载波路由376.2协议
//	处理过程 :
//	备    注 : 
//	作    者 : 赵红军
//	时    间 : 2009年11月6日
//	返 回 值 : void
//	参数说明 : void
///////////////////////////////////////////////////////////////
void TK_376_2(void)
{
	StdTaskEntry(Init, sizeof(TRouter));
}

///////////////////////////////////////////////////////////////
//	函 数 名 : Init
//	函数功能 : 初始化
//	备    注 : 
//	作    者 : 赵红军
//	时    间 : 2009年11月6日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL Init(void)
{
	TPort *pPort = (TPort *)GethPort();
	TGyCfg *pGyCfg = GetGyCfg();
	TRouter *p = (TRouter *)GethApp();

	Trace("1376.2 init");
	pGyCfg->wGyMode = MGYM_SYS_MASTER;
	pGyCfg->wMinDevNum = 0;
	pGyCfg->wRxdBufSize = 512;
	pGyCfg->wTxdBufSize = 1024;
	pGyCfg->wTxdIdleTime = 800;	//接收帧与发送帧间隔
	pGyCfg->wCommIdleTime = 500;	//字符最大间隔 
	pGyCfg->wRxDelayMax = MAX_FRAME_DEALY;	//对方最长等待时间
	if (GY_Init() != TRUE)
		return FALSE;
	Register(MGYF_SEARCHONFRAME, (HPARA)SearchOneFrame);
	Register(MGYF_TXMONITOR, (HPARA)TxMonitor);
	Register(MGYF_RXMONITOR, (HPARA)RxMonitor);
	OS_OnCommand(MCMD_ONTIMEOUT, (HPARA)OnTimeOut);
	OS_SetTimer(MTID_USER, 1000);

	byNowDay = gSysTime.Day;
	pRouterInfo = (TRouterInfo*)MemAlloc(sizeof(TRouterInfo));
	CHECKPTR(pRouterInfo, FALSE);
	pTransBuf = MemAlloc(MAX_TRANS_BUF_SIZE);
	CHECKPTR(pTransBuf, FALSE);
	pPortRouter = pPort;
	pRxRouter = GetRx();
	pTxRouter = GetTx();
	pTKRouter = (TTask *)GethTask();
	pAppRouter = (TRouter *)GethApp();
	pGyRouter = (TGy *)GethGy();
	pRouterInfo->byChipType = MCHIP_NULL;
	p->bLogon = FALSE;
	p->byModuleType = 1;//默认为载波模块
	gbyDeamonFlag = 0;
	p->byRetryCnt = 0;
	p->wMPNum = GetPlcNum();
	OpenRouter();
	InitMainID();
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : SearchOneFrame
//	函数功能 : 检索帧格式
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 2009年11月6日
//	返 回 值 : DWORD
//	参数说明 : HPARA hBuf,DWORD dwLen
///////////////////////////////////////////////////////////////
DWORD SearchOneFrame(HPARA hBuf, DWORD dwLen)
{
	TRouter *p = (TRouter *)GethApp();
	TRouterInfo *pRInfo = GetRouterInfo();
	BYTE *pBuf = (BYTE *)hBuf;
	T376_2TxFm1 *pRxFm = (T376_2TxFm1 *)pBuf;
	BYTE *pData = (BYTE *)(pRxFm + 1);
	TGyCfg *pGyCfg;
	DWORD dwRc, dwFmLen;
	BYTE byCheck;

	if (dwLen < 15)
		return FM_LESS;
	dwRc = SearchHead1(hBuf, dwLen, 0, 0x68);
	if (dwRc)
		return FM_ERR | dwRc;
	dwFmLen = pRxFm->wLen;	//检验帧长度是否合格
	if (dwFmLen >= 512)
		return FM_ERR | 1;
	if (dwFmLen > dwLen)
		return FM_LESS;

	if (!(pRxFm->byCtrl & 0x80))
		return FM_ERR | 1;

	if (0x16 != pBuf[dwFmLen - 1])
		return FM_ERR | 1;

	byCheck = CheckSum(&pRxFm->byCtrl, dwFmLen - 5);
	if (byCheck != pBuf[dwFmLen - 2])
	{
		return FM_ERR | dwFmLen;
	}

	p->dwLastRxTime = GetRunCount();
	p->byModuleType = pRxFm->byCtrl & 0x3F;
	pGyCfg = GetGyCfg();
	pGyCfg->wRxDelayMax = MAX_FRAME_DEALY; //对方最长等待时间
	return FM_OK | dwFmLen;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : AdaptCommuIdleTime
//	函数功能 : 根据模块的收发间隔时间，调整规约的字符等待间隔
//	处理过程 : 
//	备    注 : 
//	作    者 : 王可川
//	时    间 : 2016年3月7日
//	返 回 值 : void
//	参数说明 : DWORD dwTime
///////////////////////////////////////////////////////////////
static void AdaptCommuIdleTime(DWORD dwTime)
{
	TRouter *p = (TRouter *)GethApp();
	TGyCfg *pGyCfg = GetGyCfg();
	WORD wIdleTime;
	DWORD dwSumTime = 0;
	BYTE byI, byJ, byNum = 0;

	for (byI = 0; byI<MAX_RXTIME_NUM; byI++)
	{
		if (p->adwRxMonitorInTime[byI] == 0)
			break;
	}
	if (byI)
	{//和上一次接收相比，间隔时间超过1分钟，恢复默认值
		if ((DWORD)(dwTime - p->adwRxMonitorInTime[byI - 1]) >= 60000)
		{
			if (pGyCfg->wCommIdleTime != STAND_COMMUIDLE_TIME)
			{
				pGyCfg->wCommIdleTime = STAND_COMMUIDLE_TIME;
			}
			memset(p->adwRxMonitorInTime, 0, sizeof(p->adwRxMonitorInTime));
			return;
		}
	}
	if (byI < MAX_RXTIME_NUM)
		p->adwRxMonitorInTime[byI] = dwTime;

	if (++byI >= MAX_RXTIME_NUM)
	{//连续收到(MAX_RXTIME_NUM-1)帧后，进行间隔判断
		for (byJ = 0; byJ<MAX_RXTIME_NUM - 1; byJ++)
		{
			wIdleTime = (WORD)(p->adwRxMonitorInTime[byJ + 1] - p->adwRxMonitorInTime[byJ]);
			if (wIdleTime < pGyCfg->wCommIdleTime)
			{
				dwSumTime += wIdleTime;
				byNum++;
			}
		}
		if (byNum >= 7)
		{//超过7帧小于规约的字符等待间隔，需要进行调整
			wIdleTime = (WORD)(dwSumTime / byNum);
			wIdleTime = (wIdleTime / 50) * 50;
			if (wIdleTime < 100)
				wIdleTime = 100;
			else if (wIdleTime > 500)
				wIdleTime = 500;
			//Trace("进行规约间隔调整[%d]-[%d]",pGyCfg->wCommIdleTime,wIdleTime);
			pGyCfg->wCommIdleTime = wIdleTime;
		}
		memset(p->adwRxMonitorInTime, 0, sizeof(p->adwRxMonitorInTime));
	}
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxMonitor
//	函数功能 : 接收处理
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 
//	返 回 值 : void
//	参数说明 : void
///////////////////////////////////////////////////////////////
void TxMonitor(void)
{
	TRouter *p = (TRouter *)GethApp();
	TRouterInfo *pRInfo = pRouterInfo;
	static DWORD dwOldTime = 0;
	TGy *pGy = (TGy *)GethGy();
	DWORD dwTime, dwTmp;
	BYTE byBroadTime;
	BOOL bRc;
	BOOL bStudy=FALSE;
	BOOL bResetRouter;

	p->byBusy = FALSE;
	p->dwTxMonitorInTime = GetRunCount();

	if (byOldStep != p->byStatus)
	{
		byOldStep = p->byStatus;
		Trace("bystep is %d", p->byStatus);
	}
	switch (p->byStatus)
	{
	default:
	case MST_RTCHK: //开机初始化状态
		if (!RouterChk())
		{
			break;
		}
		p->byInitErrCount = 0;
		p->byStatus = MST_INIT;
	case MST_INIT:	//路由初始化(档案同步在此步完成)
		if (RouterInit())
		{
			pRInfo->byWork = RTWORK_IDLE;
			p->byRouterStart = FALSE;
			p->byStatus = MST_REST;
			gbyDeamonFlag = 0;
			p->byInitErrCount = 0;
		}
		else
		{//异常初始化检查
			p->byInitErrCount++;
			//加入模块重新初始化
			if (p->byInitErrCount > 10)
			{//复位回归到初始化
				OpenRouter();
			}
			break;
		}
	case MST_REST:
		if (IsWorkTime())
		{
			//路由启动成功进入工作状态
			if (gbyDeamonFlag)
			{
				TGyCfg *pGyCfg = GetGyCfg();
				pGyCfg->wRxDelayMax = MAX_FRAME_DEALY; //对方最长等待时间
				p->byInitStep = INISTEP_CHKIDPARA;
				p->byStatus = MST_INIT;
			}
			else
			{
				bRc = StartRouter();
				if (bRc)
				{//本次进入抄表的时间				
					p->dwRxDataCount = 0;
					gbyDeamonFlag = 0;
					pRInfo->byWork = RTWORK_IDLE;
					p->dwPauseTmStamp = 0;
					p->byStatus = MST_WORK;
					if (p->bStuFirst && (GetPlcNum()!=0))
					{//无线模块要求先组网后抄表 
						p->byStatus = MST_STUDY;
						pRInfo->byRunStatuse &= (~BIT0);
//						pRInfo->dwNetBuildSTime = GetSecs();
//						pRInfo->dwNetBuildETime = 0;
						pRInfo->wNetnum = pRInfo->wDeNetnum = 0;
						bStudy = FALSE;
						if (p->byRouterStart == TRUE)
							StopRouter();
						break;
					}
				}
				else
				{//重新检查模块
					p->byInitErrCount++;
					if (p->byInitErrCount > 10)
					{//复位回归到初始化
						TGyCfg *pGyCfg = GetGyCfg();
						pGyCfg->wRxDelayMax = MAX_FRAME_DEALY; //对方最长等待时间					
						OpenRouter();
					}
				}
			}
		}
		break;
	case MST_WORK:
		if (!IsWorkTime())
		{
			TGyCfg *pGyCfg = GetGyCfg();
			pGyCfg->wRxDelayMax = MAX_FRAME_DEALY; //对方最长等待时间
			StopRouter();
			p->byStatus = MST_REST;
			pRInfo->byWork = RTWORK_IDLE;

			if ((p->wMPNum && (p->dwRxDataCount == 0)))
			{//1个有效周期没有没有收到数据
				OpenRouter();
			}
			return;
		}
		if ((DWORD)(GetRunCount() - p->dwLastRxTime) >= 10800000)
		{//3小时没有通信,关闭模块重来
			OpenRouter();
			p->dwLastRxTime = GetRunCount();
			return;
		}
		if (gbyDeamonFlag)//在INISTEP_CHKIDPARA中删除
		{//1、重新启动参数同步
			DWORD dwDelay = GetRunCount() - gdwFreshTime;
			TGyCfg *pGyCfg = GetGyCfg();
			pGyCfg->wRxDelayMax = MAX_FRAME_DEALY; //对方最长等待时间
			if (dwDelay >= 38000)
			{
				StopRouter();
				p->byInitStep = INISTEP_CHKIDPARA;
				p->byStatus = MST_INIT;
			}
			return;
		}
		if ((DWORD)(GetRunCount() - p->dwLastRxTime) >= 20 * 60 * 1000) //zhj 2014.11.12
		{//20分钟无通信查询路由状态		
			if ((DWORD)(GetRunCount() - p->dwLastReqStatus) > 1800000)
			{//首次50分钟，以后是30分钟发送一次
				p->dwLastReqStatus = GetRunCount();
				TxAfn03_F1();
				ResumeRouter();
			}
		}
		else
			p->dwLastReqStatus = GetRunCount();
		if (!TxPaused())
		{
			Proxy_TxMonitor();
		}
		break;
	case MST_STUDY:
		if (gbyDeamonFlag)
		{
			if ((DWORD)(GetRunCount() - gdwFreshTime) > 30 * 1000)
			{
				p->byInitStep = INISTEP_CHKIDPARA;
				p->byStatus = MST_INIT;
				break;
			}
		}
		dwTime = GetSecs();
		if (pRInfo->byCommMode == PLC_WIDEBAND)
		{
			if ((DWORD)(dwTime - Var.dwStartTime) < 1 * 60)
				break;
		}
		COMPARECHK0(dwOldTime, dwTime);
		COMPARECHK(dwOldTime, dwTime);
		if ((DWORD)(dwTime - dwOldTime) > 10)//30秒查询一次
		{
			if (ChkStudySta())
			{
				dwOldTime = dwTime;
				if ((pRInfo->byRunStatuse&BIT0) == BIT0)
					bStudy = TRUE;
				else
					bStudy = FALSE;
			}
#if 0
			//判断组网时间
			if (p->byStatus != MST_WORK)
			{
				dwTime = GetSecs();
				if (dwTime < pRInfo->dwNetBuildSTime)
				{
					if ((dwTime / 86400) == (pRInfo->dwNetBuildSTime / 86400))	//校时前后在同一天不复位路由
						bResetRouter = FALSE;
					else
						bResetRouter = TRUE;
				}
				else
				{
					if ((DWORD)(dwTime - pRInfo->dwNetBuildSTime) > 18000)		//最长5个小时
						bResetRouter = TRUE;
					else
						bResetRouter = FALSE;
				}
				if (bResetRouter == TRUE)
				{//复位路由模块
					StopRouter();
					OpenRouter();
					pRInfo->byWork = RTWORK_IDLE;
					break;
				}
			}
#endif
			if (TRUE == bStudy)
			{
				p->byStatus = MST_WORK;
//				pRInfo->dwNetBuildETime = GetSecs();
				if ((p->byRouterStart == FALSE) && (pRInfo->byCommMode == PLC_NARROWBAND))
					ResumeRouter();
			}
		}
		break;
	}
}

///////////////////////////////////////////////////////////////
//	函 数 名 : RxMonitor
//	函数功能 : 接收处理
//	处理过程 : 
//	备    注 : 
//	作    者 : 赵红军
//	时    间 : 2009年11月6日
//	返 回 值 : void
//	参数说明 : void
///////////////////////////////////////////////////////////////
void RxMonitor(void)
{
	TRouter *p = (TRouter *)GethApp();
	TRouterInfo *pRInfo = GetRouterInfo();
	T376_2RxFm *pRxFm = (T376_2RxFm *)GetRxFm();
	BYTE *pBuf = (BYTE *)(pRxFm + 1);
	WORD wFn, wAddrEx, wLen = 0;
	DWORD dwAddr;
	BYTE byAfn, byErr;
	TRx *pRx = GetRx();

	AdaptCommuIdleTime(GetRunCount());

	wLen += (WORD)DealInfoField(pRxFm);
	byAfn = pBuf[wLen++];
	wFn = DT2Fn(pBuf[wLen], pBuf[wLen + 1]);
	wLen += 2;
	switch (byAfn)
	{
	case AFN03:
		if (wFn == F10) //主动上报运行模式信息
		{
			RxAfn03_F10(&pBuf[wLen], pRxFm->wLen - 15);
			p->bySeq = pRxFm->bySeq;
			p->bLogon = TRUE;
			TxAfn00_F1(0xFFFFFFFF, 0);
			FreshRTSta(RTST_IDENTIFYOK, NULLP, NULLP, NULLP);
			//模块复归发出
			if (p->byStatus != MST_RTCHK)
				Trace("模块异常,载波模块出现复位!!!");
			p->byStatus = MST_INIT;
			p->byInitStep = 0;
			p->byInitErrCount = 0;	//zhj 2015.6.15 add
			p->byRouterStart = FALSE;
		}
		break;
	case AFN14:
		p->bySeq = pRxFm->bySeq;
		dwAddr = MDW(pBuf[wLen + 4], pBuf[wLen + 3], pBuf[wLen + 2], pBuf[wLen + 1]);
		wAddrEx = MW(pBuf[wLen + 6], pBuf[wLen + 5]);
		if (wFn == F1)	//请求抄读内容
			TC_QueryDI(dwAddr, wAddrEx);
		if (wFn == F2) //请求时钟
			TxAfn14_F2();
		if (wFn == F3) //请求依通信延时修正通信数据
		{
			//!!
		}
		break;
	case AFN06:
		p->bySeq = pRxFm->bySeq;
		if (wFn == F1) //从节点信息主动上报 
		{
			FreshRTSta(RTST_SEARCH, 0, 8, 2);
			TxAfn00_F1(0xFFFFFFFF, 0);
			break;
		}
		if (wFn == F2) //数据主动上报
		{
			SendFm(PORT_ZB,&pBuf[wLen + 6], pBuf[wLen + 5],0x06f2,0,TRUE);
			TxAfn00_F1(0xFFFFFFFF, 0);
			break;
		}
		if (wFn == F3) //上报工况变动
		{
			TxAfn00_F1(0xFFFFFFFF, 0); //回复确认
			if (pBuf[wLen] == 1)
			{//抄表任务结束
				FreshRTSta(RTST_WORK, 0, 7, 1);
			}
			if (pBuf[wLen] == 2)
			{//搜表结束			
				FreshRTSta(RTST_SEARCH, 0, 7, 2);
			}
			break;
		}
		if (wFn == F4) //上报从节点信息
		{
			FreshRTSta(RTST_SEARCH, 0, 8, 2);
			TxAfn00_F1(0xFFFFFFFF, 0);
			break;
		}
		if (wFn == F5) //上报事件
		{
			DealAfn06_F5(&pBuf[wLen]); //处理路由主动上报电能表事件
			TxAfn00_F1(0xFFFFFFFF, 0);
			break;
		}
		break;
	case AFN02:
		break;
	case AFN13: //主动抄表数据返回
		if (wFn == F1)
		{
			//AFN13 F1返回数据内容
			if (!SendFm(PORT_ZB, &pBuf[wLen + 4], pBuf[wLen + 3], 0x1301,0,FALSE))
				break;
			p->byRetryCnt = 0;
			p->dwPauseTmStamp = 0;
		}
		break;
	case AFNF1:
		if (wFn == F1)
			DealAfnF1_F1(&pBuf[wLen]);
		break;
	case AFN00:
		byErr = pBuf[wLen++];
		if (wFn == F2 && byErr == 12) //节点不在网
		{
			p->byRetryCnt = 100;
		}
		if (wFn == F2 && byErr == 9) //CAC忙
		{
			PauseTx(60); //抄表暂停120秒
		}
		if (wFn == F2 && byErr == 11) //从节点无应答
		{
			p->dwPauseTmStamp = 0;
		}
	}
	p->byBusy = FALSE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : OnTimeOut
//	函数功能 : 定时处理
//	处理过程 : 
//	备    注 : 
//	作    者 : 赵红军
//	时    间 : 2009年11月6日
//	返 回 值 : void
//	参数说明 : void
///////////////////////////////////////////////////////////////
void OnTimeOut(DWORD dwTimerID)
{
	TRouter *p = (TRouter *)GethApp();
	TRouterInfo *pRInfo = GetRouterInfo();
	WORD dwTime;
	BYTE byI;

	if ((gSysTime.Hour == 23) && (gSysTime.Min >= 30))
	{
		DWORD dwIntervalTime;
		dwIntervalTime = (DWORD)(GetRunCount() - p->dwTxMonitorInTime);
		if (!IsWorkTime())
		{//不在时段内，路由状态不能保持在MST_WORK状态
			if (p->byStatus == MST_WORK)
			{
				if (p->byRouterStart && (dwIntervalTime >= 10000))
				{//延迟10s，防止正常运行时，OnTimeOut比TxMonitor先处理
					StopRouter();
					Trace("间隔时间[%d],路由工作异常，暂停!!!", dwIntervalTime);
				}
			}
		}
	}
	dwTime = GetSecs();
	for (byI = 0; byI < MAX_CONCURRENT_NUM;byI++)
	{
		if (p->Concurrent.ConFmInfo[byI].byValid != 0)
		{
			if ((DWORD)(dwTime - p->Concurrent.ConFmInfo[byI].dwTime) > 65)
				ClearConInfo(byI);
		}
	}


}

///////////////////////////////////////////////////////////////
//	函 数 名 : RouterInit
//	函数功能 : 路由初始化
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 2011年4月27日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL RouterInit(void)
{
	TRouter *p = (TRouter *)GethApp();

	switch (p->byInitStep)
	{
	default:
	case INISTEP_CHKMAINID:	//1、查询并设置主节点地址
		p->bStuFirst = FALSE;
		if (TxAfn03_F4())
		{
			p->byInitStep = INISTEP_CHKIDPARA;
		}
		else 
			break;
	case INISTEP_CHKIDPARA:	//2、查询从节点参数
		if (CheckIDPara())
		{
			p->bStuFirst = FALSE;
			p->byInitStep = INISTEP_SETPARA;
		}
		else break;
	case INISTEP_SETPARA:	//3、特殊模块处理操作，目前未用
		if (SetRTPara())
			p->byInitStep = INISTEP_READY;
		else break;
	case INISTEP_READY:
		p->byInitStep = 0xFF;
		return TRUE;
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : FreshIDPara
//	函数功能 : 刷新路由节点参数
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 2011年4月28日
//	返 回 值 : void
//	参数说明 : bThisTime - 1:立即执行同步 0:延时执行
///////////////////////////////////////////////////////////////
void FreshIDPara(BOOL bThisTime)
{
	if (Var.bySysInit & BIT0)
	{
		if (bThisTime)
			gdwFreshTime = GetRunCount() - 30000;
		else
			gdwFreshTime = GetRunCount();
		gbyDeamonFlag = 1;
	}
}

///////////////////////////////////////////////////////////////
//	函 数 名 : RouterChk
//	函数功能 : 检查模块类型
//	处理过程 : 
//	备    注 : 
//	作    者 : 赵红军
//	时    间 : 2010年12月11日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL RouterChk(void)
{
	TRouter *p = (TRouter *)GethApp();
	static BYTE byFailCnt = 0;
	BYTE ModeType[] = { 1, 10, 2, 3, 20 };

	FreshRTSta(RTST_ORIGIN, NULLP, NULLP, NULLP);
#if(!MOS_MSVC)
	if ((DWORD)(GetRunCount() - p->dwLastResetTime) < 60000)
	{//启动1min内等待上报版本信息(按互换性要求从复位模块起应为67秒),没有上报则进入主动查询流程
		if (!p->bLogon)
			return FALSE;
	}
#endif
	//查询版本信息
	if (!CheckVerInfo())
	{
		p->byModuleType = ModeType[byFailCnt % 5]; //更改类型问宽带、窄带、无线等
		if (++byFailCnt > 50)
		{
			OpenRouter();
			Trace("查询厂商代码50次无响应->复位");
			byFailCnt = 0;
		}
		return FALSE;
	}
	byFailCnt = 0;
	FreshRTSta(RTST_IDENTIFYOK, NULLP, NULLP, NULLP);
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : PauseTx
//	函数功能 : CAC忙时暂停发送n秒
//	处理过程 : 
//	备    注 : 
//	作    者 : Administrator
//	时    间 : 2013年5月7日
//	返 回 值 : void
//	参数说明 : DWORD dwSeconds
///////////////////////////////////////////////////////////////
void PauseTx(DWORD dwSeconds)
{
	TRouter *p = (TRouter *)GethApp();

	p->dwPauseTmStamp =  GetSecs();
	p->dwPauseTm = dwSeconds;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxPaused
//	函数功能 : 是否处于发送暂停状态（CAC忙时）
//	处理过程 : 
//	备    注 : 
//	作    者 : Administrator
//	时    间 : 2013年5月7日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL TxPaused(void)
{
	TRouter *p = (TRouter *)GethApp();
	DWORD dwNow =  GetSecs();

	if (p->dwPauseTmStamp == 0)
		return FALSE;
	COMPARECHK(p->dwPauseTmStamp, dwNow);
	if ((DWORD)(dwNow - p->dwPauseTmStamp) > p->dwPauseTm)
	{
		p->dwPauseTmStamp = 0;
		p->dwPauseTm = 0;
		return FALSE;
	}
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : ClearRouterID698
//	函数功能 : 删除所有载波从节点
//	处理过程 : 
//	备    注 : 
//	作    者 : 赵红军
//	时    间 : 2010年9月29日
//	返 回 值 : void
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL ClearRouterID698(void)
{
	TRouter *p = pAppRouter;

	if (p->byRouterStart)
		StopRouter();
	//参数区初始化
	return TxAfn01_F2();
}

///////////////////////////////////////////////////////////////
//	函 数 名 : StartRouter
//	函数功能 : 启动路由
//	处理过程 : 
//	备    注 : 从头开始抄表
//	作    者 : 赵红军
//	时    间 : 2009年11月6日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL StartRouter(void)
{
	TRouter *p = pAppRouter;
	TRouterInfo *pRInfo = pRouterInfo;

	FreshRTSta(RTST_START, NULLP, NULLP, NULLP);
	//仅支持集中器主导的抄表模式
	if (pRInfo->byCBMode == 0x01)
	{
		TxAfn12_F2();			//暂停路由
		p->byRouterStart = TRUE;
		FreshRTSta(RTST_START, 0, NULLP, 1);
		return TRUE;
	}
	//支持路由主导的抄表模式
	if (TxAfn12_F1())
	{
		p->byRouterStart = TRUE;
		FreshRTSta(RTST_START, 0, NULLP, 1);
		return TRUE;
	}
	FreshRTSta(RTST_START, 0, NULLP, 2);
	return FALSE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : StopRouter
//	函数功能 : 暂停路由
//	处理过程 : 
//	备    注 : 
//	作    者 : 赵红军
//	时    间 : 2009年11月6日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL StopRouter(void)
{
	TRouter *p = pAppRouter;
	TRouterInfo *pRInfo = pRouterInfo;

	FreshRTSta(RTST_STOP, NULLP, NULLP, NULLP);
	//仅支持集中器主导的抄表模式
	if (pRInfo->byCBMode == 0x01)
	{
		p->byRouterStart = FALSE;
		FreshRTSta(RTST_STOP, 0, NULLP, 1);
		return TRUE;
	}
	else
	{
		if (TxAfn12_F2())
		{
			p->byRouterStart = FALSE;
			FreshRTSta(RTST_STOP, 0, NULLP, 1);
			return TRUE;
		}
	}
	FreshRTSta(RTST_STOP, 0, NULLP, 2);
	return FALSE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : ResumeRouter
//	函数功能 : 恢复路由
//	处理过程 : 
//	备    注 : 恢复抄表
//	作    者 : 赵红军
//	时    间 : 2009年11月6日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL ResumeRouter(void)
{
	TRouter *p = pAppRouter;
	TRouterInfo *pRInfo = pRouterInfo;

	FreshRTSta(RTST_RESUME, NULLP, NULLP, NULLP);
	//仅支持集中器主导的抄表模式
	if (pRInfo->byCBMode == 0x01)
	{
		p->byRouterStart = TRUE;
		FreshRTSta(RTST_RESUME, 0, NULLP, 1);
		return TRUE;
	}
	if (TxAfn12_F3())
	{
		p->byRouterStart = TRUE;
		FreshRTSta(RTST_RESUME, 0, NULLP, 1);
		return TRUE;
	}
	FreshRTSta(RTST_RESUME, 0, NULLP, 2);
	return FALSE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : DealInfoField
//	函数功能 : 处理信息域
//	处理过程 : 
//	备    注 : 解析抄表相位、中继、信号强度等信息
//	作    者 : 赵红军
//	时    间 : 2009年11月16日
//	返 回 值 : BYTE
//	参数说明 : T376_2RxFm* pRxFm
///////////////////////////////////////////////////////////////
BYTE DealInfoField(T376_2RxFm* pRxFm)
{
	TRouter *p = (TRouter *)GethApp();
	BYTE* pAddr = (BYTE*)(pRxFm + 1);

	//信道标识(0-不分信道 1-A相 2-B相 3-C相)
	if (pRxFm->byCtrl&BIT6)
		p->byChanel = (pRxFm->byInfoChannel & 0x0F) % 4;//信道标识(0-不分信道 1-A相 2-B相 3-C相)
	//只处理与电表通信的帧
	if (((pRxFm->byInfoModule >> 2) & 0x01) == 0)
		return 0;
	return 12;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : DT2Fn
//	函数功能 : 将信息类DT1/DT2转换为Fn
//	处理过程 : 
//	备    注 : 
//	作    者 : 赵红军
//	时    间 : 2009年11月7日
//	返 回 值 : WORD
//	参数说明 : BYTE DT1,BYTE DT2
///////////////////////////////////////////////////////////////
WORD DT2Fn(BYTE DT1, BYTE DT2)
{
	BYTE i, byMask;

	for (i = 0; i<8; i++)
	{
		byMask = 1 << i;
		if (DT1&byMask)
			break;
	}
	return (i == 8) ? 0xFF : (DT2 * 8 + i + 1);
}

///////////////////////////////////////////////////////////////
//	函 数 名 : InitMainID
//	函数功能 : 根据终端地址生成主节点地址
//	处理过程 : 
//	备    注 : 主节点地址低8位取终端地址的BCD码 高4位取行政区划码
//	作    者 : 赵红军
//	时    间 : 2010年10月25日
//	返 回 值 : void
//	参数说明 : void
///////////////////////////////////////////////////////////////
void InitMainID(void)
{
	TRouterInfo *pRInfo = GetRouterInfo();
	BYTE *pBuf = pRInfo->szMainNet;
	DWORD dwAddr;
	WORD wCityCode;

	if (IsNullBuf(pRInfo->szMainNet, 6))
	{
		dwAddr = 0x77777777;
		wCityCode = 0x7777;
		pBuf[0] = 0;
		pBuf[1] = 0;
		pBuf[2] = HHBYTE(dwAddr);
		pBuf[3] = HLBYTE(dwAddr);
		pBuf[4] = LHBYTE(dwAddr);
		pBuf[5] = LLBYTE(dwAddr);
	}
}

///////////////////////////////////////////////////////////////
//	函 数 名 : OpenRouter
//	函数功能 : 模块硬复位
//	处理过程 : 
//	备    注 : 断电、上电
//	作    者 : Administrator
//	时    间 : 2012年12月27日
//	返 回 值 : void
//	参数说明 : void
///////////////////////////////////////////////////////////////
void OpenRouter(void)
{
	if (pAppRouter != NULL)
	{//复位后登陆复归
		TGyCfg *pGyCfg = GetGyCfg();
		pGyCfg->wRxDelayMax = MAX_FRAME_DEALY;
		pGyCfg->wCommIdleTime = STAND_COMMUIDLE_TIME;
		pAppRouter->dwLastResetTime = GetRunCount();
		pAppRouter->bLogon = FALSE;
		pAppRouter->byStatus = MST_RTCHK;
		pAppRouter->byInitStep = 0;//jjy
	}
}

///////////////////////////////////////////////////////////////
//	函 数 名 : GetRouterInfo
//	函数功能 : 获取路由检测信息
//	处理过程 :
//	备    注 : 
//	作    者 : 
//	时    间 :
//	返 回 值 : TRouterInfo*
//	参数说明 : void
///////////////////////////////////////////////////////////////
HPARA GetRouterInfo(void)
{
	if (NULL == pRouterInfo)
		return NULL;
	return pRouterInfo;
}

TRouter* GetRouterApp(void)
{
	if (NULL == pAppRouter)
		return NULL;
	return pAppRouter;
}

HPARA GetRouterTask(void)
{
	if (NULL == pTKRouter)
		return NULL;
	return pTKRouter;
}

BYTE *pGetTransBuf(void)
{
	return pTransBuf;
}

TTx* GetRouterTx(void)
{
	return pTxRouter;
}

TRx* GetRouterRx(void)
{
	return pRxRouter;
}

TPort* GetRouterPort(void)
{
	return pPortRouter;
}

void H_TxMonitor(BYTE byLen, BYTE *pBuf, TMPRecord *pTMP)
{
	TRouter *p = (TRouter *)GethApp();
	TRouterInfo *pRInfo = GetRouterInfo();
	TGyCfg *pGyCfg;
	BYTE Fm[256] = { 0 };
	BYTE byTmp;
	WORD wLen = 0,wAddr;
	DWORD dwAddr;

	if ((pTMP->byProtocol != 1) && (pTMP->byProtocol != 2))
		byTmp = 0;
	else
		byTmp = pTMP->byProtocol;
	Fm[wLen++] = byTmp;	//规约
	Fm[wLen++] = 0;		//通信延时相关性标志
	Fm[wLen++] = 0;
	Fm[wLen++] = byLen; //长度
	memcpy(&Fm[wLen], pBuf,byLen);
	wLen += byLen;
	RT698_PortRxClear((TPort *)GethPort());
	if (!IsNullBuf(pTMP->byCollAddr, 6))
	{
		MDByte2DWW(pTMP->byCollAddr, &dwAddr, &wAddr);
		Tx_698Fm1(dwAddr, wAddr, 0, NULLP, AFN13, F1, Fm, (BYTE)wLen);
	}
	else
		Tx_698Fm1(pTMP->dwAddr, pTMP->wAddr, 0, NULLP, AFN13, F1, Fm, (BYTE)wLen);
	p->byBusy = TRUE;
	pGyCfg = GetGyCfg();
	pGyCfg->wRxDelayMax = (pRInfo->wMaxTimeout < 60) ? pRInfo->wMaxTimeout * 1000 : 60000; 
}

BYTE GetInvalidNo(void)
{
	TRouter *p = (TRouter *)GethApp();
	BYTE byI;

	for (byI = 0; byI < MAX_CONCURRENT_NUM;byI++)
	{
		if ((p->Concurrent.ConFmInfo[byI].byValid&BIT0) == 0)
			return byI;
	}
	return 0xFF;
}

void SetConInfo(BYTE byID,BYTE byNum,BYTE *pBuf,DWORD dwAddr,WORD wAddr)
{
	TRouter *p = (TRouter *)GethApp();

	if (byID >= MAX_CONCURRENT_NUM)
		return;
	p->Concurrent.ConFmInfo[byID].byNum = byNum;
	memcpy(p->Concurrent.ConFmInfo[byID].byNo, pBuf, byNum);
	p->Concurrent.ConFmInfo[byID].byValid = 1;
	p->Concurrent.ConFmInfo[byID].dwAddr = dwAddr;
	p->Concurrent.ConFmInfo[byID].wAddr = wAddr;
	p->Concurrent.ConFmInfo[byID].dwTime = GetSecs();
}

void ClearConInfo(BYTE byID)
{
	TRouter *p = (TRouter *)GethApp();

	if (byID >= MAX_CONCURRENT_NUM)
		return;
	memset(&p->Concurrent.ConFmInfo[byID], 0, sizeof(TConFmInfo));
	Trace("ClearConInfo");
}

BYTE GetConNum(DWORD dwAddr, WORD wAddr)
{
	BYTE byI;
	TRouter *p = (TRouter *)GethApp();

	for (byI = 0; byI < MAX_CONCURRENT_NUM;byI++)
	{
		if (p->Concurrent.ConFmInfo[byI].byValid == 0)
			continue;
		if ((p->Concurrent.ConFmInfo[byI].dwAddr == dwAddr)
			&& (p->Concurrent.ConFmInfo[byI].wAddr == wAddr))
		{
			return byI;
		}
	}
	return 0xFF;
}

void DealAfnF1_F1(BYTE *pBuf)
{
	TRouter *p = (TRouter *)GethApp();
	T376_2RxFm *pRxFm = NULL;
	BYTE *pData = NULL;
	WORD wDataLen,wAddrEx;
	DWORD dwAddr;
	BYTE byNo,byI;

	pRxFm = (T376_2RxFm *)GetRxFm();
	CHECKPTR0(pRxFm);
	pData = (BYTE *)(pRxFm + 1);
	dwAddr = MDW(pData[3], pData[2], pData[1], pData[0]);
	wAddrEx = MW(pData[5], pData[4]);
	byNo = GetConNum(dwAddr, wAddrEx);
	if (0xFF == byNo)
	{
		Trace("Concurrent addr error");
		return;
	}
	pBuf++;
	wDataLen = MW(pBuf[1], pBuf[0]);											//报文长度
	pBuf += 2;
	if (wDataLen)
	{
		SendFm(PORT_ZB, pBuf, wDataLen, 0xF1F1, p->Concurrent.ConFmInfo[byNo].byNo[0],FALSE);
		for (byI = 0; byI < p->Concurrent.ConFmInfo[byNo].byNum; byI++)
		{
			Del_ProxyFm(PORT_ZB, 0, p->Concurrent.ConFmInfo[byNo].byNo[byI]);
		}
		ClearConInfo(byNo);
	}
}

void HC_TxMonitor(BYTE byNo)
{
	TRouter *p = (TRouter *)GethApp();
	TRouter *pApp = (TRouter *)GethApp();
	TFmBuf *pFmBuf;
	BYTE *pBuf;
	TAFNF1_F1 *pHead = NULL;
	TMPRecord *pMP;
	WORD wLen, wFmLen;
	BYTE byTxNo[MAX_SINGCON_NUM],byNum,byI,byID;


	byNum = GetNoNum(byNo,&byTxNo,MAX_SINGCON_NUM );
	if (byNum == 0)
	{
		Del_ProxyFm(PORT_ZB, 0, byNo);
		return;
	}
	pFmBuf = pGetPriorityNo(PORT_ZB, 0, byNo);
	CHECKPTR0(pFmBuf);
	pMP = pGetMP(pFmBuf->wMPNo);
	byID = GetInvalidNo();
	if (0xFF != byID)
	{//有空闲缓冲区
		wLen = 0;
		pHead = (TAFNF1_F1 *)p->Concurrent.buf;
		pHead->byRule = pMP->byProtocol;
		pHead->byBak = 0;
		pBuf = (BYTE *)(pHead + 1);
		for (byI = 0; byI < byNum;byI++)
		{
			pFmBuf = pGetPriorityNo(PORT_ZB, 0, byTxNo[byI]);
			if ((wLen + pFmBuf->wLen) < MAX_SINGBUF_SIZE)
			{
				memcpy(&pBuf[wLen],pFmBuf->buf,pFmBuf->wLen);
				wLen += pFmBuf->wLen;
				SetLastProxyNo(PORT_ZB, 0, byTxNo[byI]);
			}
			else
				break;
		}
		pHead->wLen = wLen;										//报文长度
		wFmLen = wLen + sizeof(TAFNF1_F1);							//数据长度
		RT698_PortRxClear((TPort *)GethPort());
		Tx_698Fm1(pMP->dwAddr, pMP->wAddr, 0, NULL, AFNF1, F1, p->Concurrent.buf, (BYTE)wFmLen);
		//置标志
		SetLastProxyNo(PORT_ZB, 0, byNo);
		SetConInfo(byID, byI, byTxNo, pMP->dwAddr, pMP->wAddr);
	}
}

void Proxy_TxMonitor(void)
{
	TRouter *p = (TRouter *)GethApp();
	TRouterInfo *pRInfo = GetRouterInfo();
	TFmBuf *pFmBuf;
	TMPRecord *pMP;
	BYTE byNo;

	if (p->byRouterStart)
		StopRouter(); 
	//高优先级
	byNo = GetProxyNo(PORT_ZB, 1);
	if (0xFF != byNo)
	{
		Trace("优先级发送序号：%d",byNo);
		pFmBuf = pGetPriorityNo(PORT_ZB, 1, byNo);
		if (NULLP == pFmBuf)
			return;
		pMP = pGetMP(pFmBuf->wMPNo);
		if (NULLP == pMP)
			return;
		H_TxMonitor(pFmBuf->wLen,pFmBuf->buf,pMP);
		SetLastProxyNo(PORT_ZB, 1, byNo);
	}
	byNo = GetProxyNo(PORT_ZB, 0);
	if (0xFF != byNo)
	{
		HC_TxMonitor(byNo);
		SetLastProxyNo(PORT_ZB, 0, byNo);
	}
}

//////////////////////////////////////////////////////////////////////
//	函 数 名 : TC_QueryDI
//	函数功能 : 处理“路由请求抄读内容”
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 2011年4月26日
//	返 回 值 : void
//	参数说明 : DWORD dwAddr,WORD wAddrEx --载波从节点
//////////////////////////////////////////////////////////////////////
void TC_QueryDI(DWORD dwAddr, WORD wAddrEx)
{
	TRouter *pRouter = (TRouter *)GethApp();

	CHECKPTR0(pRouter);
	FreshRTSta(RTST_WORK, pAppRouter->byChanel, 1, dwAddr);
	TxAfn14_F1(dwAddr, wAddrEx, MOP_SUCC, NULL, 0);
	FreshRTSta(RTST_WORK, pRouter->byChanel, 4, dwAddr);
	return;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : FreshRTSta
//	函数功能 : 刷新路由模块当前工作状态
//	处理过程 : 
//	备    注 : 用于路由监视，便于现场诊断
//	作    者 : 
//	时    间 : 2014年8月25日
//	返 回 值 : void
//	参数说明 : BYTE byRTSta,BYTE byCh,BYTE byChSta,DWORD dwPara
///////////////////////////////////////////////////////////////
void FreshRTSta(BYTE byRTSta, BYTE byCh, BYTE byChSta, DWORD dwPara)
{
	TRouterInfo *p = pRouterInfo;
	RTRunSta *pRS = (RTRunSta *)&p->tRTRunSta;

	pRS->byRTSta = byRTSta;
	if (byCh < 4)
	{
		pRS->CHSta[byCh].byChSta = byChSta;
		pRS->CHSta[byCh].dwChPara = dwPara;
	}
}


BOOL IsVirtualMP(DWORD dwAddr, WORD wAddrEx)
{
	return ((dwAddr == 0xEEEEEEEE) && (wAddrEx == 0xEEEE));
}

///////////////////////////////////////////////////////////////
//	函 数 名 : DealAfn06_F5
//	函数功能 : 处理路由主动上报的电能表事件
//	处理过程 : 
//	备    注 : 
//	作    者 : 赵红军
//	时    间 : 2014年11月24日
//	返 回 值 : void
//	参数说明 : BYTE *pBuf
///////////////////////////////////////////////////////////////
void DealAfn06_F5(BYTE *pBuf)
{
	BYTE byDevType, byProtocol, byRxLen;
	BYTE byLen = 0;

	byDevType = pBuf[byLen++]; //从节点设备类型
	byProtocol = pBuf[byLen++]; //通信协议类型
	byRxLen = pBuf[byLen++]; //报文长度
	SendFm(PORT_ZB,&pBuf[byLen],byRxLen, 0x06f5,0,TRUE);
#if 0
	//电能表事件主动上报
	if (!ChkAndRec645Fm(byRxLen, &pBuf[byLen]))
	{
		Trace("路由上报电能表事件报文格式错误!");
	}
#endif
}

#endif/*#if(MD_376_2)*/

