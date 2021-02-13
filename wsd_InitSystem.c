#include "wsd_Flat.h"
#include "wsd_record.h"


TPLCinfo *pPlcinfo;

static void GetPlcMPinfo(void);

void GetSysCfg(void)
{
	GetPlcMPinfo();
}

static void String2Dword(char* Desc, DWORD *pdwValue, BYTE byNum)
{
	char szDetails[100];
	char szTmp[13];
	BYTE byI, byLen = 0, byNo = 0;

	for (byI = 0; byI < byNum; byI++)
	{
		memset(szDetails, 0, 100);
		GetSpecifyString(Desc, "-", byI, szDetails, &byLen);
		byLen = min(12, byLen);
		if (byLen > 8)
		{
			memset(szTmp, 0, 8);
			memcpy(szTmp, szDetails, byLen - 8);
			pdwValue[byNo++] = DWBin2Bcd(atoi(szTmp));
			memset(szTmp, 0, 8);
			memcpy(szTmp, &szDetails[byLen - 8], 8);
			pdwValue[byNo++] = DWBin2Bcd(atoi(szTmp));
		}
		else
		{
			memset(szTmp, 0, 8);
			memcpy(szTmp, szDetails, byLen);
			pdwValue[byNo++] = DWBin2Bcd(atoi(szTmp));
		}
	}
}

BOOL WriteMPinfo(DWORD *pData)
{
	WORD wI;
	TMPRecord *pRec;

	for (wI = 0; wI < MAX_MP_NUM; wI++)
	{
		if ((pPlcinfo->TRecord[wI].byValid&BIT0) == 0)
			break;
	}
	if (wI >= MAX_MP_NUM)
	{
		Trace("The number of devices exceeds the maximum number");
		return FALSE;
	}
	pRec = &pPlcinfo->TRecord[wI];
	pRec->wAddr = (WORD)pData[0];
	pRec->dwAddr = pData[1];
	pRec->byProtocol = (BYTE)pData[2];
	pRec->wMPNo = wI;
	memcpy(&pRec->byCollAddr[4], &pData[3], sizeof(WORD));
	memcpy(&pRec->byCollAddr[0], &pData[4], sizeof(DWORD));
	pRec->byValid |= BIT0;
	pRec->byFlag = 0;	//
}

WORD FreshMPInfo(TMPRecord *pMPinfo)
{
#if(MD_DEV_PLC)
	WORD wI;
	BYTE byI;
	TMPRecord *pRec;

	for (wI = 0; wI < MAX_MP_NUM; wI++)
	{
		if ((pPlcinfo->TRecord[wI].byValid&BIT0) == 0)
			continue;
		if ((pPlcinfo->TRecord[wI].dwAddr == pMPinfo->dwAddr)
			&& (pPlcinfo->TRecord[wI].wAddr == pMPinfo->wAddr))
		{
			for (byI = 0; byI < 6;byI++)
			{
				if (pPlcinfo->TRecord[wI].byCollAddr[byI] != pMPinfo->byCollAddr[byI])
					break;
			}
			if (byI >= 6)
				return wI;//地址和采集器地址已经存在
		}
	}
	for (wI = 0; wI < MAX_MP_NUM; wI++)
	{
		if ((pPlcinfo->TRecord[wI].byValid&BIT0) == 0)
			break;
	}
	if (wI >= MAX_MP_NUM)
	{
		Trace("The number of devices exceeds the maximum number");
		return 0xFFFF;
	}
	pRec = &pPlcinfo->TRecord[wI];
	pRec->wAddr = pMPinfo->wAddr;
	pRec->dwAddr = pMPinfo->dwAddr;
	pRec->byProtocol = pMPinfo->byProtocol;
	pRec->wMPNo = wI;
	memcpy(pRec->byCollAddr, pMPinfo->byCollAddr, 6);
	pRec->byValid |= BIT0;
	pRec->byFlag = 0;	//
	FreshIDPara(1);
	pPlcinfo->wMPNum++;
	Trace("MQTT addr is not exit, add this addr to MP[%d] ",wI);
	return wI;
#else
	return 0xFFFF;
#endif
//刷新端口
}



void GetPlcMPinfo(void)
{
	char line[200] = { 0 };
	DWORD dwValue[5];
	BYTE byI;
	int i = 1;

	memset(pPlcinfo->TRecord, 0, sizeof(pPlcinfo->TRecord));
	pPlcinfo->wMPNum = 0;
	while (ReadCfg(CFG_PATH, "[mater]", line, i))
	{
		Trace("%s", line);
		memset(dwValue, 0, sizeof(dwValue));
		String2Dword(line, dwValue, 3);
		if (WriteMPinfo(dwValue))
			pPlcinfo->wMPNum++;
		i++;
		memset(line, 0, 200);
	}
}

void InitPlcRecord(void)
{
	//初始化PLC测量点信息
	pPlcinfo = MemAlloc(sizeof(TPLCinfo));

//	GetSysCfg();
}


TMPRecord *pGetMP(WORD wMPNo)
{
	if (wMPNo > MAX_MP_NUM)
		return NULLP;
	return &pPlcinfo->TRecord[wMPNo];
}

WORD GetPlcNum(void)
{
	return pPlcinfo->wMPNum;
}

void InitSysPort(void)
{
	TPortCfg PC;
	TSerialCfg *pSC = &PC.Cfg.SerialCfg;
	BYTE byI;

#if(MD_DEV_PLC)
	memset(&PC, 0, sizeof(TPortCfg));
	PC.dwID = MDW(MPT_SERIAL, PORT_ZB, 1, 0);
	PC.wDriverID = MDEV_PLC;
	pSC->dwBaud = 9600; //波特率
	pSC->byCharSize = 8;
	pSC->dwFlags = 0x60;//偶校验
	PT_AddPort(&PC);
#endif
#if(MD_DEV_COM)
#if 0
	for (byI = 0; byI < MAX_485_NUM; byI++)
	{
		memset(&PC, 0, sizeof(TPortCfg));
		PC.dwID = MDW(MPT_SERIAL, byI, 1, 0);
		PC.wDriverID = MDEV_COM;
		pSC->dwBaud = 2400; //波特率
		pSC->byCharSize = 8;
		pSC->dwFlags = 0x60;//偶校验
		PT_AddPort(&PC);
	}
#endif
#endif
// 	memset(&PC, 0, sizeof(TPortCfg));
// 	PC.dwID = MDW(MPT_ETHERNET ,0, 1, 0);
// 	PC.wDriverID = MDEV_COM;
// 	pSC->dwBaud = 2400; //波特率
// 	pSC->byCharSize = 8;
// 	pSC->dwFlags = 0x60;//偶校验
// 	PT_AddPort(&PC);
}

BOOL IsNeedResetPort(void)
{

}

