/* base on CAENHVDemo * */
#include<iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <map>

#include <math.h>
#include <assert.h>
#include <float.h>
#include <algorithm>

#include "CAENHVWrapper.h"
#include "TString.h"
#include "TMutex.h"

using namespace std;
//#define DEBUG
//#define MAX_CH MAX_CH
#define MAX_CH 4
#define refresh_time 1
#define Log_Time_Interval 7200
char* default_html_index="index.html";

typedef struct CAENChannelTag 
{
    char Name[MAX_CH_NAME];
    unsigned int ChStatus;
    unsigned long  Pw;
    unsigned long  Pol;
    float VSet;
    float ISet;
    float VMon;
    float IMon;
    float SVMax;
    float Trip;
    float RUp;
    float RDwn;
    unsigned long  PDwn;
}CAENHVChannelParameters;

typedef struct CAENBoardTag 
{
    int handle;
    int BdNum;
    int BdStatus;
    CAENHVChannelParameters ch[MAX_CH];
}CAENHVBoardParameters;

map<int,CAENHVBoardParameters>m_handle;
TMutex* mutex;
TString curr_time;
time_t time_pre;
unsigned int data_int[MAX_CH]={0};
float data_float[MAX_CH]={0};
unsigned long data_unsigned[MAX_CH]={true};
char data_str[MAX_CH][MAX_CH_NAME];
unsigned short chlist[4]={0,1,2,3};
TString html_lock;

int MWPC0_Pos_ch;
int MWPC0_Neg_ch;
int MWPC1_Pos_ch;
int MWPC1_Neg_ch;
int DSSD_Pos_ch;
int DSSD_Neg_ch;


/********** below are for web server , do not change ******/
/* size of buffer for incoming data, must fit sum of all attachments */
#define WEB_BUFFER_SIZE (6*1024*1024)
size_t return_size = WEB_BUFFER_SIZE;
int strlen_retbuf;
int return_length;
char host_name[256];
char referer[256];
int tcp_port = 8300;
//char* default_index="midas.html";
const char Respond_HeadFormat[] =
"HTTP/1.1 200 OK \r\n"
"Content-Type: %s\r\n"
"Accept-Ranges: bytes\r\n"
"Content-Length: %d\r\n\r\n";

const char Respond_HeadFormat1[] =
"HTTP/1.1 200 OK \r\n"
"Content-Type: %s;charset=utf8\r\n"
"Accept-Ranges: bytes\r\n"
"Content-Length: %d\r\n\r\n";

const char NotFoundHeadFormat[] =
"HTTP/1.1 404 NOT FOUND \r\n"
"Content-Type: text/html\r\n"
"Accept-Ranges: bytes\r\n"
"Content-Length: 0\r\n\r\n";

TString GetTimeString(time_t time_now)
{
    TString str;
    struct tm *time_struct;
    time_struct=localtime(&time_now);
    str=str.Format("%4d-%d-%d,%d:%02d:%02d",time_struct->tm_year+1900,time_struct->tm_mon+1,time_struct->tm_mday,time_struct->tm_hour,time_struct->tm_min,time_struct->tm_sec);
    //  str=str.Format("%s",asctime(time_struct));
    // str.Replace(str.Index("\n"),1,"");
    return str;
}
void Update_Chanme()
{
    FILE* fp_in;
    if((fp_in=fopen("./chname.txt","w")) == NULL)
    {
        printf("open file chname.txt error!\n");
        return ;
    }

    map<int,CAENHVBoardParameters>::iterator iter;
    for(iter=m_handle.begin();iter != m_handle.end();iter++)
    {
        for(int ii=0;ii<MAX_CH;ii++)
            fprintf(fp_in,"%d %d %s\n",iter->first,ii,iter->second.ch[ii].Name);
    }
    fclose(fp_in);
}
int GetChName(int BdNum,int ChNum,char (*data)[MAX_CH_NAME])
{
    int ret,ii;
    //// read parameter in board
    map<int,CAENHVBoardParameters>::iterator iter;
    iter=m_handle.find(BdNum);
    if(iter == m_handle.end())
    {
        printf("wrong bdnum, plz check");
        return -1;
    }
    int Total_Chnum=0;

    if(ChNum >= 4)
    {
        chlist[0]=0;
        chlist[1]=1;
        chlist[2]=2;
        chlist[3]=3;
        Total_Chnum=4;
    }
    else
    {
        chlist[0]=ChNum;
        Total_Chnum=1;

    }

    /////////////////
    ret=CAENHV_GetChName(iter->second.handle,0,Total_Chnum,chlist,data);
    if(ret != CAENHV_OK)
    {
        printf("r/w error,ret=%d\n",ret);
        return -1;
    }
#ifdef DEBUG
   // printf("read init data:%d\n",data[0]);
#endif
    return CAENHV_OK;
}

int SetChName(int BdNum,int ChNum,char* data)
{
    int ret,ii;
    //// read parameter in board
    map<int,CAENHVBoardParameters>::iterator iter;
    iter=m_handle.find(BdNum);
    if(iter == m_handle.end())
    {
        printf("wrong bdnum, plz check");
        return -1;
    }
    int Total_Chnum=0;

    if(ChNum >= 4)
    {
        chlist[0]=0;
        chlist[1]=1;
        chlist[2]=2;
        chlist[3]=3;
        Total_Chnum=4;
    }
    else
    {
        chlist[0]=ChNum;
        Total_Chnum=1;
    }

    /////////////////
    //ret=CAENHV_SetChParam(iter->second.handle,BdNum,ParName,Total_Chnum,chlist,data);
    ret=CAENHV_SetChName(iter->second.handle,0,Total_Chnum,chlist,data);
    if(ret != CAENHV_OK)
    {
        printf("r/w error,ret=%d\n",ret);
        return -1;
    }
    return CAENHV_OK;
}

int GetChParameter_unsigned(int BdNum,int ChNum,char* ParName,unsigned long* data)
{
    int ret,ii;
    //// read parameter in board
    map<int,CAENHVBoardParameters>::iterator iter;
    iter=m_handle.find(BdNum);
    if(iter == m_handle.end())
    {
        printf("wrong bdnum, plz check");
        return -1;
    }
    int Total_Chnum=0;

    if(ChNum >= 4)
    {
        chlist[0]=0;
        chlist[1]=1;
        chlist[2]=2;
        chlist[3]=3;
        Total_Chnum=4;
    }
    else
    {
        chlist[0]=ChNum;
        Total_Chnum=1;

    }

    /////////////////
    //ret=CAENHV_GetChParam(iter->second.handle,BdNum,ParName,Total_Chnum,chlist,data);
    ret=CAENHV_GetChParam(iter->second.handle,0,ParName,Total_Chnum,chlist,data);
    if(ret != CAENHV_OK)
    {
        return -1;
    }
#ifdef DEBUG
   // printf("read init data:%d\n",data[0]);
#endif
    return CAENHV_OK;
}

