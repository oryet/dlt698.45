
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "AccessFun.h"
#include "dlt698.h"
#include "dlt698def.h"

#define LIB698_VER 	1
INT8S (*pSendfun)(int fd,INT8U* sndbuf,INT16U sndlen);
int comfd = 0;
extern unsigned short tryfcs16(unsigned char *cp, int  len);
/**************************************
 * 函数功能：DL/T698.45 状态机
 * 参数含义：
 **************************************/
int StateProcess(int* step,int* rev_delay,int delay_num, int* rev_tail,int* rev_head,INT8U *NetRevBuf,INT8U* dealbuf)
{
	int length=0,i=0;

//	fprintf(stderr,"\nstep = %d\n",*step);
	switch(*step)
	{
		case 0:		 // 找到第一个 0x68
			while(*rev_head!=*rev_tail)
			{
//				fprintf(stderr,"NetRevBuf[*rev_tail] = %02x ",NetRevBuf[*rev_tail]);
				if (NetRevBuf[*rev_tail]== 0x68)
				{
					*step = 1;
					break;
				}else {
					*rev_tail = (*rev_tail + 1)% FRAMELEN;
				}
			}
			break;
		case 1:	   //从rev_tail2开始 跨长度字节找 0x16
			if(((*rev_head-*rev_tail+FRAMELEN)%FRAMELEN) >= 3)
			{
//				fprintf(stderr,"\nNetRevBuf[*rev_tail] = %02x  %02x\n ",NetRevBuf[(*rev_tail+ 2)%FRAMELEN],NetRevBuf[(*rev_tail+ 1)%FRAMELEN]);
				length = (NetRevBuf[(*rev_tail+ 2 + FRAMELEN )%FRAMELEN] << 8) + NetRevBuf[(*rev_tail+ 1 + FRAMELEN)%FRAMELEN];
				length = (length) & 0x03FFF;//bit15 bit14 保留
//				fprintf(stderr,"\nlength = %d \n",length);
				if (length <= 0)//长度异常
				{
					*rev_tail = (*rev_tail + 1)% FRAMELEN;
					*step = 0;
					break;
				}else {
//					fprintf(stderr,"*rev_head = %d *rev_tail = %d ",*rev_head,*rev_tail);
					if((*rev_head-*rev_tail+FRAMELEN)%FRAMELEN >= (length+2))//长度length为除 68和16 以外的字节数
					{
						if(NetRevBuf[ (*rev_tail + length + 1 + FRAMELEN)% FRAMELEN ]== 0x16)
						{
							*rev_delay = 0;
//							fprintf(stderr,"RRR=[%d]\n",length+2);
							for(i=0;i<(length+2);i++)
							{
								dealbuf[i] = NetRevBuf[*rev_tail];
//								fprintf(stderr,"%02x ",dealbuf[i]);
								*rev_tail = (*rev_tail + 1) % FRAMELEN;
							}
							*step = 0;//进入下一步
							return (length+2);
						}
						else
						{
							if (*rev_delay < delay_num)
							{
								(*rev_delay)++;
								break;
							}else
							{
								*rev_delay = 0;
								*rev_tail = (*rev_tail +1 )% FRAMELEN;
								*step = 0;//返回第一步
							}
						}
					}else
					{
							if (*rev_delay < delay_num)
							{
								(*rev_delay)++;
								break;
							}else
							{
								*rev_delay = 0;
								*rev_tail = (*rev_tail +1 )% FRAMELEN;
								*step = 0;
							}
					}
				}
			}
			break;
		default :
			break;
	}
	return 0;
}

/**********************************************************
 * 处理帧头
 * 	获取：1、帧长度       2、控制码      3、服务器地址    4、客户机地址
 * 	校验帧头部分
 */
