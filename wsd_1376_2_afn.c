#include "wsd_Flat.h"
#include "wsd_1376_2_fun.h"
#include "wsd_1376_2.h"
#if(MD_DEV_PLC)
///////////////////////////////////////////////////////////////
//	函 数 名 : RecDeal
//	函数功能 : 接收处理698帧
//	处理过程 : 判断接收帧的Afn/Fn是否与预期相符
//	备    注 : 相符则将Fn之后数据域交AfnFun处理
//	作    者 : 
//	时    间 : 2009年11月7日
//	返 回 值 : BOOL
//	参数说明 : BYTE byAfn, WORD wFn, BYTE* pBuf,FB_HD AfnFun
///////////////////////////////////////////////////////////////
BOOL RecDeal(BYTE byAfn, WORD wFn, BYTE* pBuf, FB_HD AfnFun)
{
	T376_2TxFm1 *pRxFm = (T376_2TxFm1 *)pBuf;
	BYTE* pAddr = (BYTE*)(pRxFm + 1);
	BYTE byLen, DT1, DT2;
	WORD wFmLen;

	CHECKPTR(pRxFm, FALSE);
	wFmLen = pRxFm->wLen;
	wFmLen -= 12;					//减去固定长度和信息域
	if (pRxFm->byInfoModule & 0x04)	//通讯模块标示
	{
		byLen = 6 * 2;
		pAddr += byLen;				//地址域去掉
		wFmLen -= byLen;
	}
	if (byAfn != pAddr[0])
	{
		return FALSE;
	}
	wFmLen--;
	DT1 = 1 << ((wFn - 1) % 8);
	DT2 = (wFn - 1) / 8;
	if (MW(DT1, DT2) != MW(pAddr[1], pAddr[2]))
	{
		return FALSE;
	}
	wFmLen -= 2;
	if (AfnFun == NULL)
	{
		return TRUE;
	}

	return AfnFun(&pAddr[3], wFmLen);
}

//////////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn00_F1
//	函数功能 : 确认
//	处理过程 : 
//	备    注 : 
//	作    者 : 赵红军
//	时    间 : 2009年11月6日
//	返 回 值 : void
//	参数说明 : WORD wChlSta-信道状态 WORD wDelay-等待时间
//////////////////////////////////////////////////////////////////
void TxAfn00_F1(DWORD dwChlSta, WORD wDelay)
{
	BYTE byBuf[6];

	byBuf[0] = LLBYTE(dwChlSta);
	byBuf[1] = LHBYTE(dwChlSta);
	byBuf[2] = HLBYTE(dwChlSta);
	byBuf[3] = HHBYTE(dwChlSta);
	byBuf[4] = LOBYTE(wDelay);
	byBuf[5] = HIBYTE(wDelay);
	Tx_698Fm0(AFN00, F1, byBuf, 6);
	RT698_TxDirect();
}

//////////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn00_F2
//	函数功能 : 否认
//	处理过程 : 
//	备    注 : 
//	作    者 : 赵红军
//	时    间 : 2009年11月6日
//	返 回 值 : void
//	参数说明 : BYTE byErrID-错误代码
//////////////////////////////////////////////////////////////////
void TxAfn00_F2(BYTE byErrID)
{
	Tx_698Fm0(AFN00, F2, &byErrID, 1);
	RT698_TxDirect();
}


