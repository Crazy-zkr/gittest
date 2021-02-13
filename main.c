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
		Trace("VLinux_Init 初始化失败!");
		return ;
	}
#endif
	bDebug = TRUE;
	Trace("Task is start");


	Init_PVTab();		//初始化PV操作
	Init_TimeCount();	//初始化时间计数器
	InitTask();			//初始化任务参数
	InitPort();			//初始化端口参数
	InitTcpServer();
	InitTaskProxy();
	mqtt_init();		//首先初始化化MQTT参数，以便后面参数初始
	InitPlcRecord();	//初始化参数
	Var.dwStartTime = GetSecs();
	Var.bySysInit |= BIT0;
//	TimeJoin();
	while (1)
	{
		MySleep(1000);
	}
	return 0;
}


