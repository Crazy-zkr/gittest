#include "wsd_Flat.h"
#include "wsd_cgy.h"
///////////////////////////////////////////////////////////////
//	�� �� �� : CheckGyCfg
//	�������� : ����Լ���ñ��Ƿ���Ч
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : BOOL ��Ч����TRUE
//	����˵�� : TGy *p ��Լ���ñ��ַ
///////////////////////////////////////////////////////////////
static BOOL CheckGyCfg(TGy *p)
{
	TGyCfg *pc = &p->GyCfg;
	TPort *pPort = (TPort*)GethPort();

	switch (pc->wGyMode)
	{
	case MGYM_MASTER:			   //�ʴ�ʽ(��)
	case MGYM_SLAVE:			   //�ʴ�ʽ(��)
	case MGYM_CALC:				  //�㷨��
		if (pc->wMinDevNum <1)
			return FALSE;

		p->byMode = (BYTE)pc->wGyMode;
		break;
	case MGYM_SYS_MASTER:			  //ϵͳ�ࣨ������MGYM_MASTER�������豸��
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

	//�ַ����  6201��Ҫ100ms�� 1200BPSʱ��Modem�ӳ���250ms
	if (pc->wCommIdleTime  < 400)
		pc->wCommIdleTime = 400;

	if (pc->wGyMode == MGYM_MASTER)
	{//���ͼ��
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
//	�� �� �� : OnTimeOut
//	�������� : ��Լ����Ķ�ʱ������
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : void
//	����˵�� : HPARA hGy,  ��Լ������
//				DWORD dwTimeID ��ʱ��ID
///////////////////////////////////////////////////////////////
static void OnTimeOut(DWORD dwTimeID)
{
TGy *p = (TGy *)GethGy();

	if (p->fOnTimeOut)
		p->fOnTimeOut(dwTimeID);
}

///////////////////////////////////////////////////////////////
//	�� �� �� : SearchOneFrame
//	�������� : ����һ�������ı���׮����
//	������� : 
//	��    ע : ����һ��׮����
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : DWORD
//	����˵�� : HPARA hGy,��Լ��������
//				HPARA hBuf,���Ļ�����
//				DWORD dwLen �������ĳ���
///////////////////////////////////////////////////////////////
static DWORD SearchOneFrame(HPARA hBuf, DWORD dwLen)
{
	return FM_ERR | dwLen;
}

///////////////////////////////////////////////////////////////
//	�� �� �� : RxMonitor
//	�������� : ���մ����׮����
//	������� : 
//	��    ע : ����һ��׮����
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : void
//	����˵�� : HPARA hApp Ӧ�þ��
///////////////////////////////////////////////////////////////
static void  RxMonitor(void)
{
}

///////////////////////////////////////////////////////////////
//	�� �� �� : RxMonitor
//	�������� : ���ʹ����׮����
//	������� : 
//	��    ע : ����һ��׮����
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : void
//	����˵�� : HPARA hApp Ӧ�þ��
///////////////////////////////////////////////////////////////
static void  TxMonitor(void)
{
}

///////////////////////////////////////////////////////////////
//	�� �� �� : AppCreate
//	�������� : ���ڹ�Լƽ̨�ı�׼���񴴽�����
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : BOOL
//	����˵�� : DWORD dwAppMemSize,
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
//	�� �� �� : AppDelete
//	�������� : ���ڹ�Լƽ̨�ı�׼����ɾ������
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : void
//	����˵�� : void
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
//	�� �� �� : GY_Create
//	�������� : ������Լ����
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : BOOL �����ɹ�����TRUE
//	����˵�� : void
///////////////////////////////////////////////////////////////
BOOL GY_Create(void)
{//������Լ����
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
//	�� �� �� : GY_Delete
//	�������� : ɾ����Լ����
//	������� : �ͷŻ���������������ڴ�
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : void
//	����˵�� : void
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
//	�� �� �� : GY_Init
//	�������� : ��Լ�����ʼ��
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : BOOL �ɹ�����TRUE
//	����˵�� : void
///////////////////////////////////////////////////////////////
BOOL GY_Init(void)
{//��Լ��ʼ��
TGy *p = (TGy *)GethGy();
TPort *pPort = (TPort *)GethPort();

	if (CheckGyCfg(p) != TRUE)
		return FALSE;
	//��ʼ���˿�
	if (GY_InitPort(p) != TRUE)
	{
		Trace("�˿ڴ���");
		PT_DeletePort(pPort->Cfg.dwID);
		return FALSE;
	}
	//���ض�ʱ
	OS_OnCommand(MCMD_ONTIMEOUT, (HPARA)OnTimeOut);
	p->RetryCnt = 100;
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	�� �� �� : GY_InitPort
//	�������� : ��Լ����Ķ˿ڳ�ʼ��
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : BOOL �ɹ�����TRUE
//	����˵�� : HPARA hGy ��Լ������
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
	//�򿪶˿�
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
	//������������
	if (OS_SetTimer(MTID_DRIVER, 20) != TRUE)//���ö�ʱ��
		return FALSE;

	OS_OnCommand(MCMD_ONDRIVER, (HPARA)OnDriver);
	return TRUE;
}

///////////////////////////////////////////////////////////////
//	�� �� �� : Register
//	�������� : ��Լ���������ĸ��ֺ�����ע�Ṧ��
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : void
//	����˵�� : DWORD dwFUNID,��Լ���ຯ��ID
//				HPARA hFun,  �����ĺ������
//				HPARA hPara  �����õ��Ĳ���
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
//	�� �� �� : GY_SetTimer
//	�������� : ��Լ��������ö�ʱ��
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : BOOL
//	����˵�� : DWORD dwTimerID,��ʱ��ID
//				DWORD dwMSTime����ʱ�������λMS
///////////////////////////////////////////////////////////////
BOOL GY_SetTimer(DWORD dwTimerID, DWORD dwMSTime)
{
	return OS_SetTimer(dwTimerID, dwMSTime);
}

///////////////////////////////////////////////////////////////
//	�� �� �� : GetGyCfg
//	�������� : ��ȡ��Լ����Ĺ�Լ���ñ�ָ��
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : TGyCfg* ��ȡ�Ĺ�Լ��������ñ��ָ��
//	����˵�� : void
///////////////////////////////////////////////////////////////
TGyCfg* GetGyCfg(void)
{
	TGy *p = (TGy *)GethGy();
	return &p->GyCfg;
}

///////////////////////////////////////////////////////////////
//	�� �� �� : GetDriveID
//	�������� : ��ȡ��Լ��
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : WORD
//	����˵�� : void
///////////////////////////////////////////////////////////////
WORD GetDriveID(void)
{
	TPort *pPort = (TPort *)GethPort();
	return pPort->Cfg.wDriverID;
}

///////////////////////////////////////////////////////////////
//	�� �� �� : DisableUnLock
//	�������� : ��ֹ�Զ��ͷŶ˿ڿ���Ȩ
//	������� : 
//	��    ע : ����һ�ʶ����Ĺ�Լ����һ�ڶ��Լ���ο��¡�
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : void
//	����˵�� : void
///////////////////////////////////////////////////////////////
void DisableUnLock(BOOL bMan)//��ֹ�ͷſ���Ȩ
{
	TGy *p = (TGy *)GethGy();
	p->DisableUnLock = TRUE;
	if (bMan)
		p->DisableUnLock |= DU_MAN_MASK;
}
void EnableUnLock(void)//�ֶ��ͷſ���Ȩ
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
//	�� �� �� : GetTx
//	�������� : ��ȡ��ǰ��Լ�ķ��ͻ���������ָ��
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : TTx* ��ǰ��Լ�ķ��ͻ���������ָ��
//	����˵�� : void
///////////////////////////////////////////////////////////////
TTx* GetTx(void)
{
	TGy *p = (TGy *)GethGy();
	return &p->Tx;
}

///////////////////////////////////////////////////////////////
//	�� �� �� : GetRx
//	�������� : ��ȡ��ǰ��Լ�Ľ��ջ���������ָ��
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : TRx* ��ǰ��Լ�Ľ��ջ���������ָ��
//	����˵�� : void
///////////////////////////////////////////////////////////////
TRx *GetRx(void)
{
	TGy *p = (TGy *)GethGy();
	return &p->Rx;
}

///////////////////////////////////////////////////////////////
//	�� �� �� : GetRxFm
//	�������� : ��ȡ���ձ��Ļ�������ַ
//	������� : 
//	��    ע : 
//	��    �� : 
//	ʱ    �� : 
//	�� �� ֵ : HPARA
//	����˵�� : void
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