BOOL RxAfn01_F2(BYTE* pBuf, BYTE byLen)
{
	DWORD dwRecLen = 0;
	WORD wDelay;

	dwRecLen += 4;
	wDelay = MW(pBuf[dwRecLen + 1], pBuf[dwRecLen]);
	MySleep(1000 * ((wDelay<30) ? wDelay : 30));

	return TRUE;
}
///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn01_F2
//	函数功能 : 参数区初始化
//	处理过程 : 
//	备    注 : 
//	作    者 : 王选友
//	时    间 : 2009年10月14日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL TxAfn01_F2(void)
{
	BYTE* pResult;

	if (TxCmd0(AFN01, F2, NULL, 0, 30000, 1, &pResult))
	{
		if (RecDeal(AFN00, F1, pResult, (FB_HD)RxAfn01_F2))
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL RxAfn03_F1(BYTE* pBuf, DWORD dwLen)
{
	TRouterInfo *pRInfo = GetRouterInfo();
	DWORD dwRecLen = 0;

	//厂商代码
	ReverseCpy(pRInfo->szFactory, &pBuf[dwRecLen], 2);
	switch (MW(pRInfo->szFactory[0], pRInfo->szFactory[1]))
	{
	case 0x4553:	pRInfo->byChipType = MCHIP_ES;		break;
	case 0x5443:	pRInfo->byChipType = MCHIP_TC;		break;
	case 0x4358:	pRInfo->byChipType = MCHIP_XC;		break;
	case 0x3031:	pRInfo->byChipType = MCHIP_RC;		break;
	case 0x4D49:	pRInfo->byChipType = MCHIP_MI;		break;
	case 0x5353:	pRInfo->byChipType = MCHIP_SS;		break;
	case 0x484C:    pRInfo->byChipType = MCHIP_LM;		break;
	case 0x4C52:    pRInfo->byChipType = MCHIP_RL;		break;
	case 0x4346:	pRInfo->byChipType = MCHIP_FC;		break;
	case 0x5253:	pRInfo->byChipType = MCHIP_SR;		break;
	case 0x5248:	pRInfo->byChipType = MCHIP_HS;		break;
	case 0x584D:	pRInfo->byChipType = MCHIP_MX;		break;
	case 0x5359:	pRInfo->byChipType = MCHIP_SY;		break;
	case 0x5A43:	pRInfo->byChipType = MCHIP_ZC;		break;
	case 0x3337:	pRInfo->byChipType = MCHIP_ZR;		break;
	case 0x4557:	pRInfo->byChipType = MCHIP_LM;		break;
	default:		pRInfo->byChipType = 0xFF;		Trace("新模块-厂商代码[%c%c]", pRInfo->szFactory[0], pRInfo->szFactory[1]);
	}
	dwRecLen += 2;
	//芯片代码
	memcpy(pRInfo->szChip, &pBuf[dwRecLen], 2);
	dwRecLen += 2;
	//版本时间
	snprintf(pRInfo->szPrgTime, 30, "%04d-%02d-%02d", Bcd2Bin(pBuf[dwRecLen + 2]) + 2000, Bcd2Bin(pBuf[dwRecLen + 1]), Bcd2Bin(pBuf[dwRecLen]));
	dwRecLen += 3;
	//版本号
	if (pRInfo->byChipType == MCHIP_RL || pRInfo->byChipType == MCHIP_SR || pRInfo->byChipType == MCHIP_FC)
	{
		snprintf(pRInfo->szVer,30, "%02d%02d", Bcd2Bin(pBuf[dwRecLen]), Bcd2Bin(pBuf[dwRecLen + 1]));
	}
	else
	{
		snprintf(pRInfo->szVer,30, "%02d%02d", Bcd2Bin(pBuf[dwRecLen + 1]), Bcd2Bin(pBuf[dwRecLen]));
	}
	dwRecLen += 2;
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn03_F1
//	函数功能 : 厂商代码和版本信息
//	处理过程 : 
//	备    注 : 
//	作    者 : 王选友
//	时    间 : 2009年10月14日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL TxAfn03_F1(void)
{
	BYTE* pResult;

	if (TxCmd0(AFN03, F1, NULL, 0, 1000, 1, &pResult))
	{
		if (RecDeal(AFN03, F1, pResult, (FB_HD)RxAfn03_F1))
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL RxAfn03_F4(BYTE* pBuf, DWORD dwLen)
{
	TRouterInfo *pRInfo = GetRouterInfo();
	BYTE DI[6];

	memcpy(&DI[0], pBuf, 6);
	InvertBuf(6, &DI[0]);
	if (memcmp(pRInfo->szMainNet, DI, 6) == 0)
	{
		return TRUE;
	}
	if (TxAfn05_F1((BYTE*)pRInfo->szMainNet))
	{
		return TRUE;
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn03_F4
//	函数功能 : 载波主节点地址
//	处理过程 : 
//	备    注 : 
//	作    者 : 王选友
//	时    间 : 2009年10月14日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL TxAfn03_F4(void)
{
	BYTE* pResult;

	FreshRTSta(RTST_CHKMAINID, NULLP, NULLP, NULLP);
	if (TxCmd0(AFN03, F4, NULL, 0, 2500, 1, &pResult))
	{
		if (RecDeal(AFN03, F4, pResult, (FB_HD)RxAfn03_F4))
		{
			FreshRTSta(RTST_CHKMAINID, 0, NULLP, 1);
			return TRUE;
		}
	}
	FreshRTSta(RTST_CHKMAINID, 0, NULLP, 2);
	return FALSE;
}

BOOL RxAfn03_F5(BYTE* pBuf, DWORD dwLen)
{
	TRouterInfo *pRInfo = GetRouterInfo();

	pRInfo->wMainStatuse = MW(pBuf[1], pBuf[0]);
	pRInfo->byCBMode = (pBuf[0] >> 6);
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn03_F5
//	函数功能 : 载波主节点状态字和载波速率
//	处理过程 : 
//	备    注 : 
//	作    者 : 王选友
//	时    间 : 2009年10月14日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL TxAfn03_F5(void)
{
	BYTE* pResult;

	if (TxCmd0(AFN03, F5, NULL, 0, 2000, 1, &pResult))
	{
		if (RecDeal(AFN03, F5, pResult, (FB_HD)RxAfn03_F5))
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL RxAfn03_F6(BYTE* pBuf, DWORD dwLen)
{
	TRouterInfo *pRInfo = GetRouterInfo();

	pRInfo->byHasTurb = pBuf[0];

	return TRUE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn03_F6
//	函数功能 : 载波主节点干扰状态
//	处理过程 : 
//	备    注 : 
//	作    者 : 王选友
//	时    间 : 2009年10月14日
//	返 回 值 : BOOL
//	参数说明 : BYTE byTime
///////////////////////////////////////////////////////////////
BOOL TxAfn03_F6(BYTE byTime)
{
	BYTE* pResult;
	BYTE byBuf[10];
	BYTE byLen = 0;

	byBuf[byLen++] = byTime;

	if (TxCmd0(AFN03, F6, byBuf, byLen, 2000, 1, &pResult))
	{
		if (RecDeal(AFN03, F5, pResult, NULL))
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL RxAfn03_F7(BYTE* pBuf, DWORD dwLen)
{
	TRouterInfo *pRInfo = GetRouterInfo();

	pRInfo->wMaxTimeout = pBuf[0];
	Trace("模块点抄最大超时时间：%d s", pRInfo->wMaxTimeout);
	COMPARECHK0(pRInfo->wMaxTimeout, 60);
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn03_F7
//	函数功能 : 监控从节点最大超时时间
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 2009年10月14日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL TxAfn03_F7(void)
{
	BYTE* pResult;

	if (TxCmd0(AFN03, F7, NULL, 0, 1000, 1, &pResult))
	{
		if (RecDeal(AFN03, F7, pResult, (FB_HD)RxAfn03_F7))
		{
			return TRUE;
		}
	}
	return FALSE;
}

BOOL RxAfn03_F8(BYTE* pBuf, DWORD dwLen)
{
	TRouterInfo *pRInfo = GetRouterInfo();

	pRInfo->byChanelNo = pBuf[0];
	pRInfo->byCACPower = pBuf[1];
	return TRUE;
}
///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn03_F8
//	函数功能 : 查询无线通信参数
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 2009年10月14日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL TxAfn03_F8(void)
{
	BYTE* pResult;

	if (TxCmd0(AFN03, F8, NULL, 0, 1000, 1, &pResult))
	{
		if (RecDeal(AFN03, F8, pResult, (FB_HD)RxAfn03_F8))
		{
			return TRUE;
		}
	}
	return FALSE;
}

BOOL RxAfn03_F10(BYTE* pBuf, DWORD dwLen)
{
	TRouterInfo *pRInfo = GetRouterInfo();
	BYTE DI[6];

	if (dwLen < 39)
		return FALSE;
	pRInfo->byCommMode = pBuf[0] & 0x0F;
	pRInfo->byRTManMode = (pBuf[0] >> 4) & BIT0;
	pRInfo->bySubMode = (pBuf[0] >> 5) & BIT0;
	pRInfo->byCBMode = (pBuf[0] >> 6);
	pRInfo->byDelaySupport = pBuf[1] & 0x07;
	pRInfo->bySwitchMode = (pBuf[1] >> 3) & 0x03;
	pRInfo->byBroadAckMode = (pBuf[1] >> 5) & BIT0;
	pRInfo->byBroadChanel = (pBuf[1] >> 6);
	pRInfo->byChanelNum = pBuf[2] & 0x1F;
	pRInfo->byPowerDwnInfo = pBuf[2] >> 5;
	pRInfo->byBaudNum = pBuf[3] & 0x0F;
	pRInfo->wMaxTimeout = pBuf[6];
	pRInfo->wMaxBroadTimeout = MW(pBuf[8], pBuf[7]);
	pRInfo->wMaxPackSize = MW(pBuf[10], pBuf[9]);
	pRInfo->wMaxFileTransSize = MW(pBuf[12], pBuf[11]);
	pRInfo->byUpdateIdle = pBuf[13];
	memcpy(&DI[0], &pBuf[14], 6);
	InvertBuf(6, &DI[0]);
	pRInfo->wMaxIDNum = MW(pBuf[21], pBuf[20]);
	pRInfo->wIDNum = MW(pBuf[23], pBuf[22]);
	pRInfo->byRTProTm_YMD[0] = Bcd2Bin(pBuf[24]);
	pRInfo->byRTProTm_YMD[1] = Bcd2Bin(pBuf[25]);
	pRInfo->byRTProTm_YMD[2] = Bcd2Bin(pBuf[26]);
	pRInfo->byRTProBATm_YMD[0] = Bcd2Bin(pBuf[27]);
	pRInfo->byRTProBATm_YMD[1] = Bcd2Bin(pBuf[28]);
	pRInfo->byRTProBATm_YMD[2] = Bcd2Bin(pBuf[29]);

	//厂商代码
	ReverseCpy(pRInfo->szFactory, &pBuf[30], 2);
	switch (MW(pRInfo->szFactory[0], pRInfo->szFactory[1]))
	{
	case 0x4553:	pRInfo->byChipType = MCHIP_ES;		break;
	case 0x5443:	pRInfo->byChipType = MCHIP_TC;		break;
	case 0x4358:	pRInfo->byChipType = MCHIP_XC;		break;
	case 0x3031:	pRInfo->byChipType = MCHIP_RC;		break;
	case 0x4D49:	pRInfo->byChipType = MCHIP_MI;		break;
	case 0x5353:	pRInfo->byChipType = MCHIP_SS;		break;
	case 0x484C:    pRInfo->byChipType = MCHIP_LM;		break;
	case 0x4C52:    pRInfo->byChipType = MCHIP_RL;		break;
	case 0x4643:	pRInfo->byChipType = MCHIP_FC;		break;
	case 0x5253:	pRInfo->byChipType = MCHIP_SR;		break;
	case 0x5248:	pRInfo->byChipType = MCHIP_HS;		break;
	case 0x584D:	pRInfo->byChipType = MCHIP_MX;		break;
	case 0x5359:	pRInfo->byChipType = MCHIP_SY;		break;
	case 0x5A43:	pRInfo->byChipType = MCHIP_ZC;		break;
	case 0x3337:	pRInfo->byChipType = MCHIP_ZR;		break;
	case 0x4557:	pRInfo->byChipType = MCHIP_LM;		break;
	default:
		pRInfo->byChipType = 0xFF;
		Trace("新模块-厂商代码[%c%c]", pRInfo->szFactory[0], pRInfo->szFactory[1]);
		break;
	}
	//芯片代码
	memcpy(pRInfo->szChip, &pBuf[32], 2);
	//版本时间
	snprintf(pRInfo->szPrgTime,30, "%04d-%02d-%02d", Bcd2Bin(pBuf[36]) + 2000, Bcd2Bin(pBuf[35]), Bcd2Bin(pBuf[34]));
	//版本号
	snprintf(pRInfo->szVer,30, "%02d%02d", Bcd2Bin(pBuf[38]), Bcd2Bin(pBuf[37]));
	Trace("PLC model:%d type:%d", pRInfo->byCommMode, pRInfo->byChipType);
	return TRUE;
}
///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn03_F10
//	函数功能 : 查询本地通信模块运行模式信息
//	处理过程 : 
//	备    注 : 
//	作    者 : 
//	时    间 : 2009年10月14日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL TxAfn03_F10(void)
{
	TRouterInfo *pRInfo = GetRouterInfo();
	BYTE* pResult;

	FreshRTSta(RTST_CHKROUTER, NULLP, NULLP, NULLP);
	if (TxCmd0(AFN03, F10, NULL, 0, 1000, 1, &pResult))
	{
		if (RecDeal(AFN03, F10, pResult, (FB_HD)RxAfn03_F10))
		{
			FreshRTSta(RTST_CHKROUTER, 0, NULLP, 1);
			return TRUE;
		}
	}
	FreshRTSta(RTST_CHKROUTER, 0, NULLP, 2);
	return FALSE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn03_F11
//	函数功能 : 查询AFN索引数据单元
//	处理过程 : 
//	备    注 : 
//	作    者 : Administrator
//	时    间 : 2013年8月20日
//	返 回 值 : BOOL
//	参数说明 : BYTE byAfn,
//				 BYTE *pFnList
///////////////////////////////////////////////////////////////
BOOL TxAfn03_F11(BYTE byAfn, BYTE *pFnList)
{
	return FALSE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn04_F1
//	函数功能 : 发送测试
//	处理过程 : 
//	备    注 : 
//	作    者 : 王选友
//	时    间 : 2009年10月14日
//	返 回 值 : BOOL
//	参数说明 : BYTE byTime
///////////////////////////////////////////////////////////////
BOOL TxAfn04_F1(BYTE byTime)
{
	BYTE* pResult;
	BYTE byBuf[10];
	BYTE byLen = 0;

	byBuf[byLen++] = byTime;

	if (TxCmd0(AFN04, F1, byBuf, byLen, 2000, 1, &pResult))
	{
		if (RecDeal(AFN00, F1, pResult, NULL))
		{
			return TRUE;
		}
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn04_F2
//	函数功能 : 载波从节点点名
//	处理过程 : 
//	备    注 : 
//	作    者 : 王选友
//	时    间 : 2009年10月14日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL TxAfn04_F2(void)
{
	BYTE* pResult;

	if (TxCmd0(AFN04, F2, NULL, 0, 2000, 1, &pResult))
	{
		if (RecDeal(AFN00, F1, pResult, NULL))
		{
			return TRUE;
		}
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn04_F3
//	函数功能 : 本地通信模块报文通信测试
//	处理过程 : 
//	备    注 : 
//	作    者 : Administrator
//	时    间 : 2013年8月20日
//	返 回 值 : BOOL
//	参数说明 : BYTE byBaud,DWORD dwAddr,WORD wAddrEx,
//				 BYTE byPro,BYTE byLen,BYTE *pBuf
///////////////////////////////////////////////////////////////
BOOL TxAfn04_F3(BYTE byBaud, DWORD dwAddr, WORD wAddrEx, BYTE byPro, BYTE byLen, BYTE *pBuf)
{
	BYTE* pResult;
	BYTE Buf[140] = { 0 };

	if (byLen > 128)
		return FALSE;
	Buf[0] = byBaud;
	Buf[1] = LLBYTE(dwAddr);
	Buf[2] = LHBYTE(dwAddr);
	Buf[3] = HLBYTE(dwAddr);
	Buf[4] = HHBYTE(dwAddr);
	Buf[5] = LOBYTE(wAddrEx);
	Buf[6] = HIBYTE(wAddrEx);
	Buf[7] = byPro;
	Buf[8] = byLen;
	memcpy(&Buf[9], pBuf, byLen);
	if (TxCmd0(AFN04, F3, Buf, (BYTE)(byLen + 9), 2000, 3, &pResult))
	{
		if (RecDeal(AFN00, F1, pResult, NULL))
			return TRUE;
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn05_F1
//	函数功能 : 设置载波主节点地址
//	处理过程 : 
//	备    注 : 
//	作    者 : 王选友
//	时    间 : 2009年10月14日
//	返 回 值 : BOOL
//	参数说明 : BYTE byAddr[6]
///////////////////////////////////////////////////////////////
BOOL TxAfn05_F1(BYTE byAddr[6])
{
	TRouterInfo *pRInfo = GetRouterInfo();
	BYTE* pResult;
	BYTE Buf[6];

	ReverseCpy(Buf, byAddr, 6);
	if (TxCmd0(AFN05, F1, Buf, 6, 2000, 1, &pResult))
	{
		if (RecDeal(AFN00, F1, pResult, NULL))
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL RxAfn10_F1(BYTE* pBuf, BYTE byLen)
{
	TRouterInfo *pRInfo = GetRouterInfo();
	DWORD dwRecLen = 0;

	pRInfo->wIDNum = MW(pBuf[dwRecLen + 1], pBuf[dwRecLen]);
	dwRecLen += 2;

	pRInfo->wMaxIDNum = MW(pBuf[dwRecLen + 1], pBuf[dwRecLen]);
	dwRecLen += 2;
	Trace("CCO ID Num = %d", pRInfo->wIDNum);
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn10_F1
//	函数功能 : 载波从节点数量
//	处理过程 : 
//	备    注 : 
//	作    者 : 王选友
//	时    间 : 2009年10月15日
//	返 回 值 : BOOL
//	参数说明 : BYTE byLen,
//				 BYTE* pBuf
///////////////////////////////////////////////////////////////
BOOL TxAfn10_F1(void)
{
	BYTE* pResult;

	if (TxCmd0(AFN10, F1, NULL, 0, 2000, 1, &pResult))
	{
		if (RecDeal(AFN10, F1, pResult, (FB_HD)RxAfn10_F1))
		{
			return TRUE;
		}
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn10_F2
//	函数功能 : 载波从节点信息
//	处理过程 : 
//	备    注 : 
//	作    者 : 赵红军
//	时    间 : 2012年8月13日
//	返 回 值 : BOOL
//	参数说明 : WORD wStart, BYTE byNum, --查询的起始序号和个数(最多10个)
//				BYTE* pAddr, BYTE* pbyNum --返回的地址列表和个数
///////////////////////////////////////////////////////////////
BOOL TxAfn10_F2(WORD wStart, BYTE byNum, BYTE* pAddr, BYTE* pbyNum)
{
	WORD wTotalNum;
	BYTE* pResult;
	BYTE  pBuf[10];
	BYTE byMPNum, byNo, byLen = 0;

	pBuf[0] = LOBYTE(wStart);
	pBuf[1] = HIBYTE(wStart);
	pBuf[2] = (byNum>10) ? 10 : byNum;

	FreshRTSta(RTST_CHKIDINFO, NULLP, NULLP, NULLP);
	if (TxCmd0(AFN10, F2, pBuf, 3, 2000, 3, &pResult))
	{
		if (RecDeal(AFN10, F2, pResult, NULL))
		{
			byLen += 13; //指向数据域
			wTotalNum = MW(pResult[byLen + 1], pResult[byLen]);
			byLen += 2;
			byMPNum = pResult[byLen++];
			for (byNo = 0; byNo<byMPNum && byNo<10 && byNo<byNum; byNo++)
			{
				memcpy(pAddr, &pResult[byLen], 6);
				Trace("AFN10_F2:%02x%02x%02x%02x%02x%02x", pAddr[5], pAddr[4], pAddr[3], pAddr[2], pAddr[1], pAddr[0]);
				pAddr += 6;
				byLen += 6;
				byLen += 2;
			}
			*pbyNum = byNo;
			FreshRTSta(RTST_CHKIDINFO, 0, NULLP, 1);
			return TRUE;
		}
	}
	FreshRTSta(RTST_CHKIDINFO, 0, NULLP, 2);
	return FALSE;
}

BOOL RxAfn10_F4(BYTE* pBuf, DWORD dwLen)
{
	TRouter *p = (TRouter *)GethApp();
	TRouterInfo *pRInfo = GetRouterInfo();
	DWORD dwRecLen = 0;

	pRInfo->byRunStatuse = pBuf[dwRecLen++];
	pRInfo->wIDNum = MW(pBuf[dwRecLen + 1], pBuf[dwRecLen]);
	dwRecLen += 2;

	pRInfo->wOKIDNum = MW(pBuf[dwRecLen + 1], pBuf[dwRecLen]);
	pRInfo->wNetnum = pRInfo->wOKIDNum; //zhj 2014.9.15
	if (pRInfo->wIDNum >= pRInfo->wOKIDNum)
		pRInfo->wDeNetnum = pRInfo->wIDNum - pRInfo->wNetnum;
	dwRecLen += 2;

	pRInfo->wRelayIDNUm = MW(pBuf[dwRecLen + 1], pBuf[dwRecLen]);
	dwRecLen += 2;

	pRInfo->byRunSwitch = pBuf[dwRecLen++];

	pRInfo->wBoaud = MW(pBuf[dwRecLen + 1], pBuf[dwRecLen]);
	dwRecLen += 2;

	pRInfo->byRelay[0] = pBuf[dwRecLen++];
	pRInfo->byRelay[1] = pBuf[dwRecLen++];
	pRInfo->byRelay[2] = pBuf[dwRecLen++];

	pRInfo->byRunStep[0] = pBuf[dwRecLen++];
	pRInfo->byRunStep[1] = pBuf[dwRecLen++];
	pRInfo->byRunStep[2] = pBuf[dwRecLen++];
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn10_F4
//	函数功能 : 路由运行状态
//	处理过程 : 
//	备    注 : 
//	作    者 : 王选友
//	时    间 : 2009年10月15日
//	返 回 值 : BOOL
//	参数说明 : BYTE byLen,
//				 BYTE* pBuf
///////////////////////////////////////////////////////////////
BOOL TxAfn10_F4(void)
{
	BYTE* pResult;

	if (TxCmd0(AFN10, F4, NULL, 0, 5000, 1, &pResult))
	{
		if (RecDeal(AFN10, F4, pResult, (FB_HD)RxAfn10_F4))
		{
			return TRUE;
		}
	}
	return FALSE;
}

BOOL RxAfn11_F1(BYTE* pBuf, DWORD dwLen)
{
	return (pBuf[0] == MERR_METREPEAT); //表号已存在
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn11_F1
//	函数功能 : 一次添加多个节点
//	处理过程 : 
//	备    注 : 一次不多于10个
//	作    者 : 赵红军
//	时    间 : 2010年10月9日
//	返 回 值 : BOOL
//	参数说明 : HPARA *pPara
///////////////////////////////////////////////////////////////
BOOL TxAfn11_F1(HPARA *pPara)
{
	TID_Array *pRTID = (TID_Array *)pPara;
	BYTE* pResult;

	if (TxCmd0(AFN11, F1, (BYTE*)pRTID, (BYTE)(1 + pRTID->byNum*sizeof(TRT_ID)), 5000, 3, &pResult))
	{
		if (RecDeal(AFN00, F1, pResult, NULL)) //确认
		{
			return TRUE;
		}
		if (RecDeal(AFN00, F2, pResult, (FB_HD)RxAfn11_F1)) //表号已存在
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL RxAfn11_F2(BYTE* pBuf, DWORD dwLen)
{
	return (pBuf[0] == MERR_NOTEXIST); //表号不存在
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn11_F2
//	函数功能 : 删除单个从节点
//	处理过程 : 
//	备    注 : 
//	作    者 : 王选友
//	时    间 : 2009年10月15日
//	返 回 值 : BOOL
//	参数说明 : DWORD dwAddr,WORD wAddrEx
///////////////////////////////////////////////////////////////
BOOL TxAfn11_F2(DWORD dwAddr, WORD wAddrEx)
{
	BYTE* pResult;
	BYTE pBuf[10], byLen = 0;

	pBuf[byLen++] = 1;
	pBuf[byLen++] = LLBYTE(dwAddr);
	pBuf[byLen++] = LHBYTE(dwAddr);
	pBuf[byLen++] = HLBYTE(dwAddr);
	pBuf[byLen++] = HHBYTE(dwAddr);
	pBuf[byLen++] = LOBYTE(wAddrEx);
	pBuf[byLen++] = HIBYTE(wAddrEx);

	if (TxCmd0(AFN11, F2, pBuf, byLen, 2000, 1, &pResult))
	{
		if (RecDeal(AFN00, F1, pResult, NULL))
		{
			return TRUE;
		}
		if (RecDeal(AFN00, F2, pResult, (FB_HD)RxAfn11_F2)) //表号不存在
		{
			return TRUE;
		}
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn12_F1
//	函数功能 : 重启
//	处理过程 : 
//	备    注 : 
//	作    者 : 王选友
//	时    间 : 2009年10月15日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL TxAfn12_F1(void)
{
	BYTE* pResult;

	if (TxCmd0(AFN12, F1, NULL, 0, 2000, 1, &pResult))
	{
		if (RecDeal(AFN00, F1, pResult, NULL))
		{
			return TRUE;
		}
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn12_F2
//	函数功能 : 暂停
//	处理过程 : 
//	备    注 : 
//	作    者 : 王选友
//	时    间 : 2009年10月15日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL TxAfn12_F2(void)
{
	BYTE* pResult;

	if (TxCmd0(AFN12, F2, NULL, 0, 2000, 2, &pResult))
	{
		if (RecDeal(AFN00, F1, pResult, NULL))
		{
			return TRUE;
		}
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn12_F3
//	函数功能 : 恢复
//	处理过程 : 
//	备    注 : 
//	作    者 : 王选友
//	时    间 : 2009年10月15日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL TxAfn12_F3(void)
{
	BYTE* pResult;

	if (TxCmd0(AFN12, F3, NULL, 0, 2000, 1, &pResult))
	{
		if (RecDeal(AFN00, F1, pResult, NULL))
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL RxAfn14_F3(BYTE* pBuf, DWORD dwLen)
{
	TRouter *p = GetRouterApp();

	if (dwLen<21)
		return FALSE;
	pBuf += 6; //跳过从节点地址
	p->wCommDly = MW(pBuf[1], pBuf[0]);

	return TRUE;
}

BOOL RxAfn13_F1(BYTE* pBuf, DWORD dwLen)
{
	if (pBuf[3] == 0)	//抄回长度为0
		return FALSE;
	return TRUE;
}

////////////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn14_F1
//	函数功能 : 回复抄读内容
//	处理过程 : 
//	备    注 : 
//	作    者 : 赵红军
//	时    间 : 2009年11月7日
//	返 回 值 : void
//	参数说明 : DWORD dwDAddr,WORD wDAddEx,	-目的地址
//				BYTE byType,				-操作类型[0:失败跳过 1:成功跳过 2:抄读]
//				BYTE* pData,BYTE byLen		-645帧
////////////////////////////////////////////////////////////////////
void TxAfn14_F1(DWORD dwDAddr, WORD wDAddEx, BYTE byType, BYTE* pData, BYTE byLen)
{
	TRouterInfo *pRInfo = GetRouterInfo();
	TRouter *p = GetRouterApp();
	BYTE Len = 0, Buf[130];

	if (byLen>128)
	{
		Trace("! Overflow !");
		return;
	}
	Buf[Len++] = byType;
	Buf[Len++] = 0; //通信延时相关性标志
	Buf[Len++] = byLen;
	if (byLen)
	{
		memcpy(&Buf[Len], pData, byLen);
		Len += byLen;
	}
	Buf[Len++] = 0;
	if (pRInfo->byChipType == MCHIP_ES || pRInfo->byChipType == MCHIP_MI)
	{
		Tx_698Fm0(AFN14, F1, Buf, Len);
	}
	else
	{
		Tx_698Fm1(dwDAddr, wDAddEx, 0, NULL, AFN14, F1, Buf, Len);
	}
	RT698_TxDirect();
}

//////////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn14_F2
//	函数功能 : 回应时钟
//	处理过程 : 
//	备    注 : 
//	作    者 : 赵红军
//	时    间 : 2009年11月6日
//	返 回 值 : void
//	参数说明 : void
//////////////////////////////////////////////////////////////////
void TxAfn14_F2(void)
{
	BYTE byBuf[6];

	byBuf[0] = Bin2Bcd(gSysTime.Sec);//zs 15.11.3按照376.2协议修改为BCD码
	byBuf[1] = Bin2Bcd(gSysTime.Min);
	byBuf[2] = Bin2Bcd(gSysTime.Hour);
	byBuf[3] = Bin2Bcd(gSysTime.Day);
	byBuf[4] = Bin2Bcd(gSysTime.Mon);
	byBuf[5] = Bin2Bcd((BYTE)(gSysTime.wYear - 2000));
	Tx_698Fm0(AFN14, F2, byBuf, 6);
	RT698_TxDirect();
}

///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn10_F21
//	函数功能 : 网络拓扑信息
//	处理过程 : 
//	备    注 : 
//	作    者 : 张顺
//	时    间 : 2018年09月14日
//	返 回 值 : BOOL
//	参数说明 : wStart 起始序号,byNum 本次查询个数,*pData 返回数据指针
///////////////////////////////////////////////////////////////
BOOL TxAfn10_F21(WORD wStart, BYTE byNum, BYTE *pData)
{
	BYTE* pResult;
	BYTE  Buf[3];
	BYTE *pBuf;
	T376_2RxFm0 *pHead = NULL;

	Buf[0] = LOBYTE(wStart);
	Buf[1] = HIBYTE(wStart);
	Buf[2] = min(byNum, MAX_NODENUM_FM);
	if (TxCmd0(AFN10, F21, Buf, 3, 2000, 1, &pResult))
	{
		if (RecDeal(AFN10, F21, pResult, NULL))
		{
			pHead = (T376_2RxFm0 *)pResult;
			pBuf = (BYTE *)(pHead + 1);
			memcpy(pData, pBuf, pHead->wLen - sizeof(T376_2RxFm0) - sizeof(T376_2FmTail));
			return TRUE;
		}
	}
	return FALSE;
}
///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn10_F112
//	函数功能 : 宽带载波芯片信息
//	处理过程 : 
//	备    注 : 
//	作    者 : 张顺
//	时    间 : 2018年10月22日
//	返 回 值 : BOOL
//	参数说明 : wStart 起始序号,byNum 本次查询个数,*pData 返回数据指针
///////////////////////////////////////////////////////////////
BOOL TxAfn10_F112(WORD wStart, BYTE byNum, BYTE *pData)
{
	BYTE* pResult;
	BYTE  Buf[3];
	BYTE *pBuf;
	T376_2RxFm0 *pHead = NULL;

	Buf[0] = LOBYTE(wStart);
	Buf[1] = HIBYTE(wStart);
	Buf[2] = min(byNum, MAX_NODENUM_FM);
	if (TxCmd0(AFN10, F112, Buf, 3, 2000, 1, &pResult))
	{
		if (RecDeal(AFN10, F112, pResult, NULL))
		{
			pHead = (T376_2RxFm0 *)pResult;
			pBuf = (BYTE *)(pHead + 1);
			memcpy(pData, pBuf, pHead->wLen - sizeof(T376_2RxFm0) - sizeof(T376_2FmTail));
			return TRUE;
		}
	}
	return FALSE;
}
///////////////////////////////////////////////////////////////
//	函 数 名 : TxAfn10_F111
//	函数功能 : 查询多网络信息
//	处理过程 : 
//	备    注 : 
//	作    者 : 张顺
//	时    间 : 2018年10月23日
//	返 回 值 : BOOL
//	参数说明 : void
///////////////////////////////////////////////////////////////
BOOL TxAfn10_F111(BYTE *pData)
{
	BYTE* pResult;
	BYTE *pBuf;
	T376_2RxFm0 *pHead = NULL;

	if (TxCmd0(AFN10, F111, NULL, NULLP, 2000, 1, &pResult))
	{
		if (RecDeal(AFN10, F111, pResult, NULL))
		{
			pHead = (T376_2RxFm0 *)pResult;
			pBuf = (BYTE *)(pHead + 1);
			memcpy(pData, pBuf, pHead->wLen - sizeof(T376_2RxFm0) - sizeof(T376_2FmTail));
			return TRUE;
		}
	}
	return FALSE;
}
#endif