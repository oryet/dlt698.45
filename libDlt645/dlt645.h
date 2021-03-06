#ifndef DLT645_H_
#define DLT645_H_

#include "StdDataType.h"

typedef struct
{
	INT8U Addr[6];
	INT8U Ctrl;
	INT16U Length;
	INT8U DI[2];
	INT8U Data[1024];
	INT8U Time[6];
	INT8U SEQ;
	INT8U SearchTime[2];
	INT8U Err;
}FORMAT97;

typedef struct
{
	INT8U Addr[6];			//表地址
	INT8U Ctrl;				//控制码
	INT16U Length;			//数据域长度
	INT8U DI[4];			//数据项标识
	INT8U Data[1024];		//数据内容
	INT8U Time[6];			//广播校时时间（十进制）
	INT8U SearchTime[2];	//载波搜表时间（十进制）

	INT8U sections;
	INT8U startYear;
	INT8U startMonth;
	INT8U startDay;
	INT8U startHour;
	INT8U startMinute;

	INT8U SEQ;				//帧序号
	INT8U Err;
}FORMAT07;

#define CTRL_Read_07	0x11	//读数据
#define CTRL_Read_97	0x01	//读数据
#define TESTDEF

extern INT8U getCS645(INT8U* buf, const INT16U len);
extern INT16U getFECount(INT8U* recvBuf, const INT16U recvLen);//得到待解析报文中前导符FE的个数
extern INT16S composeProtocol07(FORMAT07* format07, INT8U* sendBuf);
extern INT8S analyzeProtocol07(FORMAT07* format07, INT8U* recvBuf, const INT16U recvLen, BOOLEAN *nextFlag);
extern INT8S simpleProtocol698(INT8U* recvBuf, const INT16U recvLen,INT8U *addr);
extern INT16S composeProtocol97(FORMAT97* format97, INT8U* sendBuf);
extern INT8S analyzeProtocol97(FORMAT97* format97, INT8U* recvBuf, const INT16U recvLen, BOOLEAN *nextFlag);
#endif