int CheckHead(unsigned char* buf ,CSINFO *csinfo)
{
	unsigned char b1=0, b2=0, hsc1=0, hsc2=0 ,sa_length=0 ,ctrl=0;
	unsigned short cs16=0;

	if(buf[0]==0x68  && csinfo!=NULL)
	{
		sa_length 	= (buf[4]& 0x0f) + 1; 		/*服务器地址长度 0,1,，，15 表示 1,2,，，16*/
		cs16 = tryfcs16(&buf[1], sa_length + 5);
		b1 = (cs16 & 0x00ff);
		b2 = ((cs16 >> 8) & 0x00ff);
		hsc1 = buf[5+ sa_length + 1];
		hsc2 = buf[5+ sa_length + 2];
//		fprintf(stderr,"\nhsc1= %02x b1=%02x hsc2=%02x b2=%02x\n",hsc1,b1,hsc2,b2);
		if (hsc1 == b1 && hsc2 == b2)
		{
			ctrl 	= buf[3];
			csinfo->frame_length = ((buf[2]&0x3f << 8) + buf[1]) & 0x03FFF;
			csinfo->funcode = ctrl & 0x03;
			csinfo->dir = (ctrl & 0x80)>>7;
			csinfo->prm = (ctrl & 0x40)>>6;
			csinfo->gframeflg= (ctrl & 0x20)>>5;
			csinfo->sa_type 	= (buf[4]& 0xc0) >> 6;	/*0:单地址   1：通配地址   2：组地址   3：广播地址*/
			memcpy(csinfo->sa,&buf[5],sa_length);		/*服务器地址*/
			csinfo->ca 	 = buf[5+ sa_length]; 			/*客户机地址*/
			csinfo->sa_length = sa_length;
			return 1;
		}else
		{
			fprintf(stderr,"\n帧头校验错误!");
		}
	}
	return 0;
}
/******************************************************
 * 校验帧头部分
 * length:需要校验的字节长度  = 帧长度 - 起始符和结束符两个字节
 ******************************************************/
int CheckTail(unsigned char * buf,INT16U length)
{
	INT16U cs16=0;
	INT8U b1=0, b2=0, fsc1=0, fsc2=0 ;
	if( buf[0]==0x68  )
	{
//		fprintf(stderr,"frame length(-2) = %d ",length);
		cs16 = tryfcs16(&buf[1], length-2);
		b1 = (cs16 & 0x00ff);
		b2 = ((cs16 >> 8) & 0x00ff);
		fsc1 = buf[length - 1];
		fsc2 = buf[length];
//		fprintf(stderr,"\nfsc1= %02x b1=%02x     fsc2=%02x b2=%02x\n",fsc1,b1,fsc2,b2);
		if (fsc1 == b1 && fsc2 == b2)
		{
			return 1;
		}else {
			fprintf(stderr,"\n帧尾校验错误!");
		}
	}
	return 0;
}
INT8U CtrlWord(CSINFO csinfo)
{
	INT8U word=0;
//	fprintf(stderr,"\ncsinfo.dir=%d  csinfo.prm=%d gf=%d fun=%d",csinfo.dir,csinfo.prm,csinfo.gframeflg,csinfo.funcode);

	word = word | (csinfo.dir<<7);
	word = word | (csinfo.prm<<6);
	word = word | (csinfo.gframeflg<<5);
	word = word | (csinfo.funcode);
//	fprintf(stderr,"crtl word = %02x",word);
	return word;
}
void FrameTail(INT8U *buf,int index,int hcsi)
{
	INT8U b1=0,b2=0;
	INT16U cs16=0;
	INT16U length = (index+1) & 0x3fff;

	memcpy(&buf[1],&length,2);

	cs16 = tryfcs16(&buf[1], hcsi-1);
	b1 = (cs16 & 0x00ff);
	b2 = ((cs16 >> 8) & 0x00ff);
	buf[hcsi] =b1;
	buf[hcsi+1]	=b2;

	cs16 = tryfcs16(&buf[1], index-1);
	b1 = (cs16 & 0x00ff);
	b2 = ((cs16 >> 8) & 0x00ff);
	buf[index] = b1;
	buf[index+1] = b2;
	buf[index+2] = 0x16;
	return;
}
int FrameHead(CSINFO *csinfo,INT8U *buf)
{
	int i=0;
	buf[i++]= 0x68;//起始码
	buf[i++]= 0;	//长度
	buf[i++]= 0;
//	fprintf(stderr,"控制码 i=%d",i);
	buf[i++]= CtrlWord(*csinfo);
	buf[i++]= (csinfo->sa_type<<6) | (0<<4) | ((csinfo->sa_length-1) & 0xf);
	memcpy(&buf[i],csinfo->sa,csinfo->sa_length );
	i = i + csinfo->sa_length;
	buf[i++]=csinfo->ca;
//	fprintf(stderr,"i=%d\n",i);
	return i;
}