//int SetChParameter_bool(int BdNum,int ChNum,char* ParName,bool* data)
int SetChParameter_unsigned(int BdNum,int ChNum,char* ParName,unsigned long* data)
{
    int ret,ii;
    //// read parameter in board
    map<int,CAENHVBoardParameters>::iterator iter;
    iter=m_handle.find(BdNum);
    if(iter == m_handle.end())
    {
        printf("wrong bdnum, plz check");
        return -1;
    }
    int Total_Chnum=0;

    if(ChNum >= 4)
    {
        chlist[0]=0;
        chlist[1]=1;
        chlist[2]=2;
        chlist[3]=3;
        Total_Chnum=4;
    }
    else
    {
        chlist[0]=ChNum;
        Total_Chnum=1;

    }

    /////////////////
    //ret=CAENHV_SetChParam(iter->second.handle,BdNum,ParName,Total_Chnum,chlist,data);
    ret=CAENHV_SetChParam(iter->second.handle,0,ParName,Total_Chnum,chlist,data);
    if(ret != CAENHV_OK)
    {
        printf("r/w error,ret=%d\n",ret);
        return -1;
    }
    return CAENHV_OK;
}

int GetChParameter_float(int BdNum,int ChNum,char* ParName,float* data)
{
    int ret,ii;
    //// read parameter in board
    map<int,CAENHVBoardParameters>::iterator iter;
    iter=m_handle.find(BdNum);
    if(iter == m_handle.end())
    {
        printf("wrong bdnum, plz check");
        return -1;
    }
    int Total_Chnum=0;

    if(ChNum >= 4)
    {
        chlist[0]=0;
        chlist[1]=1;
        chlist[2]=2;
        chlist[3]=3;
        Total_Chnum=4;
    }
    else
    {
        chlist[0]=ChNum;
        Total_Chnum=1;

    }

    /////////////////
    //ret=CAENHV_GetChParam(iter->second.handle,BdNum,ParName,Total_Chnum,chlist,data);
    ret=CAENHV_GetChParam(iter->second.handle,0,ParName,Total_Chnum,chlist,data);
    if(ret != CAENHV_OK)
    {
        printf("r/w error,ret=%d\n",ret);
        return -1;
    }
#ifdef DEBUG
    //printf("read init data:%f\n",data[0]);
#endif
    return CAENHV_OK;
}

int SetChParameter_float(int BdNum,int ChNum,char* ParName,float* data)
{
    int ret,ii;
    //// read parameter in board
    map<int,CAENHVBoardParameters>::iterator iter;
    iter=m_handle.find(BdNum);
    if(iter == m_handle.end())
    {
        printf("wrong bdnum, plz check");
        return -1;
    }
    int Total_Chnum=0;

    if(ChNum >= 4)
    {
        chlist[0]=0;
        chlist[1]=1;
        chlist[2]=2;
        chlist[3]=3;
        Total_Chnum=4;
    }
    else
    {
        chlist[0]=ChNum;
        Total_Chnum=1;

    }
    /////////////////
    //ret=CAENHV_SetChParam(iter->second.handle,BdNum,ParName,Total_Chnum,chlist,data);
    ret=CAENHV_SetChParam(iter->second.handle,0,ParName,Total_Chnum,chlist,data);
    if(ret != CAENHV_OK)
    {
        printf("r/w error,ret=%d\n",ret);
        return -1;
    }
    return CAENHV_OK;
}

int GetChParameter_int(int BdNum,int ChNum,char* ParName,unsigned int* data)
{
    int ret,ii;
    //// read parameter in board
    map<int,CAENHVBoardParameters>::iterator iter;
    iter=m_handle.find(BdNum);
    if(iter == m_handle.end())
    {
        printf("wrong bdnum, plz check");
        return -1;
    }
    int Total_Chnum=0;

    if(ChNum >= 4)
    {
        chlist[0]=0;
        chlist[1]=1;
        chlist[2]=2;
        chlist[3]=3;
        Total_Chnum=4;
    }
    else
    {
        chlist[0]=ChNum;
        Total_Chnum=1;

    }

    /////////////////
    //ret=CAENHV_GetChParam(iter->second.handle,BdNum,ParName,Total_Chnum,chlist,data);
    ret=CAENHV_GetChParam(iter->second.handle,0,ParName,Total_Chnum,chlist,data);
    if(ret != CAENHV_OK)
    {
        printf("r/w error,ret=%d\n",ret);
        return -1;
    }
#ifdef DEBUG
    //printf("read init data:%d\n",data[0]);
#endif
    return CAENHV_OK;
}

int SetChParameter_int(int BdNum,int ChNum,char* ParName,unsigned int* data)
{
    int ret,ii;
    //// read parameter in board
    map<int,CAENHVBoardParameters>::iterator iter;
    iter=m_handle.find(BdNum);
    if(iter == m_handle.end())
    {
        printf("wrong bdnum, plz check");
        return -1;
    }
    int Total_Chnum=0;

    if(ChNum >= 4)
    {
        chlist[0]=0;
        chlist[1]=1;
        chlist[2]=2;
        chlist[3]=3;
        Total_Chnum=4;
    }
    else
    {
        chlist[0]=ChNum;
        Total_Chnum=1;

    }
    /////////////////
    //ret=CAENHV_SetChParam(iter->second.handle,BdNum,ParName,Total_Chnum,chlist,data);
    ret=CAENHV_SetChParam(iter->second.handle,0,ParName,Total_Chnum,chlist,data);
    if(ret != CAENHV_OK)
    {
        printf("r/w error,ret=%d\n",ret);
        return -1;
    }
    return CAENHV_OK;
}

