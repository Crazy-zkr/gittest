#include "wsd_1376_2.h"
#include "wsd_cgy.h"
#include "wsd_1376_2_fun.h"
#if(MD_DEV_PLC)
extern TGy *pGyRouter;

///////////////////////////////////////////////////////////////
//	�� �� �� : IsFirstOfCll
//	�������� : �жϸò������Ƿ�Ϊ���ڲɼ����µĵ�һ��
//	������� : 
//	��    ע : 
//	��    �� : �Ժ��
//	ʱ    �� : 2015��4��7��
//	�� �� ֵ : BOOL
//	����˵�� : TMP *pMP
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
//	�� �� �� : AddIDPara
//	�������� : ��·����Ӵӽڵ���Ϣ
//	������� : 
//	��    ע : 
//	��    �� : �Ժ��
//	ʱ    �� : 2015��4��2��
//	�� �� ֵ : WORD �ɹ���ӵĽڵ�����
//	����˵�� : BOOL bOnlyNew�� 1-ֻ��������ڵ� 0-������нڵ�
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

	//������дӽڵ�
	if (bOnlyNew == 0)
		ClearRouterID698();
	//������׼��
	pRTIDPtr = &RTID.tID[0];
	//��װ���дӽڵ�
	for (wNo = 0; wNo<MAX_MP_NUM; wNo++)
	{
		pMP = pGetMP(wNo);
		if (!IsValidMeter(pMP))
			continue;
		if (IsVirtualMP(pMP->dwAddr, pMP->wAddr))
			continue;
		if (bOnlyNew)
		{//ֻ��������ڵ�
			if (pMP->byFlag&BIT0)
				continue;
		}
		byRule = (pMP->byProtocol == MGY_645_2007) ? 2 : 1;
		if (!IsNullBuf(pMP->byCollAddr, 6))
		{//�вɼ���
			if (!IsFirstOfCll(pMP))
				continue;
			memcpy(pRTIDPtr->byAddr, pMP->byCollAddr, 6);
			pRTIDPtr->byRule = byRule;
			pRTIDPtr++;
			byCnt++;
			wOkNum++;
			Trace("ͬ���ɼ�����ַ;%02x%02x%02x%02x%02x%02x", RTID.tID->byAddr[0], RTID.tID->byAddr[1], RTID.tID->byAddr[2], RTID.tID->byAddr[3], RTID.tID->byAddr[4], RTID.tID->byAddr[5]);
		}
		else
		{//�ز���
			MDDWW2Byte(pMP->dwAddr, pMP->wAddr, byBuf);
			memcpy(pRTIDPtr->byAddr, byBuf, 6);
			pRTIDPtr->byRule = byRule;
			pRTIDPtr++;
			byCnt++;
			wOkNum++;
			Trace("ͬ���豸��ַ;%02x%02x%02x%02x%02x%02x", RTID.tID->byAddr[0], RTID.tID->byAddr[1], RTID.tID->byAddr[2], RTID.tID->byAddr[3], RTID.tID->byAddr[4], RTID.tID->byAddr[5]);
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
//	�� �� �� : CheckIDPara
//	�������� : ���ӽڵ����
//	������� : ɾ������ڵ㲹��ȱʧ�ڵ�
//	��    ע : 2013.9��Ϊ���->ͬ��ģʽ
//	��    ע : 2015.4��Ϊ���->���ɾ��ģʽ
//			   2015.7.22 �����������߼�Ϊ������ִ��10-F1 10-F2��ѯ��Ȼ����ڵ���Ϣ���ٴ����ɾ����
//			   ����ӽڵ㣬�����账�����˳��ú���
//	��    �� : �Ժ��
//	ʱ    �� : 2009��11��6��
//	�� �� ֵ : BOOL
//	����˵�� : void 
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

	//����Ҫ�ڵ㵵����ģ��(��˹��)
	if ((pRInfo->bySubMode == 0) && (pRInfo->byCBMode == 1))
		return TRUE;

	//��ͣ·��
	if (p->byRouterStart)
		StopRouter();

	if (wIDNum == 0)//����������Ϊ��,���·��
		return ClearRouterID698();

	//�ڵ��鼰���ɾ��(���3��)
	for (byRetry = 0; byRetry<MAX_CHECKIDTIMES; byRetry++)
	{
		//Ĭ�ϵ����ޱ䶯
		bChanged = FALSE;

		//���������ͬ����־
		for (wNo = 0; wNo<MAX_MP_NUM; wNo++)
		{
			pMP = pGetMP(wNo);
			if (NULLP != pMP)
				pMP->byFlag &= (~BIT0);
		}

		//��մ�ɾ����ַ�б�
		p->wDelNum = 0;
		memset(&p->DelAddr[0][0], 0, sizeof(p->DelAddr));

		//��ȡ�ڵ�����
		if (!CheckIDNum())
			return FALSE;

		//1�����ж�·�ɽڵ����
		if (pRInfo->wIDNum != 0)
		{
			//����������������·�ɵ���������һ��
			if (pRInfo->wIDNum != wIDNum)
				bChanged = TRUE;

			//2�����·�ɴӽڵ���Ϣ
			byFrams = (BYTE)((pRInfo->wIDNum + MAX_NODENUM_FM - 1) / MAX_NODENUM_FM);
			for (byIndex = 0; byIndex<byFrams; byIndex++)
			{
				wStartSN = (WORD)(byIndex*MAX_NODENUM_FM + 1);
				if (byIndex == (byFrams - 1))
					byReadNum = (BYTE)(pRInfo->wIDNum - MAX_NODENUM_FM*byIndex);
				else
					byReadNum = MAX_NODENUM_FM;

				//ÿ����10���ڵ�
				if (!TxAfn10_F2(wStartSN, byReadNum, byAddr, &byRecNum))
				{
					return FALSE;
				}

				//3������·���ϱ��Ľڵ��ַ
				for (byI = 0; byI<byRecNum; byI++)
				{
					MDByte2DWW(&byAddr[byI * 6], &dwAddr, &wAddrEx);

					if (pMP = pGetMPEx(dwAddr, wAddrEx))
					{//�õ�ַ�ڵ����д���
						pMP->byFlag |= BIT0;
					}
					else
					{//�õ�ַΪ�Ƿ���ַ�������������в�����
						if (p->wDelNum < MAX_DEL_NUM)
						{//�����ɾ����ַ�б�
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
			//ɾ���Ƿ��ڵ�
			if (p->wDelNum != 0)
				bChanged = TRUE;
		}
		else
			bChanged = TRUE;

		//4��������������
		if (bChanged == TRUE)
		{
			if (p->wDelNum != 0)
			{
				//��ɾ��������·������һ�£����ʼ��·�ɣ�Ȼ���������			
				if ((p->wDelNum == pRInfo->wIDNum) || (p->wDelNum >= MAX_DEL_NUM))
					AddIDPara(0);
				else
				{
					for (wNo = 0; wNo<p->wDelNum; wNo++)
					{
						MDByte2DWW(&p->DelAddr[wNo][0], &dwAddr, &wAddrEx);
						if (!TxAfn11_F2(dwAddr, wAddrEx))//ɾ������ʧ��תΪ������ģʽ
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
		{//5��������ϣ����³���
			break;
		}
	}
	//�������ɾ��δ��ʵ��ͬ��
	if (byRetry >= MAX_CHECKIDTIMES)
	{
		AddIDPara(0);
	}
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	�� �� �� : StaIDNum
//	�������� : ���ݼ�����������Ϣͳ��·�ɽڵ�����
//	������� : 
//	��    ע : ����У׼·��ģ��ڵ���Ϣ
//	��    �� : Administrator
//	ʱ    �� : 2011��8��24��
//	�� �� ֵ : WORD
//	����˵�� : void
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
//	�� �� �� : CheckVerInfo
//	�������� : ��ѯ·�ɰ汾��Ϣ
//	������� : 
//	��    ע : 
//	��    �� : �Ժ��
//	ʱ    �� : 2009��11��6��
//	�� �� ֵ : BOOL
//	����˵�� : void
///////////////////////////////////////////////////////////////
BOOL CheckVerInfo(void)
{
	return TxAfn03_F10();
}

///////////////////////////////////////////////////////////////
//	�� �� �� : CheckMainID
//	�������� : ������ڵ��ַ
//	������� : 
//	��    ע : �뼯������ַ��һ��ʱǿ������
//	��    �� : �Ժ��
//	ʱ    �� : 2009��11��6��
//	�� �� ֵ : BOOL
//	����˵�� : void
///////////////////////////////////////////////////////////////
BOOL CheckMainID(void)
{
	return TxAfn03_F4();
}

///////////////////////////////////////////////////////////////
//	�� �� �� : CheckIDNum
//	�������� : ��ѯ�ز��ڵ�����
//	������� : 
//	��    ע : 
//	��    �� : �Ժ��
//	ʱ    �� : 2009��11��6��
//	�� �� ֵ : BOOL
//	����˵�� : void
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
//	�� �� �� : ChkStudySta
//	�������� : ��ѯ·������״̬
//	������� : 
//	��    ע : �����ж�ѧϰ���������Ƿ����
//	��    �� : Administrator
//	ʱ    �� : 2012��12��26��
//	�� �� ֵ : BOOL
//	����˵�� : void
///////////////////////////////////////////////////////////////
BOOL ChkStudySta(void)
{
	TRouter *p = (TRouter *)GethApp();
	TRouterInfo *pRInfo = GetRouterInfo();
	WORD wIDNum = StaIDNum();
	BOOL bRc;

	bRc = TxAfn10_F4();
	//�ڵ���������
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

	//΢��������������ͨ������
	if ((pRInfo->byCommMode == MICROPOWER_WIRELESS) || (pRInfo->byCommMode == PLC_WIDEBAND))
	{
		p->bStuFirst = TRUE; //�������󳭱� zhj 2014.9.15
		return TRUE; //TxAfn05_F5(254,255);	//�����ŵ���
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
//	�� �� �� : IsWorkTime
//	�������� : 
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 2011��4��25��
//	�� �� ֵ : BOOL
//	����˵�� : void
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
//	�� �� �� : ReverseCpy
//	�������� : �����뻺�����ڵ��ֽڷ������������
//	��    ע : �������������Ϊͬһ���ڴ�
//	��    �� : �Ժ��
//	ʱ    �� : 2009��11��6��
//	�� �� ֵ : void
//	����˵�� : BYTE* outBuf,BYTE* inBuf,BYTE byLen
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
//	�� �� �� : *pGetMPEx
//	�������� : ���ݵ�ַ��ȡ������
//	������� : 
//	��    ע : ע�⣺�ú���ֻ���ز��˿����ã��Ա�֤��ַ���ظ�!
//	��    �� : Administrator
//	ʱ    �� : 2011��6��29��
//	�� �� ֵ : TMP
//	����˵�� : DWORD dwAddr,WORD wAddrEx
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
//	�� �� �� : RT698_PortRxClear
//	�������� : ���·�ɶ˿ڽ��ջ�����
//	������� :
//	��    ע : 
//	��    �� : 
//	ʱ    �� :
//	�� �� ֵ : void
//	����˵�� : TPort *pPort
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
				if (AFN06 == *pBuf)//�ȴ�����������
				{
					pBuf++;	//����AFN
					wFn = DT2Fn(*pBuf, *(pBuf + 1));
					pBuf += 2;	//����Fn
					if (F5 == wFn)
					{
						pBuf++;//�����ӽڵ��豸����
						pBuf++;//����ͨ��Э������
						wLen = (WORD)(*pBuf);
						if (wLen > 128)
							wLen = 128;
						pBuf++;//�������ݳ���
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
//	�� �� �� : RT698_TxDirect
//	�������� : ����·�ɶ˿�ֱ�ӷ��ͺ���
//	������� :
//	��    ע : 
//	��    �� : 
//	ʱ    �� :
//	�� �� ֵ : void
//	����˵�� : void
///////////////////////////////////////////////////////////////
void RT698_TxDirect(void)
{
	TPort *pPort = GetRouterPort();
	TTx	*pTx = GetRouterTx();

	FrameTrace("Tx:", pTx->pBuf, pTx->dwWptr);
	pPort->fWrite(pPort, pTx);
	pPort->byStatus = MPS_TX; //��ʼ����
	pPort->dwCtrlTime = GetRunCount();
}

///////////////////////////////////////////////////////////////
//	�� �� �� : SearchOneFrameEx
//	�������� : ����֡��ʽ
//	������� : 
//	��    ע : 
//	��    �� : �Ժ��
//	ʱ    �� : 2009��11��6��
//	�� �� ֵ : DWORD
//	����˵�� : HPARA hBuf,DWORD dwLen
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
	//����֡���ж�
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
//	�� �� �� : RxWait698_42
//	�������� : ����698��Ӧ֡
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 2009��11��7��
//	�� �� ֵ : BYTE*			-���յ�����Ӧ����ָ��
//	����˵�� : DWORD dwTime,	-���յȴ�ʱ��
//				 BYTE* pResult,	-ִ�н��[��ȷ/����/����Ӧ]
//				 DWORD* pdwLen	-���յ�����Ӧ���ĳ���
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
	dwTime /= 10; //ת��Tick����
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
		pRx->dwBusy = GetRunCount();	//��ֹ��׼RxMonitor������Ϊ�ַ���ʱ
		Rc = SearchOneFrameEx(&pRx->pBuf[pRx->dwRptr], (DWORD)MsgLen);
		Len = Rc & ~FM_SHIELD; //�Ѵ�������ֽ���
		switch (Rc & FM_SHIELD)
		{
		case FM_OK://�㲥����
			*pResult = MAT_OK;
			*pdwLen = Len;
			FrameTrace("Rx:", pRx->pBuf, pRx->dwWptr);
			//�����ŵ���ʶ
			pRxFm = (T376_2RxFm *)&pRx->pBuf[pRx->dwRptr];
			p->byChanel = (pRxFm->byInfoChannel & 0x0F) % 4;
			dwTmpRPtr = pRx->dwRptr;
			pRx->dwRptr = pRx->dwWptr;
			pBuf = (BYTE *)(pRxFm + 1);
			if (pPort == GethPort())
			{
				pBuf += DealInfoField(pRxFm);
				if (AFN06 == *pBuf)//ͬ���������������¼��ϱ�����
				{
					pBuf++;	//����AFN
					wFn = DT2Fn(*pBuf, *(pBuf + 1));
					pBuf += 2;	//����Fn
					if (F5 == wFn)
					{
						pBuf++;//�����ӽڵ��豸����
						pBuf++;//����ͨ��Э������
						wLen = (WORD)(*pBuf);
						if (wLen > 128)
							wLen = 128;
						pBuf++;//�������ݳ���
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
				pRx->dwRptr += Len; //ָ���Ƶ���һ���Ĵ�
			FrameTrace("Rx[err]:", pRx->pBuf, pRx->dwWptr);
			break;
		case FM_LESS:
			if (Len)
				pRx->dwRptr += Len; //ָ���Ƶ���һ���Ĵ�
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
//	�� �� �� : Tx_698Fm0
//	�������� : ��698֡
//	������� :
//	��    ע : ��·��ģ��ͨ��
//	��    �� : 
//	ʱ    �� : 2009��11��6��
//	�� �� ֵ : void
//	����˵�� : BYTE byAfn,WORD wFn,BYTE* pData,WORD wLen
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
		pTxFm->byCtrl = p->byModuleType & 0x3F;	//���дӶ�
		break;
	default:
		pTxFm->byCtrl = (p->byModuleType & 0x3F) | BIT6;	//��������
		break;
	}
	pTxFm->byInfoModule = 0;					//·��ģʽ���޸��ӽڵ㣬�Լ�������ͨѶģ�����
	pTxFm->byInfoChannel = p->byChanel;			//�ŵ���ʶ
	pTxFm->byInfoAskLen = 160;					//Ԥ��Ӧ���ֽ���
	pTxFm->wInfoBaud = 0;					//Ĭ��50 bps
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
//	�� �� �� : Tx_698Fm1
//	�������� : ��698֡
//	������� :
//	��    ע : ���ز��ӽڵ�ͨ��
//	��    �� : 
//	ʱ    �� : 2009��11��6��
//	�� �� ֵ : void
//	����˵�� : BYTE byAfn,WORD wFn,BYTE* pData,BYTE byLen
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
		pTxFm->byCtrl = p->byModuleType & 0x3F; //���дӶ�
		break;
	default:
		pTxFm->byCtrl = (p->byModuleType & 0x3F) | BIT6;	  //��������
		break;
	}

	pTxFm->byInfoModule = ((byRelay << 4) | 0x04);  //·��ģʽ���޸��ӽڵ㣬�Դӽڵ��ͨѶģ�����
	if (byAfn == AFN02)
		pTxFm->byInfoModule |= 0x01; //����·�ɻ�������·ģʽ

	if ((byAfn == AFN13) || (byAfn == AFN02))
		p->byChanel = 0;
	pTxFm->byInfoChannel = p->byChanel;
	pTxFm->byInfoAskLen = 160;	//Ԥ��Ӧ���ֽ���;
	pTxFm->wInfoBaud = 0;					//Ĭ��50 bps
	pTxFm->bySeq = p->bySeq++;
	//Դ��ַ
	ReverseCpy(&pBuf[byNo], pRInfo->szMainNet, 6);
	byNo += 6;
	//�м̵�ַ
	memcpy(&pBuf[byNo], pbyRelayAddr, byRelay * 6);
	byNo += (byRelay * 6);
	//Ŀ�ĵ�ַ
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
//	�� �� �� : TxCmd0
//	�������� : ����698����ȴ���Ӧ
//	������� : ��Ӧ��ʱ���ط�����/��Ӧ�ı���ָ����szPtr����
//	��    ע : �뼯����ͨ��ģ��ͨ��
//	��    �� : 
//	ʱ    �� : 2009��11��7��
//	�� �� ֵ : BOOL
//	����˵�� : BYTE byAfn, WORD wFn,		-�·������Afn/Fn
//			BYTE* pData, WORD wLen,			-�·������������
//			DWORD dwDelay,BYTE byReTimes,	-�ȴ���Ӧʱ��/�ط�����
//			BYTE** szPtr					-��Ӧ�ı���ָ��
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
//	�� �� �� : IsStudyFinish
//	�������� : ����Ƿ�ѧϰ���
//	������� : 
//	��    ע : 
//	��    �� : ��˳
//	ʱ    �� : 2015��03��14��
//	�� �� ֵ : BOOL
//	����˵�� : void
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