#include "wsd_Flat.h"
#include "wsd_cgy.h"
///////////////////////////////////////////////////////////////
//	函 数 名 : CheckGyCfg
//	函数功能 : 检查规约配置表是否有效
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 
//	返 回 值 : BOOL 有效返回TRUE
//	参数说明 : TGy *p 规约配置表地址
///////////////////////////////////////////////////////////////
static BOOL CheckGyCfg(TGy *p)
{
	TGyCfg *pc = &p->GyCfg;
	TPort *pPort = (TPort*)GethPort();

	switch (pc->wGyMode)
	{
	case MGYM_MASTER:			   //问答式(主)
	case MGYM_SLAVE:			   //问答式(从)
	case MGYM_CALC:				  //算法类
		if (pc->wMinDevNum <1)
			return FALSE;

		p->byMode = (BYTE)pc->wGyMode;
		break;
	case MGYM_SYS_MASTER:			  //系统类（从属于MGYM_MASTER，但无设备）
		if (pc->wMinDevNum != 0)
			return FALSE;
		break;
	default:
		return FALSE;
	}
	if (pc->wTxdBufSize == 0)
		return TRUE;
	if (pc->wRxMiniChars == 0)
		pc->wRxMiniChars = pc->wRxdBufSize / 2;
	if (pc->wGyMode == MGYM_SYS_MASTER)
		pc->wGyMode = MGYM_MASTER;
	if (pc->wRxDelayMax < 300)
		pc->wRxDelayMax = 300;

	//字符间隔  6201需要100ms， 1200BPS时，Modem延迟需250ms
	if (pc->wCommIdleTime  < 400)
		pc->wCommIdleTime = 400;

	if (pc->wGyMode == MGYM_MASTER)
	{//发送间隔
		if (pc->wTxdIdleTime  < 20)
			pc->wTxdIdleTime = 20;
	}
	if (pPort->Cfg.wCharIdle)
		pc->wCommIdleTime = pPort->Cfg.wCharIdle;
	if (pPort->Cfg.wRxDelay)
		pc->wRxDelayMax = pPort->Cfg.wRxDelay;

	if (pc->wGyMode == MGYM_MASTER)
	{
		if (pPort->Cfg.wTxIdle)
			pc->wTxdIdleTime = pPort->Cfg.wTxIdle;
	}
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : OnTimeOut
//	函数功能 : 规约基类的定时处理函数
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 
//	返 回 值 : void
//	参数说明 : HPARA hGy,  规约基类句柄
//				DWORD dwTimeID 定时器ID
///////////////////////////////////////////////////////////////
static void OnTimeOut(DWORD dwTimeID)
{
TGy *p = (TGy *)GethGy();

	if (p->fOnTimeOut)
		p->fOnTimeOut(dwTimeID);
}

///////////////////////////////////////////////////////////////
//	函 数 名 : SearchOneFrame
//	函数功能 : 搜索一个完整的报文桩函数
//	处理过程 : 
//	备    注 : 这是一个桩函数
//	作    者 : 
//	时    间 : 
//	返 回 值 : DWORD
//	参数说明 : HPARA hGy,规约基类名柄
//				HPARA hBuf,报文缓冲区
//				DWORD dwLen 缓冲区的长度
///////////////////////////////////////////////////////////////
static DWORD SearchOneFrame(HPARA hBuf, DWORD dwLen)
{
	return FM_ERR | dwLen;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : RxMonitor
//	函数功能 : 接收处理的桩函数
//	处理过程 : 
//	备    注 : 这是一个桩函数
//	作    者 : 
//	时    间 : 
//	返 回 值 : void
//	参数说明 : HPARA hApp 应用句柄
///////////////////////////////////////////////////////////////
static void  RxMonitor(void)
{
}

///////////////////////////////////////////////////////////////
//	函 数 名 : RxMonitor
//	函数功能 : 发送处理的桩函数
//	处理过程 : 
//	备    注 : 这是一个桩函数
//	作    者 : 
//	时    间 : 
//	返 回 值 : void
//	参数说明 : HPARA hApp 应用句柄
///////////////////////////////////////////////////////////////
static void  TxMonitor(void)
{
}

///////////////////////////////////////////////////////////////
//	函 数 名 : AppCreate
//	函数功能 : 基于规约平台的标准任务创建过程
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 
//	返 回 值 : BOOL
//	参数说明 : DWORD dwAppMemSize,
//				DWORD dwMemID
///////////////////////////////////////////////////////////////
BOOL AppCreate(DWORD dwAppMemSize, DWORD dwMemID)
{
HPARA hApp;

	SethApp(0);
	if (GY_Create() != TRUE)
		return FALSE;
	if (dwAppMemSize)
	{
		hApp = MemAlloc(dwAppMemSize);
		if (NULLP == hApp)
		{
			GY_Delete();
			return FALSE;
		}
	}
	else
		hApp = NULLP;
	SethApp(hApp);
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : AppDelete
//	函数功能 : 基于规约平台的标准任务删除过程
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 
//	返 回 值 : void
//	参数说明 : void
///////////////////////////////////////////////////////////////
void AppDelete(void)
{
HPARA hApp = GethApp();

	GY_Delete();
	if (hApp)
		MemFree(hApp);
	OS_Quit();
}

///////////////////////////////////////////////////////////////
//	函 数 名 : GY_Create
//	函数功能 : 创建规约基类
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 
//	返 回 值 : BOOL 创建成功反回TRUE
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL GY_Create(void)
{//创建规约基类
TGy *p = (TGy *)MemAlloc(sizeof(TGy));

	if (NULLP == p)
		return FALSE;
	SethGy(p);
	p->fRxMonitor = RxMonitor;
	p->fSearchOneFrame = SearchOneFrame;
	p->fTxMonitor = TxMonitor;

	memset(&p->GyCfg, 0, sizeof(TGyCfg));
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : GY_Delete
//	函数功能 : 删除规约基类
//	处理过程 : 释放基类已申请的所有内存
//	备    注 : 
//	作    者 : 
//	时    间 : 
//	返 回 值 : void
//	参数说明 : void
///////////////////////////////////////////////////////////////
void GY_Delete(void)
{
TGy *p = (TGy *)GethGy();

	if (NULLP == p)
		return;
	if (p->Rx.pBuf)
		MemFree(p->Rx.pBuf);
	if (p->Tx.pBuf)
		MemFree(p->Tx.pBuf);
	MemFree(p);
}

///////////////////////////////////////////////////////////////
//	函 数 名 : GY_Init
//	函数功能 : 规约基类初始化
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 
//	返 回 值 : BOOL 成功返加TRUE
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL GY_Init(void)
{//规约初始化
TGy *p = (TGy *)GethGy();
TPort *pPort = (TPort *)GethPort();

	if (CheckGyCfg(p) != TRUE)
		return FALSE;
	//初始化端口
	if (GY_InitPort(p) != TRUE)
	{
		Trace("端口错误");
		PT_DeletePort(pPort->Cfg.dwID);
		return FALSE;
	}
	//加载定时
	OS_OnCommand(MCMD_ONTIMEOUT, (HPARA)OnTimeOut);
	p->RetryCnt = 100;
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : GY_InitPort
//	函数功能 : 规约基类的端口初始化
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 
//	返 回 值 : BOOL 成功返回TRUE
//	参数说明 : HPARA hGy 规约基类句柄
///////////////////////////////////////////////////////////////
BOOL GY_InitPort(HPARA hGy)
{
TGy *p = (TGy *)GethGy();
TPort *pPort = (TPort *)GethPort();
BYTE byPortNo = HLBYTE(pPort->Cfg.dwID);

	if (p->GyCfg.wRxdBufSize == 0 && p->GyCfg.wTxdBufSize == 0)
		return TRUE;
	if (NULLP == pPort)
		return FALSE;
	if (p->GyCfg.wRxdBufSize)
	{
		p->Rx.pBuf = (BYTE *)MemAlloc(p->GyCfg.wRxdBufSize);
		if (NULLP == p->Rx.pBuf)
			return FALSE;
	}
	if (p->GyCfg.wTxdBufSize)
	{
		p->Tx.pBuf = (BYTE *)MemAlloc(p->GyCfg.wTxdBufSize);
		if (NULLP == p->Tx.pBuf)
			return FALSE;
	}
	//打开端口
	if (PT_Open(pPort, hGy) != TRUE)
	{
		if (p->GyCfg.wRxdBufSize)
		{
			MemFree(p->Rx.pBuf);
			p->Rx.pBuf = NULLP;
		}
		if (p->GyCfg.wTxdBufSize)
		{
			MemFree(p->Tx.pBuf);
			p->Tx.pBuf = NULLP;
		}
		return FALSE;
	}
	p->Tx.dwSize = p->GyCfg.wTxdBufSize;
	p->Rx.dwSize = p->GyCfg.wRxdBufSize;
	//加载驱动程序
	if (OS_SetTimer(MTID_DRIVER, 20) != TRUE)//设置定时器
		return FALSE;

	OS_OnCommand(MCMD_ONDRIVER, (HPARA)OnDriver);
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : Register
//	函数功能 : 规约基类派生的各种函数的注册功能
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 
//	返 回 值 : void
//	参数说明 : DWORD dwFUNID,规约基类函数ID
//				HPARA hFun,  派生的函数句柄
//				HPARA hPara  函数用到的参数
///////////////////////////////////////////////////////////////
void Register(DWORD dwFUNID, HPARA hFun)
{
	TGy *p = (TGy *)GethGy();

	if (dwFUNID >= MAX_GY_CALLBACKFUNS)
		return;
	switch (dwFUNID)
	{
	case MGYF_SEARCHONFRAME:
		p->fSearchOneFrame = (FD_HD)hFun;
		break;
	case MGYF_RXMONITOR:
		p->fRxMonitor = (FV_V)hFun;
		break;
	case MGYF_TXMONITOR:
		p->fTxMonitor = (FV_V)hFun;
		break;
	case MGYF_ONTIMEOUT:
		p->fOnTimeOut = (FV_D)hFun;
		break;
	}
}

///////////////////////////////////////////////////////////////
//	函 数 名 : GY_SetTimer
//	函数功能 : 规约基类的设置定时器
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 
//	返 回 值 : BOOL
//	参数说明 : DWORD dwTimerID,定时器ID
//				DWORD dwMSTime，定时间隔，单位MS
///////////////////////////////////////////////////////////////
BOOL GY_SetTimer(DWORD dwTimerID, DWORD dwMSTime)
{
	return OS_SetTimer(dwTimerID, dwMSTime);
}

///////////////////////////////////////////////////////////////
//	函 数 名 : GetGyCfg
//	函数功能 : 获取规约基类的规约配置表指针
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 
//	返 回 值 : TGyCfg* 获取的规约基类的配置表的指针
//	参数说明 : void
///////////////////////////////////////////////////////////////
TGyCfg* GetGyCfg(void)
{
	TGy *p = (TGy *)GethGy();
	return &p->GyCfg;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : GetDriveID
//	函数功能 : 获取规约号
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 
//	返 回 值 : WORD
//	参数说明 : void
///////////////////////////////////////////////////////////////
WORD GetDriveID(void)
{
	TPort *pPort = (TPort *)GethPort();
	return pPort->Cfg.wDriverID;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : DisableUnLock
//	函数功能 : 禁止自动释放端口控制权
//	处理过程 : 
//	备    注 : 用于一问多答类的规约用于一口多规约的形况下。
//	作    者 : 
//	时    间 : 
//	返 回 值 : void
//	参数说明 : void
///////////////////////////////////////////////////////////////
void DisableUnLock(BOOL bMan)//禁止释放控制权
{
	TGy *p = (TGy *)GethGy();
	p->DisableUnLock = TRUE;
	if (bMan)
		p->DisableUnLock |= DU_MAN_MASK;
}
void EnableUnLock(void)//手动释放控制权
{
	TGy *p = (TGy *)GethGy();
	p->DisableUnLock = FALSE;
}
void DisRetry(void)
{
	TGy *p = (TGy *)GethGy();

	p->RetryCnt = 100;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : GetTx
//	函数功能 : 获取当前规约的发送缓冲区控制指针
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 
//	返 回 值 : TTx* 当前规约的发送缓冲区控制指针
//	参数说明 : void
///////////////////////////////////////////////////////////////
TTx* GetTx(void)
{
	TGy *p = (TGy *)GethGy();
	return &p->Tx;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : GetRx
//	函数功能 : 获取当前规约的接收缓冲区控制指针
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 
//	返 回 值 : TRx* 当前规约的接收缓冲区控制指针
//	参数说明 : void
///////////////////////////////////////////////////////////////
TRx *GetRx(void)
{
	TGy *p = (TGy *)GethGy();
	return &p->Rx;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : GetRxFm
//	函数功能 : 获取接收报文缓冲区首址
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 
//	返 回 值 : HPARA
//	参数说明 : void
///////////////////////////////////////////////////////////////
HPARA GetRxFm(void)
{
	TGy *p = (TGy *)GethGy();
	TRx *pRx = &p->Rx;

	return &pRx->pBuf[pRx->dwRptr];
}

void Socket_Close(HPARA hPort)
{
	TGy *p = (TGy *)GethGy();

	PT_Close(hPort, p);
	AppDelete();
}