int Update_Parameters()
{
    int ret,ii,jj;
    map<int,CAENHVBoardParameters>::iterator iter;
    iter=m_handle.begin();
    mutex->Lock();
    time_t time_now;
    time(&time_now);
    char* str=ctime(&time_now);
    str[strlen(str)-1]='\0';
    curr_time=str;
    while(iter != m_handle.end())
    {
        ret=GetChParameter_float(iter->first,4,"VMon",data_float);
        if(ret != CAENHV_OK)
        {
            printf("init: read par VMon error\n");
        }
        else
            for(ii=0;ii<4;ii++)
            {
                iter->second.ch[ii].VMon=data_float[ii];
#ifdef DEBUG
      //          printf("read bd %d ch %d: VMon:%f\n",iter->first,ii,data_float[ii]);
#endif
            }

        ret=GetChParameter_float(iter->first,4,"IMonH",data_float);
     //   ret=GetChParameter_float(iter->first,4,"IMonL",data_float);
        if(ret != CAENHV_OK)
        {
            printf("init: read par IMon error\n");
        }
        else
            for(ii=0;ii<4;ii++)
            {
                iter->second.ch[ii].IMon=data_float[ii];
#ifdef DEBUG
      //          printf("read bd %d ch %d: IMon:%f\n",iter->first,ii,data_float[ii]);
#endif
            }

        ///////////
        for(ii=0;ii<4;ii++)
        {

            ret=GetChParameter_int(iter->first,ii,"ChStatus",data_int);
            iter->second.ch[ii].ChStatus=data_int[0];
#ifdef DEBUG
     //       printf("read bd %d ch %d: ChStatus:%d\n",iter->first,ii,data_int[0]);
#endif
        }
        //// next board 
        iter++;
    }/// end while
    mutex->UnLock();


    /// update log info file
    if(time_now>=time_pre+ Log_Time_Interval)
        time_pre=time_now;
    char logfilename[256];
    if(access("./log_dir",F_OK) != 0)
        system("mkdir log_dir");
    sprintf(logfilename,"./log_dir/HVlog_%s.txt",GetTimeString(time_pre).Data());
    FILE* fp_log;
    if((fp_log=fopen(logfilename,"a+")) ==NULL)
    {
        printf("open log file error!! \n");
        printf("open log file error!! \n");
        return -1;
    }
    for(iter=m_handle.begin();iter != m_handle.end();iter++)
    {
        for(int ii=0;ii<MAX_CH;ii++)
            fprintf(fp_log,"%s\t%10s\t%2d\t%2d\t%f\t%f\n",GetTimeString(time_now).Data(),iter->second.ch[ii].Name,iter->first,ii,iter->second.ch[ii].VMon,iter->second.ch[ii].IMon);
    }
    fclose(fp_log);
    return 0;
}



int Init_Parameters()
{
    int ret,ii,jj;
    map<int,CAENHVBoardParameters>::iterator iter;
    iter=m_handle.begin();
    mutex->Lock();
    while(iter != m_handle.end())
    {
        char (*p)[MAX_CH_NAME]=data_str;
        ret=GetChName(iter->first,4,p);
        if(ret != CAENHV_OK)
            printf("init: read chname error\n");
        else
            for(ii=0;ii<4;ii++)
            {
                if(strlen(iter->second.ch[ii].Name) <2)
                    sprintf(iter->second.ch[ii].Name,"%s",p+ii);
#ifdef DEBUG
                printf("read bd %d ch %d: chname:%s, copy:%s\n",iter->first,ii,p+ii,iter->second.ch[ii].Name);
#endif
            }

        ret=GetChParameter_float(iter->first,4, "VSet",data_float);
        if(ret != CAENHV_OK)
            printf("init: read par VSet error\n");
        else
            for(ii=0;ii<4;ii++)
            {
                iter->second.ch[ii].VSet=data_float[ii];
#ifdef DEBUG
                printf("read bd %d ch %d: VSet:%f\n",iter->first,ii,data_float[ii]);
#endif
            }

        ret=GetChParameter_float(iter->first,4, "ISet",data_float);
        if(ret != CAENHV_OK)
            printf("init: read par ISet error\n");
        else
            for(ii=0;ii<4;ii++)
            {
                iter->second.ch[ii].ISet=data_float[ii];
#ifdef DEBUG
                printf("read bd %d ch %d: ISet:%f\n",iter->first,ii,data_float[ii]);
#endif
            }

        ret=GetChParameter_float(iter->first,4,"VMon",data_float);
        if(ret != CAENHV_OK)
        {
            printf("init: read par VMon error\n");
        }
        else
            for(ii=0;ii<4;ii++)
            {
                iter->second.ch[ii].VMon=data_float[ii];
#ifdef DEBUG
                printf("read bd %d ch %d: VMon:%f\n",iter->first,ii,data_float[ii]);
#endif
            }

        ret=GetChParameter_float(iter->first,4,"IMonH",data_float);
        if(ret != CAENHV_OK)
        {
            printf("init: read par IMon error\n");
        }
        else
            for(ii=0;ii<4;ii++)
            {
                iter->second.ch[ii].IMon=data_float[ii];
#ifdef DEBUG
                printf("read bd %d ch %d: IMon:%f\n",iter->first,ii,data_float[ii]);
#endif
            }

        ret=GetChParameter_float(iter->first,4,"MaxV",data_float);
        if(ret != CAENHV_OK)
        {
            printf("init: read par SVMax error\n");
        }
        else
            for(ii=0;ii<4;ii++)
            {
                iter->second.ch[ii].SVMax=data_float[ii];
#ifdef DEBUG
                printf("read bd %d ch %d: SVMax:%f\n",iter->first,ii,data_float[ii]);
#endif
            }

        ret=GetChParameter_float(iter->first,4,"RUp",data_float);
        if(ret != CAENHV_OK)
        {
            printf("init: read par RUp error\n");
        }
        else
            for(ii=0;ii<4;ii++)
            {
                iter->second.ch[ii].RUp=data_float[ii];
#ifdef DEBUG
                printf("read bd %d ch %d: RUp:%f\n",iter->first,ii,data_float[ii]);
#endif
            }

        ret=GetChParameter_float(iter->first,4,"RDwn",data_float);
        if(ret != CAENHV_OK)
            printf("init: read par RDwn error\n");
        else
            for(ii=0;ii<4;ii++)
            {
                iter->second.ch[ii].RDwn=data_float[ii];
#ifdef DEBUG
                printf("read bd %d ch %d: RDwn:%f\n",iter->first,ii,data_float[ii]);
#endif
            }

        ret=GetChParameter_float(iter->first,4,"Trip",data_float);
        if(ret != CAENHV_OK)
            printf("init: read par Trip error\n");
        else
            for(ii=0;ii<4;ii++)
            {
                iter->second.ch[ii].Trip=data_float[ii];
#ifdef DEBUG
                printf("read bd %d ch %d: Trip:%f\n",iter->first,ii,data_float[ii]);
#endif
            }


        /////////////// bool type parameters
        for(ii=0;ii<4;ii++)
        {
            ret=GetChParameter_unsigned(iter->first,ii,"PDwn",data_unsigned);
            if(ret != CAENHV_OK)
                 printf("init: read par PDwn error\n");
            iter->second.ch[ii].PDwn=data_unsigned[0];
#ifdef DEBUG
            printf("read bd %d ch %d: PDwn:%d\n",iter->first,ii,data_unsigned[0]);
#endif
        }

        for(ii=0;ii<4;ii++)
        {
            ret=GetChParameter_unsigned(iter->first,ii,"Polarity",data_unsigned);
            iter->second.ch[ii].Pol=data_unsigned[0];
#ifdef DEBUG
            printf("read bd %d ch %d: Pol:%d\n",iter->first,ii,data_unsigned[0]);
#endif
        }


        for(ii=0;ii<4;ii++)
        {
            ret=GetChParameter_unsigned(iter->first,ii,"Pw",data_unsigned);
            iter->second.ch[ii].Pw=data_unsigned[0];
#ifdef DEBUG
            printf("read bd %d ch %d: PW:%d\n",iter->first,ii,data_unsigned[0]);
#endif
        }


        ///////////
        for(ii=0;ii<4;ii++)
        {

            ret=GetChParameter_int(iter->first,ii,"ChStatus",data_int);
            iter->second.ch[ii].ChStatus=data_int[0];
#ifdef DEBUG
            printf("read bd %d ch %d: ChStatus:%d\n",iter->first,ii,data_int[0]);
#endif
        }
        //// next board 
        iter++;
    }/// end while


    mutex->UnLock();
}