/**********************************************************************
 *	服务器向远方客户机提出
 *	1.登录
 *	2.心跳
 *	3.退出登录
 */
int Link_Request(LINK_Request request,INT8U *addr,INT8U *buf)
{
	int index=0, hcsi=0;
	CSINFO csinfo={};

	csinfo.dir = 1;		//服务器发出
	csinfo.prm = 0; 	//服务器发出
	csinfo.funcode = 1; //链路管理
	csinfo.sa_type = 0 ;//单地址
	csinfo.sa_length = 6;//sizeof(addr)-1;//服务器地址长度

//	fprintf(stderr,"sa_length = %d \n",csinfo.sa_length);
	memcpy(csinfo.sa,addr,csinfo.sa_length );//服务器地址
	csinfo.ca = 0;

	index = FrameHead(&csinfo,buf) ; //	2：hcs  hcs
	hcsi = index;
	index = index + 2;
//	fprintf(stderr,"\n link request type i=%d",index);
	buf[index++] = 1;//LINK_Request

	memcpy(&buf[index],&request,sizeof(LINK_Request));
	index = index + sizeof(LINK_Request);
//	fprintf(stderr,"\n add link request  i=%d",index);
//	fprintf(stderr,"\n  LINK_Request = %d",sizeof(LINK_Request));
	FrameTail(buf,index,hcsi);
	return (index + 3);			//3: cs cs 16
}

int Link_Response(INT8U *apdu)
{
	LINK_Response linkresponse={};
	memcpy(&linkresponse,&apdu[1],sizeof(LINK_Response));
	linkresponse.request_time.year = ((linkresponse.request_time.year<<8)&0xff00) | ((linkresponse.request_time.year>>8)&0x00ff);
	linkresponse.response_time.year = ((linkresponse.response_time.year<<8)&0xff00) | ((linkresponse.response_time.year>>8)&0x00ff);
	linkresponse.reached_time.year = ((linkresponse.reached_time.year<<8)&0xff00) | ((linkresponse.reached_time.year>>8)&0x00ff);
	fprintf(stderr,"\nLinkResponse: \nPIID = %02x \nResult = %02x ",linkresponse.piid.data,linkresponse.result);
	fprintf(stderr,"\nrequest time: %d-%d-%d %d:%d:%d",linkresponse.request_time.year,linkresponse.request_time.month,linkresponse.request_time.day_of_month,linkresponse.request_time.hour,linkresponse.request_time.minute,linkresponse.request_time.second);
	fprintf(stderr,"\nresponse time: %d-%d-%d  %d:%d:%d",linkresponse.response_time.year,linkresponse.response_time.month,linkresponse.response_time.day_of_month,linkresponse.response_time.hour,linkresponse.response_time.minute,linkresponse.response_time.second);
	fprintf(stderr,"\nreached time: %d-%d-%d  %d:%d:%d",linkresponse.reached_time.year,linkresponse.reached_time.month,linkresponse.reached_time.day_of_month,linkresponse.reached_time.hour,linkresponse.reached_time.minute,linkresponse.reached_time.second);
	return 1;
}
/**********************************************************************
 * 1.	LINK.response 服务,本服务由客户机应用进程调用,用于向服务器应用进程响应预连接请求。
 * 					  主站（客户机）应答集中器（服务器）预连接请求
 */
