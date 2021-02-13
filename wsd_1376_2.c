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
BYTE *pTransBuf; //͸��ת��������
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
//	�� �� �� : TK_376_2
//	�������� : �ز�·��376.2Э��
//	������� :
//	��    ע : 
//	��    �� : �Ժ��
//	ʱ    �� : 2009��11��6��
//	�� �� ֵ : void
//	����˵�� : void
///////////////////////////////////////////////////////////////
void TK_376_2(void)
{
	StdTaskEntry(Init, sizeof(TRouter));
}

///////////////////////////////////////////////////////////////
//	�� �� �� : Init
//	�������� : ��ʼ��
//	��    ע : 
//	��    �� : �Ժ��
//	ʱ    �� : 2009��11��6��
//	�� �� ֵ : BOOL
//	����˵�� : void
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
	pGyCfg->wTxdIdleTime = 800;	//����֡�뷢��֡���
	pGyCfg->wCommIdleTime = 500;	//�ַ������ 
	pGyCfg->wRxDelayMax = MAX_FRAME_DEALY;	//�Է���ȴ�ʱ��
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
	p->byModuleType = 1;//Ĭ��Ϊ�ز�ģ��
	gbyDeamonFlag = 0;
	p->byRetryCnt = 0;
	p->wMPNum = GetPlcNum();
	OpenRouter();
	InitMainID();
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	�� �� �� : SearchOneFrame
//	�������� : ����֡��ʽ
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 2009��11��6��
//	�� �� ֵ : DWORD
//	����˵�� : HPARA hBuf,DWORD dwLen
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
	dwFmLen = pRxFm->wLen;	//����֡�����Ƿ�ϸ�
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
	pGyCfg->wRxDelayMax = MAX_FRAME_DEALY; //�Է���ȴ�ʱ��
	return FM_OK | dwFmLen;
}

///////////////////////////////////////////////////////////////
//	�� �� �� : AdaptCommuIdleTime
//	�������� : ����ģ����շ����ʱ�䣬������Լ���ַ��ȴ����
//	������� : 
//	��    ע : 
//	��    �� : ���ɴ�
//	ʱ    �� : 2016��3��7��
//	�� �� ֵ : void
//	����˵�� : DWORD dwTime
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
	{//����һ�ν�����ȣ����ʱ�䳬��1���ӣ��ָ�Ĭ��ֵ
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
	{//�����յ�(MAX_RXTIME_NUM-1)֡�󣬽��м���ж�
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
		{//����7֡С�ڹ�Լ���ַ��ȴ��������Ҫ���е���
			wIdleTime = (WORD)(dwSumTime / byNum);
			wIdleTime = (wIdleTime / 50) * 50;
			if (wIdleTime < 100)
				wIdleTime = 100;
			else if (wIdleTime > 500)
				wIdleTime = 500;
			//Trace("���й�Լ�������[%d]-[%d]",pGyCfg->wCommIdleTime,wIdleTime);
			pGyCfg->wCommIdleTime = wIdleTime;
		}
		memset(p->adwRxMonitorInTime, 0, sizeof(p->adwRxMonitorInTime));
	}
}