/// for web server

void Json_Array(TString& json_string, TString value)
{
    if(json_string.IsNull())
        json_string=TString::Format("[%s]",value.Data());
    else
        json_string=json_string.Insert(json_string.Last(']'),","+value);
}

void Json_ObjectArray(TString &json_string,TString id,TString value)
{
    if(json_string.IsNull())
    {
        json_string=TString::Format("{\"%s\":%s}",id.Data(),value.Data());
    }
    else if(value.IsNull())
    {
        TString str_tmp=TString::Format(",\"%s\":[]",id.Data());
        json_string=json_string.Insert(json_string.Last('}'),str_tmp);
    }
    else
    {
        TString str_tmp=TString::Format(",\"%s\":%s",id.Data(),value.Data());
        json_string=json_string.Insert(json_string.Last('}'),str_tmp);
    }
}

void Json_ObjectObject(TString &json_string,TString id,TString value)
{
    if(json_string.IsNull())
    {
        json_string=TString::Format("{\"%s\":%s}",id.Data(),value.Data());
    }
    else
    {
        TString str_tmp=TString::Format(",\"%s\":%s",id.Data(),value.Data());
        json_string=json_string.Insert(json_string.Last('}'),str_tmp);
    }
}


void Json_Object(TString &json_string,TString id,TString value)
{
    if(json_string.IsNull())
    {
        json_string=TString::Format("{\"%s\":\"%s\"}",id.Data(),value.Data());
    }
    else
    {
        TString str_tmp=TString::Format(",\"%s\":\"%s\"",id.Data(),value.Data());
        json_string=json_string.Insert(json_string.Last('}'),str_tmp);
    }
}

void Json_Object(TString &json_string,TString id,int value)
{

    if(json_string.IsNull())
    {
        json_string=TString::Format("{\"%s\":\"%d\"}",id.Data(),value);
    }
    else
    {
        TString str_tmp=TString::Format(",\"%s\":\"%d\"",id.Data(),value);
        json_string=json_string.Insert(json_string.Last('}'),str_tmp);
    }
}

void Json_Object(TString &json_string,TString id, ULong64_t value)
{

    if(json_string.IsNull())
    {
        json_string=TString::Format("{\"%s\":\"%llu\"}",id.Data(),value);
    }
    else
    {
        TString str_tmp=TString::Format(",\"%s\":\"%llu\"",id.Data(),value);
        json_string=json_string.Insert(json_string.Last('}'),str_tmp);
    }
}


void Json_Object(TString &json_string,TString id,double value)
{
    if(json_string.IsNull())
    {
        json_string=TString::Format("{\"%s\":\"%f\"}",id.Data(),value);
    }
    else
    {
        TString str_tmp=TString::Format(",\"%s\":\"%f\"",id.Data(),value);
        json_string=json_string.Insert(json_string.Last('}'),str_tmp);
    }
}