int dealClientResponse(INT8U *apdu,CSINFO *csinfo)
{

	INT8U apduType = 0;

	apduType = apdu[0];

	switch(apduType)
	{
		case LINK_RESPONSE:
			Link_Response( apdu );//预连接响应
			break;
		case REPORT_RESPONSE:
			break;
	}
	return apduType;
}
int appConnectResponse(INT8U *apdu,CSINFO *csinfo,INT8U *buf)
{
	int index=0, hcsi=0;
	CONNECT_Request *request=(CONNECT_Request *)apdu;
	CONNECT_Response response={};
	csinfo->dir = 1;
	csinfo->prm = 0;

	memset(&response,0,sizeof(response));
	index = FrameHead(csinfo,buf);
	hcsi = index;
	index = index + 2;

	response.piid_acd = request->piid;
	response.app_version = request->expect_app_ver;
	response.server_recv_size = BUFLEN;
	response.server_send_size = BUFLEN;
	response.server_deal_maxApdu = 1;
//	response.server_factory_version=1;
	memcpy(response.ProtocolConformance,request->ProtocolConformance,sizeof(request->ProtocolConformance));
	memcpy(&buf[index],&response,sizeof(response));
	index = index + sizeof(response);

	FrameTail(buf,index,hcsi);

	if(pSendfun!=NULL)
		pSendfun(comfd,buf,index+3);
	return (index+3);
}


int setRequestNormal(INT8U *Object,CSINFO *csinfo,INT8U *buf)
{
	int bytes=0;
	OAD oad={};//对象属性描述符
	memcpy(&oad , Object,4);
//	bytes = setOneObject(&oad,csinfo,buf);
	return bytes;
}
int setRequestNormalList(INT8U *Object,CSINFO *csinfo,INT8U *buf)
{
	int stepsize=1 , i=0 , bytes=0, objbytes=0;
	INT8U objectnum = Object[0];

	for(i=0 ; i< objectnum ; i++)
	{
		objbytes = setRequestNormal(Object+stepsize,csinfo,buf);
		if (objbytes >0)
			stepsize = stepsize + objbytes;
		else
			break;
	}
	return bytes;
}
int doSetAttribute(INT8U *apdu,CSINFO *csinfo,INT8U *buf)
{
	PIID piid={};
	INT8U setType = apdu[1];
	piid.data = apdu[2];

	switch(setType)
	{
		case SET_REQUEST_NORMAL:
			setRequestNormal(&apdu[3],csinfo,buf);
			break;
		case SET_REQUEST_NORMAL_LIST:
			setRequestNormalList(&apdu[3],csinfo,buf);
			break;
		case SET_THENGET_REQUEST_NORMAL_LIST:

			break;
	}
	return 1;
}
int doGetAttribute(INT8U *apdu,CSINFO *csinfo,INT8U *buf)
{
	INT8U setType = apdu[1];
	PIID piid={};
	piid.data = apdu[2];
	switch(setType)
	{
		case GET_REQUEST_NORMAL:

			break;
		case GET_REQUEST_NORMAL_LIST:
			break;
		case GET_REQUEST_RECORD:
			break;
		case GET_REQUEST_RECORD_LIST:
			break;
		case GET_REQUEST_RECORD_NEXT:
			break;
	}
	return 1;
}

int doActionReponse(int reponse,CSINFO *csinfo,PIID piid,OMD omd,int dar,INT8U *data,INT8U *buf)
{
	int index=0, hcsi=0;

	csinfo->dir = 1;
	csinfo->prm = 0;

	index = FrameHead(csinfo,buf);
	hcsi = index;
	index = index + 2;
	buf[index] = ACTION_RESPONSE;
	index++;
	buf[index] = reponse;
	index++;
//	fprintf(stderr,"piid.data[%d]=%02x\n",index,piid.data);
	buf[index] = piid.data;
	index++;
	memcpy(&buf[index],&omd,sizeof(OMD));
	index = index + sizeof(OMD);
	buf[index] = omd.OI & 0xff;
	index++;
	buf[index] = (omd.OI>>8) & 0xff;
	index++;
	buf[index] = omd.method_tag;
	index++;
	buf[index] = omd.oper_model;
	index++;

	buf[index] = dar;
	index++;
	if(data!=NULL) {
		memcpy(&buf[index],&data,sizeof(data));
		index = index + sizeof(data);
	}
	FrameTail(buf,index,hcsi);

	if(pSendfun!=NULL)
		pSendfun(comfd,buf,index+3);
	return (index+3);
}

