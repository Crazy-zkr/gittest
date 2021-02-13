#include "wsd_Flat.h"

BOOL bDebug;
extern void Init_TimeCount(void);
extern void InitPort(void);
extern void mqtt_init(void);
extern void Time_Join(void);
extern void InitPlcRecord(void);
extern void InitTaskProxy(void);
extern void InitTcpServer(void);
TVar    Var;

int main(int _Argc, char ** _Argv)
{
#if(!MOS_MSVC)
	if ((_Argc>1) && (strcmp(_Argv[1], "debug") == 0))
		bDebug = TRUE;
	else
		bDebug = FALSE;
#else
	bDebug = TRUE;
	if (!VLinux_Init("./Vlinux/VLinux.dll"))
	{
		Trace("VLinux_Init ��ʼ��ʧ��!");
		return ;
	}
#endif
	bDebug = TRUE;
	Trace("Task is start");


	Init_PVTab();		//��ʼ��PV����
	Init_TimeCount();	//��ʼ��ʱ�������
	InitTask();			//��ʼ���������
	InitPort();			//��ʼ���˿ڲ���
	InitTcpServer();
	InitTaskProxy();
	mqtt_init();		//���ȳ�ʼ����MQTT�������Ա���������ʼ
	InitPlcRecord();	//��ʼ������
	Var.dwStartTime = GetSecs();
	Var.bySysInit |= BIT0;
//	TimeJoin();
	while (1)
	{
		MySleep(1000);
	}
	return 0;
}