/// recv data from web
//
void Decode_Post(char* str_post,int mysock)
{
    ///post  method: str format usually is head and data, data format: name1=value1&name2=value2
    bool decode_OK=true;
    char* ptr=str_post;
    char* ptr_id;
    char* ptr_data;
    char* return_buffer=(char*)malloc(WEB_BUFFER_SIZE);
    TString info_buffer;
    int ii;

    while(decode_OK)
    {
        ptr_id=ptr;
        //search data
        if(ptr=strchr(ptr,'='))
        {
            *ptr='\0';
            ptr++;
            ptr_data=ptr;
        }
        else 
        {
            strcpy(ptr_id,"error");
        }
        ///seach end sign of data
        if(ptr=strchr(ptr,'&') )
        {
            *ptr='\0';
            ptr++;
        }
        else
            decode_OK=false;

#ifdef DEBUG
        printf("recv post info, id=%s,value=%s\n",ptr_id,ptr_data);
#endif

        // prepare the return string
        map<int,CAENHVBoardParameters>::iterator iter;
        if(strstr(ptr_id,"refresh"))
        {
            mutex->Lock();
            iter=m_handle.begin();
            TString str_json_array;
            TString str_json_object;
            TString str_json_monitor;
            str_json_array.Clear();
            str_json_monitor.Clear();
            while(iter != m_handle.end())
            {
                for(ii=0;ii<MAX_CH;ii++)
                {
                    str_json_object.Clear();
                    Json_Object(str_json_object,"ChID",iter->first*100+ii);
                    if(iter->first*100+ii == MWPC0_Pos_ch)
                        Json_Object(str_json_monitor,"MWPC0_Pos",iter->second.ch[ii].IMon);
                    else if(iter->first*100+ii == MWPC0_Neg_ch)
                        Json_Object(str_json_monitor,"MWPC0_Neg",iter->second.ch[ii].IMon);
                    else if(iter->first*100+ii == MWPC1_Pos_ch)
                        Json_Object(str_json_monitor,"MWPC1_Pos",iter->second.ch[ii].IMon);
                    else if(iter->first*100+ii == MWPC1_Neg_ch)
                        Json_Object(str_json_monitor,"MWPC1_Neg",iter->second.ch[ii].IMon);
                    else if(iter->first*100+ii == DSSD_Pos_ch)
                        Json_Object(str_json_monitor,"DSSD_Pos",iter->second.ch[ii].IMon);
                    else if(iter->first*100+ii == DSSD_Neg_ch)
                        Json_Object(str_json_monitor,"DSSD_Neg",iter->second.ch[ii].IMon);

                    Json_Object(str_json_object,"Name",iter->second.ch[ii].Name);
                    Json_Object(str_json_object,"VSet",TString::Format("%.1f",iter->second.ch[ii].VSet));
                    Json_Object(str_json_object,"ISet",TString::Format("%.2f",iter->second.ch[ii].ISet));
                    Json_Object(str_json_object,"VMon",TString::Format("%.2f",iter->second.ch[ii].VMon));
                    Json_Object(str_json_object,"IMon",TString::Format("%.3f",iter->second.ch[ii].IMon));
                    Json_Object(str_json_object,"SVMax",TString::Format("%.1f",iter->second.ch[ii].SVMax));
                    Json_Object(str_json_object,"RUp",TString::Format("%.0f",iter->second.ch[ii].RUp));
                    Json_Object(str_json_object,"RDwn",TString::Format("%.0f",iter->second.ch[ii].RDwn));
                    Json_Object(str_json_object,"Trip",TString::Format("%.0f",iter->second.ch[ii].Trip));
                    /// ch power swith
                    if(iter->second.ch[ii].Pw == 0)
                        Json_Object(str_json_object,"Pw","Off");
                    else
                        Json_Object(str_json_object,"Pw","On");

                    /// ch hv Polarity
                    if(iter->second.ch[ii].Pol == 0)
                        Json_Object(str_json_object,"Pol","Pos");
                    else
                        Json_Object(str_json_object,"Pol","Neg");

                    /// ch power off down way
                    if(iter->second.ch[ii].PDwn == 0)
                        Json_Object(str_json_object,"PDwn","Kill");
                    else
                        Json_Object(str_json_object,"PDwn","Ramp");

                    /// ch status
                    if(iter->second.ch[ii].ChStatus == 0)
                        Json_Object(str_json_object,"ChStatus","Off");
                    else if(iter->second.ch[ii].ChStatus == 1 || iter->second.ch[ii].ChStatus == 65)
                        Json_Object(str_json_object,"ChStatus","On");
                    else if(iter->second.ch[ii].ChStatus == 3)
                        Json_Object(str_json_object,"ChStatus","Up");
                    else if(iter->second.ch[ii].ChStatus == 5)
                        Json_Object(str_json_object,"ChStatus","Down");
                    else if((iter->second.ch[ii].ChStatus & 8) == 8)
                        Json_Object(str_json_object,"ChStatus","OVC");
                    else if((iter->second.ch[ii].ChStatus & 16) == 16)
                        Json_Object(str_json_object,"ChStatus","OVV");
                    else if((iter->second.ch[ii].ChStatus & 32) == 32)
                        Json_Object(str_json_object,"ChStatus","RUN");
                    else if(iter->second.ch[ii].ChStatus == 128)
                        Json_Object(str_json_object,"ChStatus","Trip");
                    else if(iter->second.ch[ii].ChStatus == 1024)
                        Json_Object(str_json_object,"ChStatus","Disable");
                    else if(iter->second.ch[ii].ChStatus == 2048)
                        Json_Object(str_json_object,"ChStatus","Kill");
                    else
                        Json_Object(str_json_object,"ChStatus","Error");
                    ////// for json array
                    Json_Array(str_json_array,str_json_object);
                }
                iter++;
            }
            mutex->UnLock();
            Json_Object(info_buffer,"curr_time",curr_time);
            Json_Object(info_buffer,"ButtonLock",html_lock);
            Json_ObjectObject(info_buffer,"Mon_Var",str_json_monitor);
            Json_ObjectArray(info_buffer,"HVParam",str_json_array);
        }
        else if(strstr(ptr_id,"ButtonLock"))
        {
            html_lock=ptr_data;
            Json_Object(info_buffer,ptr_id,ptr_data);
        }
        else
        {
            mutex->Lock();
            char* ptr_chid=ptr_id;
            char str_retid[50];
            strcpy(str_retid,ptr_id);
            ptr_id=strstr(ptr_id,".");
            if(ptr_id != NULL)
            {
                *ptr_id='\0';
                ptr_id++;
                int ChId=atoi(ptr_chid);
                int BdNum=(int)ChId/100;
                int ChNum=ChId%100;
                int ret;
#ifdef DEBUG
                printf("recv bd num:%d, chnunm:%d,value:%s\n",BdNum,ChNum,ptr_data);
#endif
                ////////////
                iter=m_handle.find(BdNum);
                if(strstr(ptr_id,"VSet"))
                {
                    float value=atof(ptr_data);
                    if(value > iter->second.ch[ChNum].SVMax)
                        value=iter->second.ch[ChNum].SVMax;
                    else if (value<0)
                        value=0;
                    ret=SetChParameter_float(BdNum,ChNum,"VSet",&value);
                    if(ret != CAENHV_OK)
                    {
                        printf("set parameters VSet error!!\n");
                    }
                    else
                        iter->second.ch[ChNum].VSet=value;
                    Json_Object(info_buffer,str_retid,TString::Format("%.1f",value));
                }
                else if(strstr(ptr_id,"Name"))
                {
                    //ret=SetChName(BdNum,ChNum,ptr_data);
                    strcpy(iter->second.ch[ChNum].Name,ptr_data);
                    Update_Chanme();
                    Json_Object(info_buffer,str_retid,ptr_data);
                }
                               else if(strstr(ptr_id,"ISet"))
                {
                    float value=atof(ptr_data);
                    float maxvalue;
                    CAENHV_GetChParamProp(iter->second.handle,0,ChNum,"ISet","Maxval",&maxvalue);
                    if(value>maxvalue)
                        value=maxvalue;
                    else if(value<0)
                        value=0;
                    ret=SetChParameter_float(BdNum,ChNum,"ISet",&value);
                    if(ret != CAENHV_OK)
                        printf("set parameters ISet error!!\n");
                    else
                        iter->second.ch[ChNum].ISet=value;
                    Json_Object(info_buffer,str_retid,TString::Format("%.2f",value));
                }
                else if(strstr(ptr_id,"SVMax"))
                {
                    float value=atof(ptr_data);
                    float maxvalue;
                    CAENHV_GetChParamProp(iter->second.handle,0,ChNum,"MaxV","Maxval",&maxvalue);
                    if(value>maxvalue)
                        value=maxvalue;
                    else if(value<0)
                        value=0;
                    ret=SetChParameter_float(BdNum,ChNum,"MaxV",&value);
                    if(ret != CAENHV_OK)
                        printf("set parameters MaxV error!!\n");
                    else
                        iter->second.ch[ChNum].SVMax=value;
                    Json_Object(info_buffer,str_retid,TString::Format("%.1f",value));
                }
                else if(strstr(ptr_id,"RUp"))
                {
                    float value=atof(ptr_data);
                    if(value<1)
                        value=1;
                    ret=SetChParameter_float(BdNum,ChNum,"RUp",&value);
                    if(ret != CAENHV_OK)
                        printf("set parameters RUp error!!\n");
                    else
                        iter->second.ch[ChNum].RUp=value;
                    Json_Object(info_buffer,str_retid,TString::Format("%.0f",value));
                }
                else if(strstr(ptr_id,"RDwn"))
                {
                    float value=atof(ptr_data);
                    if(value<1)
                        value=1;
                    ret=SetChParameter_float(BdNum,ChNum,"RDwn",&value);
                    if(ret != CAENHV_OK)
                        printf("set parameters RDwn error!!\n");
                    else
                        iter->second.ch[ChNum].RDwn=value;
                    Json_Object(info_buffer,str_retid,TString::Format("%.0f",value));
                }
                else if(strstr(ptr_id,"Trip"))
                {
                    float value=atof(ptr_data);
                    if(value<1)
                        value=1;
                    ret=SetChParameter_float(BdNum,ChNum,"Trip",&value);
                    if(ret != CAENHV_OK)
                        printf("set parameters Trip error!!\n");
                    else
                        iter->second.ch[ChNum].Trip=value;
                    Json_Object(info_buffer,str_retid,TString::Format("%.0f",value));
                }
                else if(strstr(ptr_id,"PDwn"))
                {
                    //bool value;
                    unsigned long value;
                    if(strstr(ptr_data,"Kill"))
                        value=0;
                    else
                        value=1;
                    ret=SetChParameter_unsigned(BdNum,ChNum,"PDwn",&value);
                    if(ret != CAENHV_OK)
                        printf("set parameters PDwn error!!\n");
                    else
                        iter->second.ch[ChNum].PDwn=value;
                    Json_Object(info_buffer,str_retid,ptr_data);
                }
                else if(strstr(ptr_id,"Pw"))
                {
                    //bool value;
                    unsigned long value;
                    if(strstr(ptr_data,"On"))
                        value=1;
                    else
                        value=0;
                    ret=SetChParameter_unsigned(BdNum,ChNum,"Pw",&value);
                    if(ret != CAENHV_OK)
                        printf("set parameters Pw error!!\n");
                    else
                        iter->second.ch[ChNum].Pw=value;
                    Json_Object(info_buffer,str_retid,ptr_data);
                }
            }
            else
                Json_Object(info_buffer,"error","OK");

            ///////finish 
            mutex->UnLock();
        }
    }

    // info_buffer="\'"+info_buffer+"\'";
    sprintf(return_buffer,Respond_HeadFormat,"text/plain",info_buffer.Length());
    int post_length=strlen(return_buffer)+info_buffer.Length();
    memcpy(return_buffer+strlen(return_buffer),info_buffer.Data(),info_buffer.Length());
    return_buffer[post_length]='\0';
    post_length=strlen(return_buffer);
#ifdef DEBUG
    printf("return info:length:%d\n\n%s\n",post_length,return_buffer);
#endif
    send(mysock, return_buffer, strlen(return_buffer), 0);

    ///finish
    free(return_buffer);
}
void Decode_Get(char* str,int mysock)
{
    ///get method: str format usually is /index.html?name=test&item=aaa, befor ? is path, behind is data field
    char* return_buffer=(char*)malloc(WEB_BUFFER_SIZE);
    char path[256]=".";
    int len_total=0;

    if(!strchr(str,'?'))
        sscanf(str,"%s",path+1);
    else
    {
        strncpy(path+1, str, strchr(str,'?')-str);
    }
    if(strcmp(path,"./")==0)
    {
        // now the html file in current fold, or use defaut dir use env varial?
        strcat(path,default_html_index);
    }
    else
    {
        /// other files, not used in our set, we just have one html file
    }

#ifdef DEBUG
    printf("decode get: decode path:%s\n",path);
#endif

    if(access(path,F_OK) !=0)
    {
        //send(mysock, NotFoundHeadFormat, strlen(NotFoundHeadFormat), 0);
        // return -1;
#ifdef DEBUG
        printf("can not find %s\n",path);
#endif
        memcpy(return_buffer,NotFoundHeadFormat,strlen(NotFoundHeadFormat));
        len_total=strlen(NotFoundHeadFormat);
    }
    else
    {
        FILE* fin;
        if((fin=fopen(path,"r")) == NULL)
        {
            memcpy(return_buffer,NotFoundHeadFormat,strlen(NotFoundHeadFormat));
            len_total=strlen(NotFoundHeadFormat);
            printf("open file error");
            fclose(fin);
        }
        else
        {
            fseek(fin,0,SEEK_END);
            int len=ftell(fin);
            fseek(fin,0,SEEK_SET);
            // write head
            if(strstr(path,".css"))
                sprintf(return_buffer,Respond_HeadFormat,"text/css",len);
            else if(strstr(path,".html"))
                sprintf(return_buffer,Respond_HeadFormat,"text/html",len);
            else if(strstr(path,".ico"))
            {
                sprintf(return_buffer,Respond_HeadFormat,"image/x-icon",len);
#ifdef DEBUG
                printf("ico length:%d\n",len);
#endif
            }
            else if(strstr(path,".png"))
            {
                sprintf(return_buffer,Respond_HeadFormat,"image/png",len);
#ifdef DEBUG 
                printf("png length:%d\n",len);
#endif
            }
            else if(strstr(path,".jpg")||strstr(path,".jpeg"))
            {
                sprintf(return_buffer,Respond_HeadFormat,"image/jpeg",len);
#ifdef DEBUG 
                printf("png length:%d\n",len);
#endif
            }
            else if(strstr(path,".gif"))
            {
                sprintf(return_buffer,Respond_HeadFormat,"image/gif",len);
#ifdef DEBUG 
                printf("png length:%d\n",len);
#endif
            }
            else if(strstr(path,".js"))
            {
                sprintf(return_buffer,Respond_HeadFormat,"text/plain",len);
#ifdef DEBUG 
                printf("png length:%d\n",len);
#endif
            }

            // read content
            len_total=strlen(return_buffer)+len;
            fread(return_buffer+strlen(return_buffer),1,len,fin);
            fclose(fin);
        }
    } //get html data
    /// send data
#ifdef DEBUG
    //    printf("%s\n\n\n",return_buffer);
#endif
    send(mysock, return_buffer, len_total, 0);
    free(return_buffer);
}