int doActionRequest(INT8U *apdu,CSINFO *csinfo,INT8U *buf)
{
	int  DAR=0;
	PIID piid={};
	OMD omd={};
	INT8U *data=NULL;
	INT8U request_choice = apdu[1];		//ACTION-Request
	piid.data = apdu[2];				//PIID
//	memcpy(&omd,&apdu[3],4);			//OMD
	omd.OI= (apdu[3]<<8) | apdu[4];
	omd.method_tag = apdu[5];
	omd.oper_model = apdu[6];
	data = &apdu[7];					//Data
	fprintf(stderr,"\n-------- request choice = %d omd OI = %04x  method=%d",request_choice,omd.OI,omd.method_tag);
	switch(request_choice)
	{
		case ACTIONREQUEST:
			DAR = doObjectAction(omd,data);
			doActionReponse(ActionResponseNormal,csinfo,piid,omd,DAR,NULL,buf);
			break;
		case ACTIONREQUEST_LIST:
			break;
		case ACTIONTHENGET_REQUEST_NORMAL_LIST:
			break;
	}
	return 1;
}
/**********************************************************************
 * 1.	CONNECT.request 服务,本服务由客户机应用进程调用,用于向远方服务器的应用进程提出建立应用连接请求。
 * 						主站（客户机）请求集中器（客户机）建立应用连接
 */
INT8U dealClientRequest(INT8U *apdu,CSINFO *csinfo,INT8U *sendbuf)
{
	INT8U apduType = apdu[0];
	fprintf(stderr,"\n-------- apduType = %d ",apduType);
	switch(apduType)
	{
		case CONNECT_REQUEST:
			appConnectResponse(apdu,csinfo,sendbuf);
			break;
		case GET_REQUEST:
			doGetAttribute(apdu,csinfo,sendbuf);
			break;
		case SET_REQUEST:
			doSetAttribute(apdu,csinfo,sendbuf);
			break;
		case ACTION_REQUEST:
			doActionRequest(apdu,csinfo,sendbuf);
			break;
		case PROXY_REQUEST:
			break;
		case RELEASE_REQUEST:
			break;
	}
	return(apduType);
}

int ProcessData(CommBlock *com)
{
	CSINFO csinfo={};
	int hcsok = 0 ,fcsok = 0;
	INT8U *apdu= NULL;
	INT8U *Rcvbuf = com->DealBuf;
	INT8U *SendBuf = com->SendBuf;
	pSendfun = com->p_send;
	comfd = com->phy_connect_fd;

	hcsok = CheckHead( Rcvbuf ,&csinfo);
	fcsok = CheckTail( Rcvbuf ,csinfo.frame_length);
	if ((hcsok==1) && (fcsok==1))
	{
		apdu = &Rcvbuf[csinfo.sa_length+8];
		if (csinfo.dir == 0 && csinfo.prm == 0)		/*客户机对服务器上报的响应	（主站对集中器上报的响应）*/
		{
			return(dealClientResponse(apdu,&csinfo));
		}else if (csinfo.dir==0 && csinfo.prm == 1)	/*客户机发起的请求			（主站对集中器发起的请求）*/
		{
			fprintf(stderr,"\n-------- 客户机发起请求 ");
			return(dealClientRequest(apdu,&csinfo,SendBuf));
		}else if (csinfo.dir==1 && csinfo.prm == 0)	/*服务器发起的上报			（电表主动上报）*/
		{
			//MeterReport();
		}else if (csinfo.dir==1 && csinfo.prm == 1)	/*服务器对客户机请求的响应	（电表应答）*/
		{
			//MeterEcho();
		}else
		{
			fprintf(stderr,"\n控制码解析错误(传输方向与启动位错误)");
		}
	}
	return 1;
}
