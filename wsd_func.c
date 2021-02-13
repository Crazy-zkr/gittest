///////////////////////////////////////////////////////////////
//	文 件 名 : func.c
//	文件功能 : 
//	作    者 : 
//	创建时间 : 
//	项目名称 ：
//	操作系统 : 
//	备    注 :
//	历史记录 : 
///////////////////////////////////////////////////////////////
#include "wsd_Flat.h"
#include <string.h>

const BYTE BIT8_L1[] = {
	0x80, 0x40, 0x20, 0x10,
	0x8, 0x4, 0x2, 0x1 };

WORD MW(BYTE Hi, BYTE Lo)
{
	WORD x;
	BYTE *p = (BYTE *)&x;
	p[1] = Hi;
	p[0] = Lo;
	return x;
}
DWORD MDW(BYTE HH, BYTE HL, BYTE LH, BYTE LL)
{
	DWORD x;
	BYTE *p = (BYTE *)&x;
	p[3] = HH;
	p[2] = HL;
	p[1] = LH;
	p[0] = LL;
	return x;
}

DWORD MDW2(WORD H, WORD L)
{
	return MDW(HIBYTE(H), LOBYTE(H), HIBYTE(L), LOBYTE(L));
}

WORD SwapWord(WORD iw)
{
	WORD w;
	BYTE *sp = (BYTE *)&iw;
	BYTE *dp = (BYTE *)&w;

	dp[0] = sp[1];
	dp[1] = sp[0];
	return w;
}

DWORD SwapDWord(DWORD idw)
{
	DWORD dw;
	BYTE *sp = (BYTE *)&idw;
	BYTE *dp = (BYTE *)&dw;
	dp[0] = sp[3];
	dp[1] = sp[2];
	dp[2] = sp[1];
	dp[3] = sp[0];
	return dw;
}

void MySleep(DWORD dwMSecond)
{
	if (dwMSecond == 0)
		return;
	dwMSecond--;
	usleep(dwMSecond * 1000);
}

DWORD SearchHead1(HPARA hBuf, DWORD Len, DWORD Offset1, BYTE Key1)
{
	BYTE *Buf = (BYTE *)hBuf;
	DWORD i;
	for (i = 0; i<(DWORD)(Len - Offset1); i++)
	{
		if (Buf[Offset1 + i] == Key1)
			return i;
	}
	return i;
}

DWORD SearchHead2(HPARA hBuf, DWORD Len, DWORD Offset1, BYTE Key1, DWORD Offset2, BYTE Key2)
{
	BYTE *Buf = (BYTE *)hBuf;
	DWORD i;
	DWORD MLen;
	if (Offset1<Offset2)
		MLen = Offset2;
	else
		MLen = Offset1;

	for (i = 0; i<(DWORD)(Len - MLen); i++)
	{
		if (Buf[Offset1 + i] == Key1&&Buf[Offset2 + i] == Key2)
			return i;
	}
	return i;
}

BOOL CheckBCD(BYTE byIn)
{
	if ((byIn & 0x0f) > 0x09)
	{
		return FALSE;
	}
	if ((byIn >> 4) > 0x09)
	{
		return FALSE;
	}
	return TRUE;
}

BOOL IsAllBCD(BYTE *pBuf, BYTE bylen)
{
	BYTE byI;

	for (byI = 0; byI<bylen; byI++)
	{
		if (!CheckBCD(pBuf[byI]))
			return FALSE;
	}
	return TRUE;
}
//Bcd转换为Bin
BYTE   Bcd2Bin(BYTE byIn)
{//OK
	BYTE byOut;
	byOut = ((byIn >> 4) & 0xf) * 10 + (byIn & 0xf);
	return byOut;
}

WORD WBcd2Bin(WORD wIn)
{
	BYTE byHigh, byLow;
	WORD wOut;
	byHigh = HIBYTE(wIn);
	byLow = LOBYTE(wIn);
	wOut = ((byHigh >> 4) & 0xf) * 1000 + (byHigh & 0xf) * 100 + ((byLow >> 4) & 0xf) * 10 + (byLow & 0xf);
	return wOut;
}