//static void* socket_test(void* socket_in)
void* Socket_Recv(void* socket_in)
{
    fd_set readfds;
    int mysock=*(int*)(socket_in);
    free(socket_in);
    int i;

    char* net_buffer=(char*)malloc(WEB_BUFFER_SIZE);
    //char net_buffer[WEB_BUFFER_SIZE];
    char boundary[256];
    struct timeval timeout;
    int len = 0;
    int header_length = 0;
    int content_length = 0;
    int n_error = 0;
    int status=0;
    int loop;

    while(1) // read all data from request
    {
        FD_ZERO(&readfds);
        FD_SET(mysock, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 5000;
        loop=0;
        do {
            status = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);
            /* if an alarm signal was cought, restart with reduced timeout */
        } while (status == -1 && errno == EINTR && (++loop < 2));

        memset(net_buffer, 0, strlen(net_buffer)*sizeof(char));
        if (status > 0 && FD_ISSET(mysock, &readfds))
            i = recv(mysock, net_buffer+len, WEB_BUFFER_SIZE-len , 0);
        else
        {
#ifdef DEBUG
            printf("select error: status:%d, errno:%d,FSSET:%x,socket:%x\n",status,errno,FD_ISSET(mysock, &readfds),mysock);
#endif
            break;
        }
        //check recv ret 
        if(i < 0)
        {
#ifdef DEBUG
            printf("recv error \n");
#endif
            break;
        }
        else if(i==0)
        {
            n_error++;
#ifdef DEBUG 
            //    if(n_error%10 == 0)
            //      printf("n_error:%s in socket:%x\n",n_error,mysock);
#endif
            if(n_error > 10)
            {
#ifdef DEBUG
                printf("error >10 \n");
#endif
                send(mysock, NotFoundHeadFormat, strlen(NotFoundHeadFormat), 0);
                goto finish;
            }
            continue;
        }

        //recv data success
        len += i;
        if(len>0 && len < WEB_BUFFER_SIZE)
            net_buffer[len]='\0';
        //data larger than buffer
        else if  (len >= sizeof(net_buffer))
        {
            /* drain incoming remaining data */
            do {
                FD_ZERO(&readfds);
                FD_SET(mysock, &readfds);

                timeout.tv_sec = 0;
                timeout.tv_usec = 5000;
                loop = 0;
                do {
                    status = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);
                    /* if an alarm signal was cought, restart with reduced timeout */
                } while (status == -1 && errno == EINTR && (++loop < 2));
                if (status <= 0)
                    break;
                if (!FD_ISSET(mysock, &readfds))
                    break;
                i = recv(mysock, net_buffer, sizeof(net_buffer), 0);
            } while (i > 0);
            printf("socket buffer is tow small\n");
            send(mysock, NotFoundHeadFormat,strlen(NotFoundHeadFormat), 0);
            break;
        }
#ifdef DEBUG
        printf("pid=%d: socket recv data: len=%d\n%s \n========\n\n",getpid,len, net_buffer);
#endif
        ////////// finish recv from web client ///

        /////// check  recv data  is finish ?////
        char* ptr;
        if (strstr(net_buffer, "\r\n\r\n")) 
        {
            if (strstr(net_buffer, "GET") != NULL && strstr(net_buffer, "POST") == NULL) 
            {
                if (len > 4 && strcmp(&net_buffer[len - 4], "\r\n\r\n") == 0)
                    break;
                if (len > 6 && strcmp(&net_buffer[len - 6], "\r\r\n\r\r\n") == 0)
                    break;
            } 
            else if (strstr(net_buffer, "POST") != NULL) 
            {
#ifdef DEBUG
                printf("recv POST info:\n%s\n=====\r\n",net_buffer);
#endif
                if (header_length == 0) 
                {
                    /* extract header and content length */
                    if (ptr=strstr(net_buffer, "Content-Length:"))
                        content_length = atoi(ptr + 15);
                    else if (strstr(net_buffer, "Content-length:"))
                        content_length = atoi(ptr + 15);
#ifdef DEBUG
                    printf("decode content-length: %d\n", content_length);
#endif
                    boundary[0] = 0;
                    if (ptr=strstr(net_buffer, "boundary=")) {
                        strncpy(boundary, ptr + 9, sizeof(boundary));
                        if (strchr(boundary, '\r'))
                            *strchr(boundary, '\r') = 0;
                    }

                    if (ptr = strstr(net_buffer, "\r\n\r\n"))
                        header_length = ptr -   net_buffer + 4;

                    if (ptr = strstr(net_buffer, "\r\r\n\r\r\n"))
                        header_length = ptr -  net_buffer + 6;
                }
                if (header_length > 0 && (int) len >= header_length + content_length) 
                {
                    //if post, set net buffer as two string, one for head, one for content
                    if (header_length)
                        net_buffer[header_length - 1] = 0;  
#ifdef DEBUG
                    printf("post content:%s\n====\n",net_buffer+header_length);
#endif
                    break;
                }
            }// if get else post 
            //other request discard
            else
            {
#ifdef DEBUG
                printf("request type error\n");
#endif
                break;
                goto finish;
            }
        } // check data finish 
        else
        {
#ifdef DEBUG
            printf("recv data is not complete\n");
#endif
        }
    } // recv request finish
    //// decode request info and respond info

    if (strstr(net_buffer, "GET") != NULL && strstr(net_buffer, "POST") == NULL) 
    {

#ifdef DEBUG
        printf("start decode request\n======\n");
#endif
        //get url;
        //sscanf(net_buffer+3,"%s",return_buffer);
        *(strstr(net_buffer,"HTTP")-1)=0;
#ifdef DEBUG
        printf("url line:%s\n\n",net_buffer);
#endif
        Decode_Get(net_buffer+4,mysock);
    }
    else if(strstr(net_buffer, "POST") != NULL)
    {
        ///do something
        Decode_Post(net_buffer+header_length,mysock);
    }