///////////////////////////////////////////////////////////////
//	�� �� �� : TxMonitor
//	�������� : ���մ���
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : void
//	����˵�� : void
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
	case MST_RTCHK: //������ʼ��״̬
		if (!RouterChk())
		{
			break;
		}
		p->byInitErrCount = 0;
		p->byStatus = MST_INIT;
	case MST_INIT:	//·�ɳ�ʼ��(����ͬ���ڴ˲����)
		if (RouterInit())
		{
			pRInfo->byWork = RTWORK_IDLE;
			p->byRouterStart = FALSE;
			p->byStatus = MST_REST;
			gbyDeamonFlag = 0;
			p->byInitErrCount = 0;
		}
		else
		{//�쳣��ʼ�����
			p->byInitErrCount++;
			//����ģ�����³�ʼ��
			if (p->byInitErrCount > 10)
			{//��λ�ع鵽��ʼ��
				OpenRouter();
			}
			break;
		}
	case MST_REST:
		if (IsWorkTime())
		{
			//·�������ɹ����빤��״̬
			if (gbyDeamonFlag)
			{
				TGyCfg *pGyCfg = GetGyCfg();
				pGyCfg->wRxDelayMax = MAX_FRAME_DEALY; //�Է���ȴ�ʱ��
				p->byInitStep = INISTEP_CHKIDPARA;
				p->byStatus = MST_INIT;
			}
			else
			{
				bRc = StartRouter();
				if (bRc)
				{//���ν��볭���ʱ��				
					p->dwRxDataCount = 0;
					gbyDeamonFlag = 0;
					pRInfo->byWork = RTWORK_IDLE;
					p->dwPauseTmStamp = 0;
					p->byStatus = MST_WORK;
					if (p->bStuFirst && (GetPlcNum()!=0))
					{//����ģ��Ҫ���������󳭱� 
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
				{//���¼��ģ��
					p->byInitErrCount++;
					if (p->byInitErrCount > 10)
					{//��λ�ع鵽��ʼ��
						TGyCfg *pGyCfg = GetGyCfg();
						pGyCfg->wRxDelayMax = MAX_FRAME_DEALY; //�Է���ȴ�ʱ��					
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
			pGyCfg->wRxDelayMax = MAX_FRAME_DEALY; //�Է���ȴ�ʱ��
			StopRouter();
			p->byStatus = MST_REST;
			pRInfo->byWork = RTWORK_IDLE;

			if ((p->wMPNum && (p->dwRxDataCount == 0)))
			{//1����Ч����û��û���յ�����
				OpenRouter();
			}
			return;
		}
		if ((DWORD)(GetRunCount() - p->dwLastRxTime) >= 10800000)
		{//3Сʱû��ͨ��,�ر�ģ������
			OpenRouter();
			p->dwLastRxTime = GetRunCount();
			return;
		}
		if (gbyDeamonFlag)//��INISTEP_CHKIDPARA��ɾ��
		{//1��������������ͬ��
			DWORD dwDelay = GetRunCount() - gdwFreshTime;
			TGyCfg *pGyCfg = GetGyCfg();
			pGyCfg->wRxDelayMax = MAX_FRAME_DEALY; //�Է���ȴ�ʱ��
			if (dwDelay >= 38000)
			{
				StopRouter();
				p->byInitStep = INISTEP_CHKIDPARA;
				p->byStatus = MST_INIT;
			}
			return;
		}
		if ((DWORD)(GetRunCount() - p->dwLastRxTime) >= 20 * 60 * 1000) //zhj 2014.11.12
		{//20������ͨ�Ų�ѯ·��״̬		
			if ((DWORD)(GetRunCount() - p->dwLastReqStatus) > 1800000)
			{//�״�50���ӣ��Ժ���30���ӷ���һ��
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
		if ((DWORD)(dwTime - dwOldTime) > 10)//30���ѯһ��
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
			//�ж�����ʱ��
			if (p->byStatus != MST_WORK)
			{
				dwTime = GetSecs();
				if (dwTime < pRInfo->dwNetBuildSTime)
				{
					if ((dwTime / 86400) == (pRInfo->dwNetBuildSTime / 86400))	//Уʱǰ����ͬһ�첻��λ·��
						bResetRouter = FALSE;
					else
						bResetRouter = TRUE;
				}
				else
				{
					if ((DWORD)(dwTime - pRInfo->dwNetBuildSTime) > 18000)		//�5��Сʱ
						bResetRouter = TRUE;
					else
						bResetRouter = FALSE;
				}
				if (bResetRouter == TRUE)
				{//��λ·��ģ��
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
//	�� �� �� : RxMonitor
//	�������� : ���մ���
//	������� : 
//	��    ע : 
//	��    �� : �Ժ��
//	ʱ    �� : 2009��11��6��
//	�� �� ֵ : void
//	����˵�� : void
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
		if (wFn == F10) //�����ϱ�����ģʽ��Ϣ
		{
			RxAfn03_F10(&pBuf[wLen], pRxFm->wLen - 15);
			p->bySeq = pRxFm->bySeq;
			p->bLogon = TRUE;
			TxAfn00_F1(0xFFFFFFFF, 0);
			FreshRTSta(RTST_IDENTIFYOK, NULLP, NULLP, NULLP);
			//ģ�鸴�鷢��
			if (p->byStatus != MST_RTCHK)
				Trace("ģ���쳣,�ز�ģ����ָ�λ!!!");
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
		if (wFn == F1)	//���󳭶�����
			TC_QueryDI(dwAddr, wAddrEx);
		if (wFn == F2) //����ʱ��
			TxAfn14_F2();
		if (wFn == F3) //������ͨ����ʱ����ͨ������
		{
			//!!
		}
		break;
	case AFN06:
		p->bySeq = pRxFm->bySeq;
		if (wFn == F1) //�ӽڵ���Ϣ�����ϱ� 
		{
			FreshRTSta(RTST_SEARCH, 0, 8, 2);
			TxAfn00_F1(0xFFFFFFFF, 0);
			break;
		}
		if (wFn == F2) //���������ϱ�
		{
			SendFm(PORT_ZB,&pBuf[wLen + 6], pBuf[wLen + 5],0x06f2,0,TRUE);
			TxAfn00_F1(0xFFFFFFFF, 0);
			break;
		}
		if (wFn == F3) //�ϱ������䶯
		{
			TxAfn00_F1(0xFFFFFFFF, 0); //�ظ�ȷ��
			if (pBuf[wLen] == 1)
			{//�����������
				FreshRTSta(RTST_WORK, 0, 7, 1);
			}
			if (pBuf[wLen] == 2)
			{//�ѱ����			
				FreshRTSta(RTST_SEARCH, 0, 7, 2);
			}
			break;
		}
		if (wFn == F4) //�ϱ��ӽڵ���Ϣ
		{
			FreshRTSta(RTST_SEARCH, 0, 8, 2);
			TxAfn00_F1(0xFFFFFFFF, 0);
			break;
		}
		if (wFn == F5) //�ϱ��¼�
		{
			DealAfn06_F5(&pBuf[wLen]); //����·�������ϱ����ܱ��¼�
			TxAfn00_F1(0xFFFFFFFF, 0);
			break;
		}
		break;
	case AFN02:
		break;
	case AFN13: //�����������ݷ���
		if (wFn == F1)
		{
			//AFN13 F1������������
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
		if (wFn == F2 && byErr == 12) //�ڵ㲻����
		{
			p->byRetryCnt = 100;
		}
		if (wFn == F2 && byErr == 9) //CACæ
		{
			PauseTx(60); //������ͣ120��
		}
		if (wFn == F2 && byErr == 11) //�ӽڵ���Ӧ��
		{
			p->dwPauseTmStamp = 0;
		}
	}
	p->byBusy = FALSE;
}

///////////////////////////////////////////////////////////////
//	�� �� �� : OnTimeOut
//	�������� : ��ʱ����
//	������� : 
//	��    ע : 
//	��    �� : �Ժ��
//	ʱ    �� : 2009��11��6��
//	�� �� ֵ : void
//	����˵�� : void
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
		{//����ʱ���ڣ�·��״̬���ܱ�����MST_WORK״̬
			if (p->byStatus == MST_WORK)
			{
				if (p->byRouterStart && (dwIntervalTime >= 10000))
				{//�ӳ�10s����ֹ��������ʱ��OnTimeOut��TxMonitor�ȴ���
					StopRouter();
					Trace("���ʱ��[%d],·�ɹ����쳣����ͣ!!!", dwIntervalTime);
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
//	�� �� �� : RouterInit
//	�������� : ·�ɳ�ʼ��
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 2011��4��27��
//	�� �� ֵ : BOOL
//	����˵�� : void
///////////////////////////////////////////////////////////////
BOOL RouterInit(void)
{
	TRouter *p = (TRouter *)GethApp();

	switch (p->byInitStep)
	{
	default:
	case INISTEP_CHKMAINID:	//1����ѯ���������ڵ��ַ
		p->bStuFirst = FALSE;
		if (TxAfn03_F4())
		{
			p->byInitStep = INISTEP_CHKIDPARA;
		}
		else 
			break;
	case INISTEP_CHKIDPARA:	//2����ѯ�ӽڵ����
		if (CheckIDPara())
		{
			p->bStuFirst = FALSE;
			p->byInitStep = INISTEP_SETPARA;
		}
		else break;
	case INISTEP_SETPARA:	//3������ģ�鴦�������Ŀǰδ��
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
//	�� �� �� : FreshIDPara
//	�������� : ˢ��·�ɽڵ����
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 2011��4��28��
//	�� �� ֵ : void
//	����˵�� : bThisTime - 1:����ִ��ͬ�� 0:��ʱִ��
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
//	�� �� �� : RouterChk
//	�������� : ���ģ������
//	������� : 
//	��    ע : 
//	��    �� : �Ժ��
//	ʱ    �� : 2010��12��11��
//	�� �� ֵ : BOOL
//	����˵�� : void
///////////////////////////////////////////////////////////////
BOOL RouterChk(void)
{
	TRouter *p = (TRouter *)GethApp();
	static BYTE byFailCnt = 0;
	BYTE ModeType[] = { 1, 10, 2, 3, 20 };

	FreshRTSta(RTST_ORIGIN, NULLP, NULLP, NULLP);
#if(!MOS_MSVC)
	if ((DWORD)(GetRunCount() - p->dwLastResetTime) < 60000)
	{//����1min�ڵȴ��ϱ��汾��Ϣ(��������Ҫ��Ӹ�λģ����ӦΪ67��),û���ϱ������������ѯ����
		if (!p->bLogon)
			return FALSE;
	}
#endif
	//��ѯ�汾��Ϣ
	if (!CheckVerInfo())
	{
		p->byModuleType = ModeType[byFailCnt % 5]; //���������ʿ����խ�������ߵ�
		if (++byFailCnt > 50)
		{
			OpenRouter();
			Trace("��ѯ���̴���50������Ӧ->��λ");
			byFailCnt = 0;
		}
		return FALSE;
	}
	byFailCnt = 0;
	FreshRTSta(RTST_IDENTIFYOK, NULLP, NULLP, NULLP);
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	�� �� �� : PauseTx
//	�������� : CACæʱ��ͣ����n��
//	������� : 
//	��    ע : 
//	��    �� : Administrator
//	ʱ    �� : 2013��5��7��
//	�� �� ֵ : void
//	����˵�� : DWORD dwSeconds
///////////////////////////////////////////////////////////////
void PauseTx(DWORD dwSeconds)
{
	TRouter *p = (TRouter *)GethApp();

	p->dwPauseTmStamp =  GetSecs();
	p->dwPauseTm = dwSeconds;
}

///////////////////////////////////////////////////////////////
//	�� �� �� : TxPaused
//	�������� : �Ƿ��ڷ�����ͣ״̬��CACæʱ��
//	������� : 
//	��    ע : 
//	��    �� : Administrator
//	ʱ    �� : 2013��5��7��
//	�� �� ֵ : BOOL
//	����˵�� : void
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
//	�� �� �� : ClearRouterID698
//	�������� : ɾ�������ز��ӽڵ�
//	������� : 
//	��    ע : 
//	��    �� : �Ժ��
//	ʱ    �� : 2010��9��29��
//	�� �� ֵ : void
//	����˵�� : void
///////////////////////////////////////////////////////////////
BOOL ClearRouterID698(void)
{
	TRouter *p = pAppRouter;

	if (p->byRouterStart)
		StopRouter();
	//��������ʼ��
	return TxAfn01_F2();
}

///////////////////////////////////////////////////////////////
//	�� �� �� : StartRouter
//	�������� : ����·��
//	������� : 
//	��    ע : ��ͷ��ʼ����
//	��    �� : �Ժ��
//	ʱ    �� : 2009��11��6��
//	�� �� ֵ : BOOL
//	����˵�� : void
///////////////////////////////////////////////////////////////
BOOL StartRouter(void)
{
	TRouter *p = pAppRouter;
	TRouterInfo *pRInfo = pRouterInfo;

	FreshRTSta(RTST_START, NULLP, NULLP, NULLP);
	//��֧�ּ����������ĳ���ģʽ
	if (pRInfo->byCBMode == 0x01)
	{
		TxAfn12_F2();			//��ͣ·��
		p->byRouterStart = TRUE;
		FreshRTSta(RTST_START, 0, NULLP, 1);
		return TRUE;
	}
	//֧��·�������ĳ���ģʽ
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
//	�� �� �� : StopRouter
//	�������� : ��ͣ·��
//	������� : 
//	��    ע : 
//	��    �� : �Ժ��
//	ʱ    �� : 2009��11��6��
//	�� �� ֵ : BOOL
//	����˵�� : void
///////////////////////////////////////////////////////////////
BOOL StopRouter(void)
{
	TRouter *p = pAppRouter;
	TRouterInfo *pRInfo = pRouterInfo;

	FreshRTSta(RTST_STOP, NULLP, NULLP, NULLP);
	//��֧�ּ����������ĳ���ģʽ
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
//	�� �� �� : ResumeRouter
//	�������� : �ָ�·��
//	������� : 
//	��    ע : �ָ�����
//	��    �� : �Ժ��
//	ʱ    �� : 2009��11��6��
//	�� �� ֵ : BOOL
//	����˵�� : void
///////////////////////////////////////////////////////////////
BOOL ResumeRouter(void)
{
	TRouter *p = pAppRouter;
	TRouterInfo *pRInfo = pRouterInfo;

	FreshRTSta(RTST_RESUME, NULLP, NULLP, NULLP);
	//��֧�ּ����������ĳ���ģʽ
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
//	�� �� �� : DealInfoField
//	�������� : ������Ϣ��
//	������� : 
//	��    ע : ����������λ���м̡��ź�ǿ�ȵ���Ϣ
//	��    �� : �Ժ��
//	ʱ    �� : 2009��11��16��
//	�� �� ֵ : BYTE
//	����˵�� : T376_2RxFm* pRxFm
///////////////////////////////////////////////////////////////
BYTE DealInfoField(T376_2RxFm* pRxFm)
{
	TRouter *p = (TRouter *)GethApp();
	BYTE* pAddr = (BYTE*)(pRxFm + 1);

	//�ŵ���ʶ(0-�����ŵ� 1-A�� 2-B�� 3-C��)
	if (pRxFm->byCtrl&BIT6)
		p->byChanel = (pRxFm->byInfoChannel & 0x0F) % 4;//�ŵ���ʶ(0-�����ŵ� 1-A�� 2-B�� 3-C��)
	//ֻ��������ͨ�ŵ�֡
	if (((pRxFm->byInfoModule >> 2) & 0x01) == 0)
		return 0;
	return 12;
}

///////////////////////////////////////////////////////////////
//	�� �� �� : DT2Fn
//	�������� : ����Ϣ��DT1/DT2ת��ΪFn
//	������� : 
//	��    ע : 
//	��    �� : �Ժ��
//	ʱ    �� : 2009��11��7��
//	�� �� ֵ : WORD
//	����˵�� : BYTE DT1,BYTE DT2
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
//	�� �� �� : InitMainID
//	�������� : �����ն˵�ַ�������ڵ��ַ
//	������� : 
//	��    ע : ���ڵ��ַ��8λȡ�ն˵�ַ��BCD�� ��4λȡ����������
//	��    �� : �Ժ��
//	ʱ    �� : 2010��10��25��
//	�� �� ֵ : void
//	����˵�� : void
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
//	�� �� �� : OpenRouter
//	�������� : ģ��Ӳ��λ
//	������� : 
//	��    ע : �ϵ硢�ϵ�
//	��    �� : Administrator
//	ʱ    �� : 2012��12��27��
//	�� �� ֵ : void
//	����˵�� : void
///////////////////////////////////////////////////////////////
void OpenRouter(void)
{
	if (pAppRouter != NULL)
	{//��λ���½����
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
//	�� �� �� : GetRouterInfo
//	�������� : ��ȡ·�ɼ����Ϣ
//	������� :
//	��    ע : 
//	��    �� : 
//	ʱ    �� :
//	�� �� ֵ : TRouterInfo*
//	����˵�� : void
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
	Fm[wLen++] = byTmp;	//��Լ
	Fm[wLen++] = 0;		//ͨ����ʱ����Ա�־
	Fm[wLen++] = 0;
	Fm[wLen++] = byLen; //����
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
	wDataLen = MW(pBuf[1], pBuf[0]);											//���ĳ���
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
	{//�п��л�����
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
		pHead->wLen = wLen;										//���ĳ���
		wFmLen = wLen + sizeof(TAFNF1_F1);							//���ݳ���
		RT698_PortRxClear((TPort *)GethPort());
		Tx_698Fm1(pMP->dwAddr, pMP->wAddr, 0, NULL, AFNF1, F1, p->Concurrent.buf, (BYTE)wFmLen);
		//�ñ�־
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
	//�����ȼ�
	byNo = GetProxyNo(PORT_ZB, 1);
	if (0xFF != byNo)
	{
		Trace("���ȼ�������ţ�%d",byNo);
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
//	�� �� �� : TC_QueryDI
//	�������� : ����·�����󳭶����ݡ�
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 2011��4��26��
//	�� �� ֵ : void
//	����˵�� : DWORD dwAddr,WORD wAddrEx --�ز��ӽڵ�
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
//	�� �� �� : FreshRTSta
//	�������� : ˢ��·��ģ�鵱ǰ����״̬
//	������� : 
//	��    ע : ����·�ɼ��ӣ������ֳ����
//	��    �� : 
//	ʱ    �� : 2014��8��25��
//	�� �� ֵ : void
//	����˵�� : BYTE byRTSta,BYTE byCh,BYTE byChSta,DWORD dwPara
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
//	�� �� �� : DealAfn06_F5
//	�������� : ����·�������ϱ��ĵ��ܱ��¼�
//	������� : 
//	��    ע : 
//	��    �� : �Ժ��
//	ʱ    �� : 2014��11��24��
//	�� �� ֵ : void
//	����˵�� : BYTE *pBuf
///////////////////////////////////////////////////////////////
void DealAfn06_F5(BYTE *pBuf)
{
	BYTE byDevType, byProtocol, byRxLen;
	BYTE byLen = 0;

	byDevType = pBuf[byLen++]; //�ӽڵ��豸����
	byProtocol = pBuf[byLen++]; //ͨ��Э������
	byRxLen = pBuf[byLen++]; //���ĳ���
	SendFm(PORT_ZB,&pBuf[byLen],byRxLen, 0x06f5,0,TRUE);
#if 0
	//���ܱ��¼������ϱ�
	if (!ChkAndRec645Fm(byRxLen, &pBuf[byLen]))
	{
		Trace("·���ϱ����ܱ��¼����ĸ�ʽ����!");
	}
#endif
}

#endif/*#if(MD_376_2)*/