DWORD DWBcd2Bin(DWORD dwIn)
{
BYTE HH, HL, LH, LL;
DWORD dwOut;
HH = HHBYTE(dwIn);
HL = HLBYTE(dwIn);
LH = LHBYTE(dwIn);
LL = LLBYTE(dwIn);
	//12345678
	dwOut =
		((HH >> 4) & 0x0F) * 10000000 +
		(HH & 0x0F) * 1000000 +
		((HL >> 4) & 0x0F) * 100000 +
		(HL & 0x0F) * 10000 +
		((LH >> 4) & 0x0F) * 1000 +
		(LH & 0x0F) * 100 +
		((LL >> 4) & 0x0F) * 10 +
		(LL & 0x0F);

	return dwOut;
}

//Bin转换为Bcd
BYTE   Bin2Bcd(BYTE byIn)
{
BYTE byOut;
BYTE d1, d2;
	d1 = byIn / 10;
	d2 = byIn % 10;
	byOut = d1;
	byOut <<= 4;
	byOut += d2;
	return byOut;
}

WORD  WBin2Bcd(WORD wIn)
{
WORD wOut;
WORD d[4];
int i;
	for (i = 3; i >= 0; i--)
	{
		d[i] = wIn % 10;
		wIn /= 10;
	}
	wOut = 0;
	for (i = 0; i<4; i++)
	{
		wOut <<= 4;
		wOut += d[i];
	}
	return wOut;
}

DWORD DWBin2Bcd(DWORD dwIn)
{
DWORD dwOut;
DWORD d[8];
int i;
	for (i = 7; i >= 0; i--)
	{
		d[i] = dwIn % 10;
		dwIn /= 10;
	}
	dwOut = 0;
	for (i = 0; i<8; i++)
	{
		dwOut <<= 4;
		dwOut += d[i];
	}
	return dwOut;
}

//BCD到十进制数
BYTE Bcd2Dec(BYTE byBcd)
{
	if ((byBcd >= '0') && (byBcd <= '9'))
	{
		return byBcd - '0';
	}

	if ((byBcd >= 'A') && (byBcd <= 'F'))
	{
		return byBcd - 'A' + 10;
	}

	if ((byBcd >= 'a') && (byBcd <= 'f'))
	{
		return byBcd - 'a' + 10;
	}
	return 0;
}

//十进制数到BCD
BYTE Dec2Bcd(BYTE byDec)
{
	if (byDec <= 9)
	{
		return byDec + '0';
	}

	if ((byDec >= 0xa) && (byDec <= 0x0f))
	{
		return byDec + 'A' - 10;
	}
	return '0';
}

int a2i(char *p, int MaxLen)
{
	//MaxLen:最大长度
	int iRc = 0;
	if (MaxLen == 0)
		MaxLen = 10000;
	while (*p && MaxLen--)
	{
		if (*p<'0' || *p>'9')
		{//非数字停止
			return iRc;
		}
		iRc *= 10;
		iRc += *p - '0';
		p++;
	}
	return iRc;
}

BYTE AscII2Bin(BYTE byData)
{
	if (byData<'0' || byData>'9')
	{
		return 0;
	}
	return byData - '0';
}


DWORD ASCII2INTEX(char *p, DWORD MaxLen)
{
	//MaxLen:最大长度
	DWORD iRc = 0;

	if (MaxLen == 0)
		MaxLen = 100;
	while (*p && (MaxLen--))
	{
		if ((*p >= '0') && (*p <= '9'))
		{//非数字停止
			iRc *= 10;
			iRc += *p - '0';
		}
		p++;
	}
	return iRc;
}

void* MemAlloc(DWORD dwMemSize)
{
BYTE *pbyMem;
DWORD i;
	
//	Trace("malloc size is %d", dwMemSize);
	pbyMem = (BYTE *)malloc(dwMemSize);
	for (i = 0; i<dwMemSize; i++)
		pbyMem[i] = 0;
	return (void*)pbyMem;
}

void MemFree(void *pMem)
{	
//	Trace("free size is %d", *(int*)((char*)pMem - 16));
	free(pMem);
	pMem = NULLP;
}

BYTE CheckSum(HPARA pBuf, DWORD dwLen)
{
BYTE	*p = (BYTE *)pBuf;
BYTE	cSum = 0;
DWORD	i;

	for (i = 0; i<dwLen; i++)
	{
		cSum += p[i];
	}
	return cSum;
}

WORD Crc16(HPARA pBuf, DWORD len)
{
	WORD rc = 0xffff;
	BYTE *p = (BYTE *)pBuf;
	BYTE i, j;

	for (i = 0; i < len; i++)
	{
		rc ^= p[i];
		for (j = 0; j < 8; j++)
		{
			if (rc & 1)
				rc = (rc >> 1) ^ 0xa001;
			else
				rc >>= 1;
		}
	}
	return rc;
}