finish:

#ifdef DEBUG
    printf("finish one tcp %x\n",mysock);
#endif
    close(mysock);
    free(net_buffer);
    return 0;
}

/// recv socket connect request
void* WebServer_loop(void* argv)
{
    int status ;
    struct sockaddr_in bind_addr, acc_addr;
    int lsock,flag;
    unsigned int len;
    fd_set readfds;
    struct timeval timeout;


    /* create a new socket as web server */
    lsock = socket(AF_INET, SOCK_STREAM, 0);
    if (lsock == -1) {
        printf("Cannot create socket\n");
        return 0;
    }
    /* bind local node name and port to socket */
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons((short) tcp_port);
    /* try reusing address */
    flag = 1;
    setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, (char *) &flag, sizeof(int));
    status = bind(lsock, (struct sockaddr *) &bind_addr, sizeof(bind_addr));
    if (status < 0) {
        printf ("Cannot bind to port %d.\nPlease try later or use the \"-p\" flag to specify a different port\n", tcp_port);
        return 0;
    }
    /* listen for connection */
    status = listen(lsock, SOMAXCONN);
    if (status < 0) {
        printf("listen errno %d (%s), bye!\n", errno, strerror(errno));
        return 0;
    }
    TString str_tmp=TString::Format("Start WebServer @port:%d", tcp_port);

    printf("*****%s*****\n",str_tmp.Data());

    //////// wait for connect from web 
    do {
        FD_ZERO(&readfds);
        FD_SET(lsock, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 1000;
        do{
            status = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);
            /* if an alarm signal was cought, restart with reduced timeout */
        } while (status == -1 && errno == EINTR);

        if (FD_ISSET(lsock, &readfds))
        {
            len = sizeof(acc_addr);
            int* websock =(int*) malloc(sizeof(int));
            *websock= accept(lsock, (struct sockaddr *) &acc_addr, (socklen_t *)&len);
#ifdef DEBUG
            printf("time:%d,socket loop: create new socket connection,socket:%x\n",time(NULL),*websock);
#endif
            Socket_Recv(websock);
            // pthread_create(&pid,NULL,(void(*)(void*))&Socket_Recv,(void*)(websock));
            // pthread_detach(pid);
        }
    } while (true);  //while listen client and accept tcp connect

    return 0;
}

