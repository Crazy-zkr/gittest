#include "wsd_1376_2.h"
#include "wsd_cgy.h"
#include "wsd_1376_2_fun.h"
#if(MD_DEV_PLC)
extern TGy *pGyRouter;

///////////////////////////////////////////////////////////////
//	函 数 名 : IsFirstOfCll
//	函数功能 : 判断该测量点是否为所在采集器下的第一个
//	处理过程 : 
//	备    注 : 
//	作    者 : 赵红军
//	时    间 : 2015年4月7日
//	返 回 值 : BOOL
//	参数说明 : TMP *pMP
///////////////////////////////////////////////////////////////
BOOL IsFirstOfCll(TMPRecord *pMP)
{
	WORD wNo, wMPNum = GetPlcNum();
	TMPRecord *p;

	for (wNo = 0; wNo < wMPNum; wNo++)
	{
		p = pGetMP(wNo);
		if (!IsValidMeter(p))
			continue;
		if (IsVirtualMP(p->dwAddr, p->wAddr))
			continue;
		if (IsNullBuf(p->byCollAddr, 6))
			continue;
		if (0 == memcmp(p->byCollAddr, pMP->byCollAddr, 6))
		{
			return (p == pMP);
		}
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : AddIDPara
//	函数功能 : 向路由添加从节点信息
//	处理过程 : 
//	备    注 : 
//	作    者 : 赵红军
//	时    间 : 2015年4月2日
//	返 回 值 : WORD 成功添加的节点数量
//	参数说明 : BOOL bOnlyNew： 1-只添加新增节点 0-添加所有节点
///////////////////////////////////////////////////////////////
WORD AddIDPara(BOOL bOnlyNew)
{
	TRouter *p = GetRouterApp();
	TRT_ID *pRTIDPtr = NULL;
	TMPRecord *pMP = NULL;
	TID_Array RTID;
	WORD wNo;
	WORD wMPNum = GetPlcNum();
	WORD wOkNum = 0;
	BYTE byRule;
	BYTE byCnt = 0;
	BYTE byBuf[6];

	//清除所有从节点
	if (bOnlyNew == 0)
		ClearRouterID698();
	//缓冲区准备
	pRTIDPtr = &RTID.tID[0];
	//下装所有从节点
	for (wNo = 0; wNo<MAX_MP_NUM; wNo++)
	{
		pMP = pGetMP(wNo);
		if (!IsValidMeter(pMP))
			continue;
		if (IsVirtualMP(pMP->dwAddr, pMP->wAddr))
			continue;
		if (bOnlyNew)
		{//只添加新增节点
			if (pMP->byFlag&BIT0)
				continue;
		}
		byRule = (pMP->byProtocol == MGY_645_2007) ? 2 : 1;
		if (!IsNullBuf(pMP->byCollAddr, 6))
		{//有采集器
			if (!IsFirstOfCll(pMP))
				continue;
			memcpy(pRTIDPtr->byAddr, pMP->byCollAddr, 6);
			pRTIDPtr->byRule = byRule;
			pRTIDPtr++;
			byCnt++;
			wOkNum++;
			Trace("同步采集器地址;%02x%02x%02x%02x%02x%02x", RTID.tID->byAddr[0], RTID.tID->byAddr[1], RTID.tID->byAddr[2], RTID.tID->byAddr[3], RTID.tID->byAddr[4], RTID.tID->byAddr[5]);
		}
		else
		{//载波表
			MDDWW2Byte(pMP->dwAddr, pMP->wAddr, byBuf);
			memcpy(pRTIDPtr->byAddr, byBuf, 6);
			pRTIDPtr->byRule = byRule;
			pRTIDPtr++;
			byCnt++;
			wOkNum++;
			Trace("同步设备地址;%02x%02x%02x%02x%02x%02x", RTID.tID->byAddr[0], RTID.tID->byAddr[1], RTID.tID->byAddr[2], RTID.tID->byAddr[3], RTID.tID->byAddr[4], RTID.tID->byAddr[5]);
		}
		if (byCnt >= 10)
		{
			RTID.byNum = byCnt;
			TxAfn11_F1((HPARA)&RTID);
			byCnt = 0;
			pRTIDPtr = &RTID.tID[0];
		}
	}
	if (byCnt != 0)
	{
		RTID.byNum = byCnt;
		TxAfn11_F1((HPARA)&RTID);
	}
	return wOkNum;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : CheckIDPara
//	函数功能 : 检查从节点参数
//	处理过程 : 删除多余节点补充缺失节点
//	备    注 : 2013.9改为清除->同步模式
//	备    注 : 2015.4改为清除->添加删除模式
//			   2015.7.22 调整程序处理逻辑为：首先执行10-F1 10-F2查询，然后检查节点信息，再处理待删除和
//			   待添加节点，如无需处理，则退出该函数
//	作    者 : 赵红军
//	时    间 : 2009年11月6日
//	返 回 值 : BOOL
//	参数说明 : void 
///////////////////////////////////////////////////////////////
BOOL CheckIDPara(void)
{
	TRouter *p = GetRouterApp();
	TGy *pGy = (TGy *)GethGy();
	TRouterInfo *pRInfo = GetRouterInfo();
	TMPRecord *pMP = NULL;
	BYTE byI;
	BYTE byRecNum;
	BYTE byAddr[60];
	BYTE byReadNum;
	BYTE byRetry = 0;
	BYTE byFrams;
	BYTE byIndex;
	WORD wAddrEx;
	WORD wMPNum = 0;
	WORD wIDNum = StaIDNum();
	WORD wStartSN;
	WORD wNo;
	DWORD dwAddr;
	BOOL bChanged = FALSE;

	CHECKPTR(p, FALSE);
	CHECKPTR(pRInfo, FALSE);
	CHECKPTR(pGy, FALSE);

	wMPNum = GetPlcNum();

	//不需要节点档案的模块(瑞斯康)
	if ((pRInfo->bySubMode == 0) && (pRInfo->byCBMode == 1))
		return TRUE;

	//暂停路由
	if (p->byRouterStart)
		StopRouter();

	if (wIDNum == 0)//集中器档案为空,清空路由
		return ClearRouterID698();

	//节点检查及添加删除(最多3次)
	for (byRetry = 0; byRetry<MAX_CHECKIDTIMES; byRetry++)
	{
		//默认档案无变动
		bChanged = FALSE;

		//清除所有已同步标志
		for (wNo = 0; wNo<MAX_MP_NUM; wNo++)
		{
			pMP = pGetMP(wNo);
			if (NULLP != pMP)
				pMP->byFlag &= (~BIT0);
		}

		//清空待删除地址列表
		p->wDelNum = 0;
		memset(&p->DelAddr[0][0], 0, sizeof(p->DelAddr));

		//获取节点数量
		if (!CheckIDNum())
			return FALSE;

		//1、先判断路由节点个数
		if (pRInfo->wIDNum != 0)
		{
			//集中器档案数量与路由档案数量不一致
			if (pRInfo->wIDNum != wIDNum)
				bChanged = TRUE;

			//2、检查路由从节点信息
			byFrams = (BYTE)((pRInfo->wIDNum + MAX_NODENUM_FM - 1) / MAX_NODENUM_FM);
			for (byIndex = 0; byIndex<byFrams; byIndex++)
			{
				wStartSN = (WORD)(byIndex*MAX_NODENUM_FM + 1);
				if (byIndex == (byFrams - 1))
					byReadNum = (BYTE)(pRInfo->wIDNum - MAX_NODENUM_FM*byIndex);
				else
					byReadNum = MAX_NODENUM_FM;

				//每包读10个节点
				if (!TxAfn10_F2(wStartSN, byReadNum, byAddr, &byRecNum))
				{
					return FALSE;
				}

				//3、检验路由上报的节点地址
				for (byI = 0; byI<byRecNum; byI++)
				{
					MDByte2DWW(&byAddr[byI * 6], &dwAddr, &wAddrEx);

					if (pMP = pGetMPEx(dwAddr, wAddrEx))
					{//该地址在档案中存在
						pMP->byFlag |= BIT0;
					}
					else
					{//该地址为非法地址，集中器档案中不存在
						if (p->wDelNum < MAX_DEL_NUM)
						{//加入待删除地址列表
							memcpy(&p->DelAddr[p->wDelNum][0], &byAddr[byI * 6], 6);
							p->wDelNum++;
						}
						else
							break;
					}
				}
				if (p->wDelNum >= MAX_DEL_NUM)
					break;
			}
			//删除非法节点
			if (p->wDelNum != 0)
				bChanged = TRUE;
		}
		else
			bChanged = TRUE;

		//4、处理变更测量点
		if (bChanged == TRUE)
		{
			if (p->wDelNum != 0)
			{
				//待删除数量和路由数量一致，则初始化路由，然后重新添加			
				if ((p->wDelNum == pRInfo->wIDNum) || (p->wDelNum >= MAX_DEL_NUM))
					AddIDPara(0);
				else
				{
					for (wNo = 0; wNo<p->wDelNum; wNo++)
					{
						MDByte2DWW(&p->DelAddr[wNo][0], &dwAddr, &wAddrEx);
						if (!TxAfn11_F2(dwAddr, wAddrEx))//删除操作失败转为清空添加模式
							AddIDPara(0);
					}
					AddIDPara(1);
				}
				p->wDelNum = 0;
			}
			else
				AddIDPara(1);
		}
		else
		{//5、处理完毕，重新抄表
			break;
		}
	}
	//三次添加删除未能实现同步
	if (byRetry >= MAX_CHECKIDTIMES)
	{
		AddIDPara(0);
	}
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : StaIDNum
//	函数功能 : 根据集中器档案信息统计路由节点数量
//	处理过程 : 
//	备    注 : 用于校准路由模块节点信息
//	作    者 : Administrator
//	时    间 : 2011年8月24日
//	返 回 值 : WORD
//	参数说明 : void
///////////////////////////////////////////////////////////////
WORD StaIDNum(void)
{
	WORD wNo, wNo2, wMPNum = GetPlcNum(), wCnt = 0;
	TMPRecord *pMP, *pMP2;

	for (wNo = 0; wNo < wMPNum; wNo++)
	{
		pMP = pGetMP(wNo);
		if (!IsValidMeter(pMP))
			continue;
		if (IsNullBuf(pMP->byCollAddr, 6))
		{
			wCnt++;
			continue;
		}
		for (wNo2 = 0; wNo2 < wNo; wNo2++)
		{
			pMP2 = pGetMP(wNo2);
			if (!IsValidMeter(pMP2))
				continue;
			if (memcmp(pMP->byCollAddr, pMP2->byCollAddr, 6) == 0)
			{
				break;
			}
		}
		if (wNo2 == wNo)
		{
			if (FALSE == IsVirtualMP(pMP->dwAddr,pMP->wAddr))
				wCnt++;
		}
	}
		return wCnt;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : CheckVerInfo
//	函数功能 : 查询路由版本信息
//	处理过程 : 
//	备    注 : 
//	作    者 : 赵红军
//	时    间 : 2009年11月6日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL CheckVerInfo(void)
{
	return TxAfn03_F10();
}

///////////////////////////////////////////////////////////////
//	函 数 名 : CheckMainID
//	函数功能 : 检查主节点地址
//	处理过程 : 
//	备    注 : 与集中器地址不一致时强行设置
//	作    者 : 赵红军
//	时    间 : 2009年11月6日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL CheckMainID(void)
{
	return TxAfn03_F4();
}

///////////////////////////////////////////////////////////////
//	函 数 名 : CheckIDNum
//	函数功能 : 查询载波节点数量
//	处理过程 : 
//	备    注 : 
//	作    者 : 赵红军
//	时    间 : 2009年11月6日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL CheckIDNum(void)
{
	TRouter *p = (TRouter *)GethApp();
	BOOL bRc;

	FreshRTSta(RTST_CHKIDNUM, NULLP, NULLP, NULLP);
	bRc = TxAfn10_F1();
	if (bRc)
		FreshRTSta(RTST_CHKIDNUM, 0, NULLP, 1);
	else
		FreshRTSta(RTST_CHKIDNUM, 0, NULLP, 2);

	return bRc;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : ChkStudySta
//	函数功能 : 查询路由运行状态
//	处理过程 : 
//	备    注 : 用于判断学习（组网）是否完成
//	作    者 : Administrator
//	时    间 : 2012年12月26日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL ChkStudySta(void)
{
	TRouter *p = (TRouter *)GethApp();
	TRouterInfo *pRInfo = GetRouterInfo();
	WORD wIDNum = StaIDNum();
	BOOL bRc;

	bRc = TxAfn10_F4();
	//节点数量检验
	if (bRc && (pRInfo->wIDNum != wIDNum))
	{
		if (pRInfo->bySubMode == 1)
		{
			FreshIDPara(1);
		}
	}
	return bRc;
}


BOOL SetRTPara(void)
{
	TRouter *p = (TRouter *)GethApp();
	TRouterInfo *pRInfo = GetRouterInfo();

	//微功率无线需设置通道参数
	if ((pRInfo->byCommMode == MICROPOWER_WIRELESS) || (pRInfo->byCommMode == PLC_WIDEBAND))
	{
		p->bStuFirst = TRUE; //先组网后抄表 zhj 2014.9.15
		return TRUE; //TxAfn05_F5(254,255);	//设置信道号
	}
	return TRUE;
}

BOOL IsValidMeter(TMPRecord *pMP)
{
	if (pMP == NULLP)
		return FALSE;
	if ((pMP->byValid&BIT0) == 0)
		return FALSE;
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : IsWorkTime
//	函数功能 : 
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 2011年4月25日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL IsWorkTime(void)
{
	return TRUE;
}

void MDByte2DWW(BYTE* pBuf, DWORD* pdwAddr, WORD* pwAddrEx)
{
	*pdwAddr = MDW(pBuf[3], pBuf[2], pBuf[1], pBuf[0]);
	*pwAddrEx = MW(pBuf[5], pBuf[4]);

}

void MDDWW2Byte(DWORD dwAddr, WORD wAddrEx, BYTE* pBuf)
{
	pBuf[5] = HIBYTE(wAddrEx);
	pBuf[4] = LOBYTE(wAddrEx);
	pBuf[3] = HHBYTE(dwAddr);
	pBuf[2] = HLBYTE(dwAddr);
	pBuf[1] = LHBYTE(dwAddr);
	pBuf[0] = LLBYTE(dwAddr);
}

BOOL IsNullBuf(BYTE *pBuf, DWORD dwLen)
{
	DWORD dwNo;

	for (dwNo = 0; dwNo<dwLen; dwNo++)
	{
		if (pBuf[dwNo] != 0)
			return FALSE;
	}
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : ReverseCpy
//	函数功能 : 将输入缓冲区内的字节反序到输出缓冲区
//	备    注 : 输入与输出不能为同一段内存
//	作    者 : 赵红军
//	时    间 : 2009年11月6日
//	返 回 值 : void
//	参数说明 : BYTE* outBuf,BYTE* inBuf,BYTE byLen
///////////////////////////////////////////////////////////////
void ReverseCpy(BYTE* outBuf, BYTE* inBuf, BYTE byLen)
{
	BYTE i;

	for (i = 0; i<byLen; i++)
	{
		outBuf[i] = inBuf[byLen - 1 - i];
	}
}

///////////////////////////////////////////////////////////////
//	函 数 名 : *pGetMPEx
//	函数功能 : 根据地址获取测量点
//	处理过程 : 
//	备    注 : 注意：该函数只在载波端口下用，以保证地址不重复!
//	作    者 : Administrator
//	时    间 : 2011年6月29日
//	返 回 值 : TMP
//	参数说明 : DWORD dwAddr,WORD wAddrEx
///////////////////////////////////////////////////////////////
TMPRecord *pGetMPEx(DWORD dwAddr, WORD wAddrEx)
{
	TGy *pGy = pGyRouter;
	WORD wNo, wMPNum =GetPlcNum();
	TMPRecord *pMP;

	for (wNo = 0; wNo<wMPNum; wNo++)
	{
		pMP = pGetMP(wNo);
		if (NULLP == pMP)
			continue;
		if ((dwAddr == pMP->dwAddr) && (wAddrEx == pMP->wAddr))
		{
			return pMP;
		}
	}
	return NULLP;
}


///////////////////////////////////////////////////////////////
//	函 数 名 : RT698_PortRxClear
//	函数功能 : 清除路由端口接收缓冲区
//	处理过程 :
//	备    注 : 
//	作    者 : 
//	时    间 :
//	返 回 值 : void
//	参数说明 : TPort *pPort
///////////////////////////////////////////////////////////////
void RT698_PortRxClear(TPort *pPort)
{
	TPort *pRTPort = GetRouterPort();
	TRx *pRx = GetRouterRx();
	DWORD dwDelay = 0, dwTmp = 0;
	T376_2RxFm *pRxFm = NULL;
	DWORD dwRc;
	DWORD dwLen;
	WORD wFn;
	WORD wLen;
	BYTE Buf[128];
	BYTE *pBuf = NULL;

	pRx->dwWptr = 0;
	pRx->dwRptr = 0;
	while ((dwDelay < 100) && (dwTmp < 10000))
	{
		MySleep(10);
		dwTmp += 10;
		if (pPort->fRead(pPort, pRx))
			dwDelay = 0;
		else
			dwDelay += 10;
	}
	if (pRTPort == pPort)
	{
		while (pRx->dwWptr > pRx->dwRptr)
		{
			dwRc = SearchOneFrameEx(&pRx->pBuf[pRx->dwRptr], (DWORD)(pRx->dwWptr - pRx->dwRptr));
			dwLen = (DWORD)(dwRc & ~FM_SHIELD);
			switch (dwRc&FM_SHIELD)
			{
			case FM_OK:
				pRxFm = (T376_2RxFm *)&pRx->pBuf[pRx->dwRptr];
				pBuf = (BYTE *)(pRxFm + 1);
				pBuf += DealInfoField(pRxFm);
				if (AFN06 == *pBuf)//先处理缓冲区报文
				{
					pBuf++;	//跳过AFN
					wFn = DT2Fn(*pBuf, *(pBuf + 1));
					pBuf += 2;	//跳过Fn
					if (F5 == wFn)
					{
						pBuf++;//跳过从节点设备类型
						pBuf++;//跳过通信协议类型
						wLen = (WORD)(*pBuf);
						if (wLen > 128)
							wLen = 128;
						pBuf++;//跳过数据长度
						memset(Buf, 0, sizeof(Buf));
						memcpy(Buf, pBuf, wLen);
						SendFm(PORT_ZB, Buf, wLen, 0x06F5, 0,TRUE);
						TxAfn00_F1(0xFFFFFFFF, 0);
					}
				}
				pRx->dwRptr += dwLen;
				break;
			case FM_ERR:
				if (dwLen)
					pRx->dwRptr += dwLen;
				else
					pRx->dwRptr++;
				break;
			default:
				pRx->dwRptr = pRx->dwWptr;
				break;
			}
		}
	}
	pRx->dwWptr = 0;
	pRx->dwRptr = 0;
	memset(pRx->pBuf, 0, pRx->dwSize);
}

///////////////////////////////////////////////////////////////
//	函 数 名 : RT698_TxDirect
//	函数功能 : 调用路由端口直接发送函数
//	处理过程 :
//	备    注 : 
//	作    者 : 
//	时    间 :
//	返 回 值 : void
//	参数说明 : void
///////////////////////////////////////////////////////////////
void RT698_TxDirect(void)
{
	TPort *pPort = GetRouterPort();
	TTx	*pTx = GetRouterTx();

	FrameTrace("Tx:", pTx->pBuf, pTx->dwWptr);
	pPort->fWrite(pPort, pTx);
	pPort->byStatus = MPS_TX; //开始发送
	pPort->dwCtrlTime = GetRunCount();
}

///////////////////////////////////////////////////////////////
//	函 数 名 : SearchOneFrameEx
//	函数功能 : 检索帧格式
//	处理过程 : 
//	备    注 : 
//	作    者 : 赵红军
//	时    间 : 2009年11月6日
//	返 回 值 : DWORD
//	参数说明 : HPARA hBuf,DWORD dwLen
///////////////////////////////////////////////////////////////
DWORD SearchOneFrameEx(HPARA hBuf, DWORD dwLen)
{
	BYTE *pBuf = (BYTE *)hBuf;
	T376_2TxFm0 *pRxFm = (T376_2TxFm0 *)pBuf;
	TRouterInfo *pRInfo = GetRouterInfo();
	TRouter *p = GetRouterApp();
	DWORD dwRc, dwFmLen;
	BYTE byCheck;

	if (dwLen < 15)
		return FM_LESS;

	dwRc = SearchHead1(hBuf, dwLen, 0, 0x68);
	if (dwRc)
		return FM_ERR | dwRc;
	//错误帧长判断
	if (pRxFm->wLen > 0x200)
		return FM_ERR | 1;
	dwFmLen = pRxFm->wLen;
	if (dwFmLen > dwLen)
		return FM_LESS;
	if (pRInfo->byChipType != MCHIP_HR)
	{
		if (!(pRxFm->byCtrl & 0x80))
			return FM_ERR | 1;
	}

	if (0x16 != pBuf[dwFmLen - 1])
		return FM_ERR | 1;

	byCheck = CheckSum(&pRxFm->byCtrl, dwFmLen - 5);
	if (byCheck != pBuf[dwFmLen - 2])
	{
		return FM_ERR | dwFmLen;
	}

	p->dwLastRxTime = GetRunCount();
	p->byModuleType = pRxFm->byCtrl & 0x3F;
	return FM_OK | dwFmLen;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : RxWait698_42
//	函数功能 : 接收698响应帧
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 2009年11月7日
//	返 回 值 : BYTE*			-接收到的响应报文指针
//	参数说明 : DWORD dwTime,	-接收等待时间
//				 BYTE* pResult,	-执行结果[正确/错误/无响应]
//				 DWORD* pdwLen	-接收到的响应报文长度
///////////////////////////////////////////////////////////////
BYTE* RxWait698_42(DWORD dwTime, BYTE* pResult, DWORD* pdwLen)
{
	TPort *pPort = GetRouterPort();
	TRouter *p = GetRouterApp();
	TRx *pRx = GetRouterRx();
	T376_2RxFm *pRxFm;
	BYTE Buf[128];
	DWORD dwLen, Rc, Len;
	int MsgLen;
	DWORD dwTmpRPtr;
	WORD wFn;
	WORD wLen;
	BYTE *pBuf = NULL;

	*pResult = 0;
	*pdwLen = 0;
	dwTime += 9;
	dwTime /= 10; //转成Tick数据
	if (dwTime == 0)
		dwTime = 1;
#if(0)
	while (1)
#else
	while (dwTime--)
#endif
	{
		MySleep(10);
		dwLen = pPort->fRead(pPort, pRx);
		if (dwLen == 0)
			continue;
		MsgLen = pRx->dwWptr - pRx->dwRptr;
		if (MsgLen <= 0)
			pRx->dwWptr = pRx->dwRptr = 0;
		pRx->dwBusy = GetRunCount();	//防止标准RxMonitor函数判为字符超时
		Rc = SearchOneFrameEx(&pRx->pBuf[pRx->dwRptr], (DWORD)MsgLen);
		Len = Rc & ~FM_SHIELD; //已处理过的字节数
		switch (Rc & FM_SHIELD)
		{
		case FM_OK://广播报文
			*pResult = MAT_OK;
			*pdwLen = Len;
			FrameTrace("Rx:", pRx->pBuf, pRx->dwWptr);
			//更新信道标识
			pRxFm = (T376_2RxFm *)&pRx->pBuf[pRx->dwRptr];
			p->byChanel = (pRxFm->byInfoChannel & 0x0F) % 4;
			dwTmpRPtr = pRx->dwRptr;
			pRx->dwRptr = pRx->dwWptr;
			pBuf = (BYTE *)(pRxFm + 1);
			if (pPort == GethPort())
			{
				pBuf += DealInfoField(pRxFm);
				if (AFN06 == *pBuf)//同步档案过程中有事件上报处理
				{
					pBuf++;	//跳过AFN
					wFn = DT2Fn(*pBuf, *(pBuf + 1));
					pBuf += 2;	//跳过Fn
					if (F5 == wFn)
					{
						pBuf++;//跳过从节点设备类型
						pBuf++;//跳过通信协议类型
						wLen = (WORD)(*pBuf);
						if (wLen > 128)
							wLen = 128;
						pBuf++;//跳过数据长度
						memset(Buf, 0, sizeof(Buf));
						memcpy(Buf, pBuf, wLen);
						SendFm(PORT_ZB, Buf, wLen, 0x06F5, 0,TRUE);
						TxAfn00_F1(0xFFFFFFFF, 0);
						RT698_PortRxClear(pPort);
						continue;
					}
				}
			}
			return &pRx->pBuf[dwTmpRPtr];
		case FM_ERR:
			if (!Len)
				pRx->dwRptr++;
			else
				pRx->dwRptr += Len; //指针移到下一报文处
			FrameTrace("Rx[err]:", pRx->pBuf, pRx->dwWptr);
			break;
		case FM_LESS:
			if (Len)
				pRx->dwRptr += Len; //指针移到下一报文处
			break;
		}

	}
	if (pRx->dwWptr != pRx->dwRptr)
		*pResult = MAT_ERR;
	else
		*pResult = MAT_NOASW;

	return &pRx->pBuf[pRx->dwRptr];
}

///////////////////////////////////////////////////////////////
//	函 数 名 : Tx_698Fm0
//	函数功能 : 组698帧
//	处理过程 :
//	备    注 : 与路由模块通信
//	作    者 : 
//	时    间 : 2009年11月6日
//	返 回 值 : void
//	参数说明 : BYTE byAfn,WORD wFn,BYTE* pData,WORD wLen
///////////////////////////////////////////////////////////////
void Tx_698Fm0(BYTE byAfn, WORD wFn, BYTE* pData, WORD wLen)
{
	TTx *pTx = GetRouterTx();
	T376_2TxFm0 *pTxFm = (T376_2TxFm0 *)pTx->pBuf;
	BYTE *pBuf = (BYTE *)(pTxFm + 1);
	TRouter *p = (TRouter *)GetRouterApp();

	pTx->dwRptr = pTx->dwWptr = 0;
	pTxFm->byS68 = 0x68;
	pTxFm->wLen = wLen + 6 + 9;
	switch (byAfn)
	{
	case 0x00:
	case 0x06:
	case 0x14:
		pTxFm->byCtrl = p->byModuleType & 0x3F;	//下行从动
		break;
	default:
		pTxFm->byCtrl = (p->byModuleType & 0x3F) | BIT6;	//下行主动
		break;
	}
	pTxFm->byInfoModule = 0;					//路由模式，无附加节点，对集中器的通讯模块操作
	pTxFm->byInfoChannel = p->byChanel;			//信道标识
	pTxFm->byInfoAskLen = 160;					//预计应答字节数
	pTxFm->wInfoBaud = 0;					//默认50 bps
	pTxFm->bySeq = p->bySeq++;
	pTxFm->byAfn = byAfn;
	pTxFm->byDT1 = 1 << ((wFn - 1) % 8);
	pTxFm->byDT2 = (wFn - 1) / 8;
	if (pData != NULL)
		memcpy(pBuf, pData, wLen);
	pBuf[wLen] = CheckSum(&pTxFm->byCtrl, wLen + 10);
	pBuf[wLen + 1] = 0x16;
	pTx->dwWptr = sizeof(T376_2TxFm0) + wLen + 2;
	Trace("AFN:%x FN:%x", byAfn, wFn);
//	FrameTrace("Tx:", pTx->pBuf, pTx->dwWptr);
}

///////////////////////////////////////////////////////////////
//	函 数 名 : Tx_698Fm1
//	函数功能 : 组698帧
//	处理过程 :
//	备    注 : 与载波从节点通信
//	作    者 : 
//	时    间 : 2009年11月6日
//	返 回 值 : void
//	参数说明 : BYTE byAfn,WORD wFn,BYTE* pData,BYTE byLen
///////////////////////////////////////////////////////////////
void Tx_698Fm1(DWORD dwDAddr, WORD wDAddEx, BYTE byRelay, BYTE* pbyRelayAddr, BYTE byAfn, WORD wFn, BYTE* pData, BYTE byLen)
{
	TTx *pTx = GetRouterTx();
	T376_2TxFm1 *pTxFm = (T376_2TxFm1 *)pTx->pBuf;
	TRouter *p = (TRouter *)GetRouterApp();
	TRouterInfo *pRInfo = GetRouterInfo();
	BYTE *pBuf = (BYTE *)(pTxFm + 1);
	BYTE byNo = 0;

	pTx->dwRptr = pTx->dwWptr = 0;
	pTxFm->byS68 = 0x68;
	pTxFm->wLen = byLen + 6 + 9 + 12 + byRelay * 6;
	switch (byAfn)
	{
	case AFN00:
	case AFN06:
	case AFN14:
		pTxFm->byCtrl = p->byModuleType & 0x3F; //下行从动
		break;
	default:
		pTxFm->byCtrl = (p->byModuleType & 0x3F) | BIT6;	  //下行主动
		break;
	}

	pTxFm->byInfoModule = ((byRelay << 4) | 0x04);  //路由模式，无附加节点，对从节点的通讯模块操作
	if (byAfn == AFN02)
		pTxFm->byInfoModule |= 0x01; //不带路由或工作在旁路模式

	if ((byAfn == AFN13) || (byAfn == AFN02))
		p->byChanel = 0;
	pTxFm->byInfoChannel = p->byChanel;
	pTxFm->byInfoAskLen = 160;	//预计应答字节数;
	pTxFm->wInfoBaud = 0;					//默认50 bps
	pTxFm->bySeq = p->bySeq++;
	//源地址
	ReverseCpy(&pBuf[byNo], pRInfo->szMainNet, 6);
	byNo += 6;
	//中继地址
	memcpy(&pBuf[byNo], pbyRelayAddr, byRelay * 6);
	byNo += (byRelay * 6);
	//目的地址
	pBuf[byNo++] = LLBYTE(dwDAddr);
	pBuf[byNo++] = LHBYTE(dwDAddr);
	pBuf[byNo++] = HLBYTE(dwDAddr);
	pBuf[byNo++] = HHBYTE(dwDAddr);
	pBuf[byNo++] = LOBYTE(wDAddEx);
	pBuf[byNo++] = HIBYTE(wDAddEx);
	pBuf[byNo++] = byAfn;
	pBuf[byNo++] = 1 << ((wFn - 1) % 8);
	pBuf[byNo++] = (wFn - 1) / 8;
	if (pData != NULL)
	{
		memcpy(&pBuf[byNo], pData, byLen);
		byNo += byLen;
	}
	pBuf[6 * 2 + byLen + byRelay * 6 + 3] = CheckSum(&pTxFm->byCtrl, 6 * 2 + byLen + 10 + byRelay * 6);
	pBuf[6 * 2 + byLen + byRelay * 6 + 4] = 0x16;
	pTx->dwWptr = sizeof(T376_2TxFm1) + byNo + 2;
	Trace("AFN:%x FN:%x", byAfn, wFn);
//	FrameTrace("Tx_698Fm1:", pTx->pBuf, pTx->dwWptr);
}

////////////////////////////////////////////////////////////////////
//	函 数 名 : TxCmd0
//	函数功能 : 发送698命令并等待响应
//	处理过程 : 响应超时将重发命令/响应的报文指针由szPtr传出
//	备    注 : 与集中器通信模块通信
//	作    者 : 
//	时    间 : 2009年11月7日
//	返 回 值 : BOOL
//	参数说明 : BYTE byAfn, WORD wFn,		-下发命令的Afn/Fn
//			BYTE* pData, WORD wLen,			-下发命令的数据域
//			DWORD dwDelay,BYTE byReTimes,	-等待响应时间/重发次数
//			BYTE** szPtr					-响应的报文指针
////////////////////////////////////////////////////////////////////
BOOL TxCmd0(BYTE byAfn, WORD wFn, BYTE* pData, WORD wLen, DWORD dwDelay, BYTE byReTimes, BYTE** szPtr)
{
	TPort *pPort = GetRouterPort();
	TTx *pTx = GetRouterTx();
	BYTE* Result;
	BYTE byTmp;
	DWORD dwLen;

	byTmp = byReTimes;
	*szPtr = NULL;
	while (byReTimes--)
	{
		RT698_PortRxClear(pPort);
		pTx->dwRptr = pTx->dwWptr = 0;
		Tx_698Fm0(byAfn, wFn, pData, wLen);
		RT698_TxDirect();
		Result = RxWait698_42(dwDelay, &byTmp, &dwLen);
		if (byTmp == MAT_OK)
		{
			*szPtr = Result;
			return TRUE;
		}
	}
	return FALSE;
}
///////////////////////////////////////////////////////////////
//	函 数 名 : IsStudyFinish
//	函数功能 : 检查是否学习完成
//	处理过程 : 
//	备    注 : 
//	作    者 : 张顺
//	时    间 : 2015年03月14日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL IsStudyFinish(void)
{
	TRouterInfo *pRInfo = NULL;

	if (TxAfn10_F4())
	{
		pRInfo = (TRouterInfo *)GetRouterInfo();
		CHECKPTR(pRInfo, FALSE);
		if (pRInfo->byChipType == MCHIP_ZR)
		{
			if ((pRInfo->byRunStatuse&BIT1) == 0)
				return TRUE;
			else
				return FALSE;
		}
		else
		{
			if ((pRInfo->byRunStatuse&BIT0) == BIT0)
				return TRUE;
			else
				return FALSE;
		}
	}
	else
		return FALSE;
}

BOOL IsEnabledFn(BYTE byFn, BYTE *pCfg)
{
	BYTE byByteNo, byBitNo;

	byByteNo = (byFn - 1) / 8;
	byBitNo = (byFn - 1) % 8;
	return (pCfg[byByteNo] & (1 << byBitNo));
}

#endif