BOOL GetSpecifyString(char* szString, char* szCode, BYTE byNum, char* szResult, BYTE *pbyLen)
{
	char *pCfg = szString;
	char *strFind;
	BYTE byNo, byLen;

	if ((NULLP == pCfg) || (szResult == NULLP) || (szCode == NULLP))
		return FALSE;
	for (byNo = 0; byNo <= byNum; byNo++)
	{
		strFind = strstr(pCfg, szCode);
		if (strFind == NULLP)
		{
			if (byNo == byNum)
			{
				strcpy(szResult, pCfg);
				byLen = strlen(szResult);
			}
			else
				szResult[0] = 0;
			break;
		}
		byLen = (strFind - pCfg);
		memcpy((HPARA)szResult, (HPARA)pCfg, byLen);
		szResult[byLen] = 0;
		pCfg = strFind + strlen(szCode);
	}
	*pbyLen = byLen;
	return TRUE;
}

void InvertBuf(WORD wBufLen, BYTE *pBuf)
{
	WORD i, L;
	BYTE b;

	L = wBufLen / 2;
	for (i = 0; i < L; i++)
	{
		b = pBuf[i];
		pBuf[i] = pBuf[wBufLen - i - 1];
		pBuf[wBufLen - i - 1] = b;
	}
}



void byte2str(BYTE *pBuf, WORD wLen, char *message)
{
	WORD wI;
	char string[2];

	for (wI = 0; wI < wLen; wI++)
	{
		sprintf(&message[wI * 2], "%02X", pBuf[wI]);
	}
}


void str2byte(BYTE *pBuf, WORD *pwLen, char *message, WORD wLen)
{
	WORD wI, wLenEx = 0;
	BYTE byTmp, byI = 0, byTmpEx, bywee;

	byTmp = 0;
	for (wI = 0; wI < wLen; wI++)
	{
		byTmp *= 0x10;
		if ((*message <= 'F') && (*message >= 'A'))
			byTmpEx = (*message - 'A' + 10);
		else if ((*message <= 'f') && (*message >= 'a'))
			byTmpEx = (*message - 'a' + 10);
		else if ((*message <= '9') && (*message >= '0'))
			byTmpEx = (*message - '0');
		else
			return;
		byTmp += byTmpEx;
		byTmpEx = 0;
		message++;
		if (wI % 2)
		{
			pBuf[wLenEx++] = byTmp;
			byTmp = 0;
		}
	}
	*pwLen = wLenEx;
}

static WORD _GetTextPosition(char* szStr, char* szResult)
{//找到字符串的偏移
	char *pStr = szStr;
	char *pST;

	if ((pStr == NULL) || (szResult == NULL))
		return 0xFFFF;
	pST = strstr(pStr, szResult);
	if (pST)
		return (pST - pStr);
	return 0xFFFF;
}

BOOL GetMiddleStr(char* szString, char* szStart, char* szEnd, char* szResult)
{//查找夹在指定字符串头尾的字符数据
	WORD wLen1, wLen2;

	wLen1 = _GetTextPosition(szString, szStart);
	if (wLen1 != 0xFFFF)
	{
		wLen1 += strlen(szStart);
		wLen2 = _GetTextPosition(&szString[wLen1], szEnd);
		if (wLen2 != 0xFFFF)
		{
			memcpy((HPARA)szResult, &szString[wLen1], (wLen2));
			szResult[wLen2] = 0;
			return TRUE;
		}
	}
	szResult[0] = 0;
	return FALSE;
}

DWORD ASCII2INT(char *p, DWORD MaxLen)
{
	//MaxLen:最大长度
	DWORD iRc = 0;

	if (MaxLen == 0)
		MaxLen = 100;
	while (*p && (MaxLen--))
	{
		if ((*p<'0') || (*p>'9'))
		{//非数字停止
			return iRc;
		}
		iRc *= 10;
		iRc += *p - '0';
		p++;
	}
	return iRc;
}
int SearchValue(char* szString, char* szStart, char* szEnd)
{//查找夹在指定字符串头尾的字符数据并将之转换成16进制数据
	char szTmp[50];

	if (!GetMiddleStr(szString, szStart, szEnd, szTmp))
		return -1;

	return ASCII2INT(szTmp, 8);
}