////////////
int main(int argc, const char *argv[])
{
    /* avoid getting killed by "Broken pipe" signals */
    setbuf(stdout, NULL);
	setbuf(stderr, NULL);
    signal(SIGPIPE, SIG_IGN);

    ////// init HV parameters//
    CAENHVRESULT  ret;
    char     arg[256];
    char     *userName="admin";
    char     *passwd="admin";
    char     comport[36];
    char     str_tmp2[256];
    char     BoundRate[36]; 
    char     str_tmp[256];
    char     str_tmp1[256];
    int      i,index, link;
    int      sysHndl=-1;
    int      sysType=-1;
    int      board_num=0;
    int      ch_num=0;
    int      ChainAdd[30]={0};

    /// read setup parameters
    FILE* fp_in;
    if((fp_in=fopen("./setup.txt","r")) == NULL)
    {
        printf("open file setup.txt error!\n");
        return -1;
    }
    while(fgets(str_tmp,256,fp_in) != NULL)
    {
        if(strcmp(str_tmp,"\r\n")==0)
            continue;
        if(strstr(str_tmp,"type"))
        {
            sscanf(str_tmp,"%s %s",str_tmp1,arg);
            if(strstr(arg,"N1470"))
                sysType=N1470;
            else
            {
                printf("borad type should be N1470 now\n");
                exit(-1);
            }
        }
        else if(strstr(str_tmp,"link"))
        {
            sscanf(str_tmp,"%s %s",str_tmp1,arg);
            if(strstr(arg,"USB_VCP"))
                link=LINKTYPE_USB_VCP;
            else
            {
                printf("borad type should be USB-VCP now\n");
                exit(-1);
            }
        }
        else if(strstr(str_tmp,"ComPort"))
        {
			FILE* fp_pipe=popen("ls /dev/ttyUSB* 2>&1","r");
			if(fp_pipe == NULL)
			{
				printf("open pipe error\n");
				sscanf(str_tmp,"%s %s",str_tmp1,comport);
			}
			else
			{
				int n=fread(str_tmp,sizeof(char),sizeof(str_tmp),fp_pipe);
				if(n>25)
					sscanf(str_tmp,"%s %s",str_tmp1,comport);
				else
				{
					sscanf(str_tmp,"%s",str_tmp1);
					strcpy(comport,str_tmp1+5);
				}
				pclose(fp_pipe);
			}
        }
        else if(strstr(str_tmp,"BoundRate"))
        {
            sscanf(str_tmp,"%s %s",str_tmp1,BoundRate);
        }
        else if(strstr(str_tmp,"BoardNum"))
        {
            sscanf(str_tmp,"%s %d",str_tmp1,&board_num);
            if(board_num > MAX_BOARDS)
            {
                printf("borad number larger than MAX_BOARDS\n");
                exit(-1);
            }
        }
        else if(strstr(str_tmp,"MWPC0_Pos_ch"))
            sscanf(str_tmp,"%s %d",str_tmp1,&MWPC0_Pos_ch);
        else if(strstr(str_tmp,"MWPC0_Neg_ch"))
            sscanf(str_tmp,"%s %d",str_tmp1,&MWPC0_Neg_ch);
        else if(strstr(str_tmp,"MWPC1_Pos_ch"))
            sscanf(str_tmp,"%s %d",str_tmp1,&MWPC1_Pos_ch);
        else if(strstr(str_tmp,"MWPC1_Neg_ch"))
            sscanf(str_tmp,"%s %d",str_tmp1,&MWPC1_Neg_ch);
        else if(strstr(str_tmp,"DSSD_Pos_ch"))
            sscanf(str_tmp,"%s %d",str_tmp1,&DSSD_Pos_ch);
        else if(strstr(str_tmp,"DSSD_Neg_ch"))
            sscanf(str_tmp,"%s %d",str_tmp1,&DSSD_Neg_ch);
        else if(strstr(str_tmp,"ChainAddNum"))
        {
            int offset=0;
            int indexnum=0;
            int readbytenum=0;
            sscanf(str_tmp+offset,"%s%*c%n",str_tmp1,&readbytenum);
            offset+=readbytenum;
            while(sscanf(str_tmp+offset,"%d%*c%n",&ChainAdd[indexnum],&readbytenum) == 1)
            {

#ifdef DEBUG
                    printf("str:%s,com:%s\n",str_tmp+offset,comport);
#endif
                offset+=readbytenum;
                if(link == LINKTYPE_USB_VCP)
                {
                    sprintf(arg,"%s_%s_%s_%s_%s_%d",comport,BoundRate,"8","0","0",ChainAdd[indexnum]);
#ifdef DEBUG
                    printf("arg str:%s\n",arg);
#endif
                    ret = CAENHV_InitSystem((CAENHV_SYSTEM_TYPE_t)sysType, link, arg, userName, passwd, &sysHndl);
                    if(ret == CAENHV_OK)
                    {
#ifdef DEBUG
                        printf("open board %d succ!!\n",ChainAdd[indexnum]);
#endif
                        CAENHVBoardParameters bd_data={NULL};
                        bd_data.handle=sysHndl;
                        m_handle.insert(pair<int,CAENHVBoardParameters>(ChainAdd[indexnum],bd_data));
                    }
                    else
                    {
                        printf("open board %d error in port: %s, plz check!\n",ChainAdd[indexnum],comport);
                        return -1;
                    }
                }
                indexnum++;
            }
            if(indexnum != board_num)
            {
                printf("the chain bdnum %d != total board_num %d, plz check!!\n",indexnum,board_num);
                return -1;
            }
        }
    } //read setup file
    fclose(fp_in);
#ifdef DEBUG
printf ("DSSD_Pos_Ch:%d\n", DSSD_Pos_ch);
#endif

    if((fp_in=fopen("./chname.txt","r")) == NULL)
    {
        printf("open file chname.txt error!\n");
        return -1;
    }

    map<int,CAENHVBoardParameters>::iterator iter;
    while(fgets(str_tmp,256,fp_in) != NULL)
    {
        if(strcmp(str_tmp,"\r\n")==0)
            continue;
        sscanf(str_tmp,"%d %d %s",&board_num,&ch_num,str_tmp1);
#ifdef DEBUG
        printf("%d %d %s\n",board_num,ch_num,str_tmp1);
#endif
        iter=m_handle.find(board_num);
        if(iter == m_handle.end())
            continue;
        sprintf(iter->second.ch[ch_num].Name,"%s",str_tmp1);
    }
    fclose(fp_in);

#ifdef DEBUG
    printf("start r/w\n");
#endif
    ///start web monitor server
    mutex=new TMutex();
    mutex->CleanUp();
    Init_Parameters();
    html_lock="UnLock";
//    return -1;

    int port=tcp_port;
//    WebServer_loop(&port);
    pthread_t pid;
    if(pthread_create(&pid,NULL,WebServer_loop,(void*)(port)) == -1)
    {
        printf("start webserver errori, plz check port%d\n",port);
        return -1;
    }
    //pthread_detach(pid);
    ///////////// update device info
#ifdef DEBUG
    printf("period read dev info\n");
#endif
    time_t t1,t2;
    t1=time(NULL);
    time_pre=t1;
    char* str=ctime(&time_pre);
    str[strlen(str)-1]='\0';
    curr_time=str;

    while(true)
    {
        t2=time(NULL);
        if(t2>t1+refresh_time)
        {
            Update_Parameters();
            t1=t2;
        }
        else
            usleep(100000);
    }
    return 0;
}

