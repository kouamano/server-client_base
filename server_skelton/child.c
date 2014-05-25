#include    <stdio.h>
#include    <string.h>
#include    <ctype.h>
#include    <stdlib.h>
#include    <sys/socket.h>
#include    <netinet/in.h>
#include    <netinet/tcp.h>
#include    <netdb.h>
#include    <signal.h>
#include    <errno.h>
#include    <fcntl.h>

/*#define    DB(x)    x*/
#define    DB(x)

extern char *VersionString();
extern int ExecRecvCommand(char *buf,int len,int *command_len);

/* �Хåե��������ñ�� */
#define    BUF_ALLOC_SIZE    (1024)

/* �Хåե���¤�� */
typedef    struct {
    char    *buf;
    int    size;
    int    len;
}DATA_BUF;

/* �����Хåե� */
DATA_BUF    SendBuf={NULL,0,0};

/* �����Хåե� */
DATA_BUF    RecvBuf={NULL,0,0};

int    SetBlock(int fd,int flag)
{
int    flags;

    if((flags=fcntl(fd,F_GETFL,0))==-1){
        perror("fcntl");
        return(-1);
    }
    if(flag==0){
        fcntl(fd,F_SETFL,flags|O_NDELAY);
    }
    else if(flag==1){
        fcntl(fd,F_SETFL,flags&~O_NDELAY);
    }
    return (0);
}

int DataPrint(FILE *fp,char *buf,int len)
{
int    i;

    for(i=0;i<len;i++){
        if(isascii(buf[i])&&isprint(buf[i])){
            fputc(buf[i],fp);
        }
        else{
            fprintf(fp,"[%02x]",(unsigned int)(buf[i]&0xFF));
        }
    }

    return(0);
}

int IntoDataBuf(DATA_BUF *data_buf,char *add_buf,int add_len)
{
    DB(fprintf(stderr,"IntoDataBuf()\n"));

    if(add_len<=0){
        DB(fprintf(stderr,"add_len==0\n"));
        return(0);
    }

    if(data_buf->size==0){
        /* �Хåե������ξ�� */
        data_buf->size=BUF_ALLOC_SIZE;
        data_buf->buf=(char *)malloc(data_buf->size);
        /* �Хåե��˥��ԡ� */
        memcpy(data_buf->buf,add_buf,add_len);
        data_buf->len=add_len;
    }
    else{
        if(data_buf->len+add_len>=data_buf->size){
            /* ��ץ�������������ƥ�������Ķ������ */
            data_buf->size+=BUF_ALLOC_SIZE;
            data_buf->buf=(char *)realloc(data_buf->buf,data_buf->size);
        }
        /* �Хåե����ɲ� */
        memcpy(data_buf->buf+data_buf->len,add_buf,add_len);
        data_buf->len+=add_len;
    }

    DB(fprintf(stderr,"size=%d,len=%d : {",data_buf->size,data_buf->len));
    DB(DataPrint(stderr,data_buf->buf,data_buf->len));
    DB(fprintf(stderr,"}\n"));

    return(0);
}

int IntoSendDataBuf(char *add_buf,int add_len)
{
        IntoDataBuf(&SendBuf,add_buf,add_len);

        return(0);
}

int ShiftDataBuf(DATA_BUF *data_buf,int shift_len)
{
int    i,len;
char   *ptr_new,*ptr_old;

    DB(fprintf(stderr,"ShiftDataBuf()\n"));

    if(data_buf->len==0||shift_len<=0){
        DB(fprintf(stderr,"len=%d,shift_len=%d:No Shift\n",
                                    data_buf->len,shift_len));
        return(0);
    }

    len=data_buf->len-shift_len;
    if(len<=0){
        /* ���ˤʤ��� */
        free(data_buf->buf);
        data_buf->buf=NULL;
        data_buf->size=0;
        data_buf->len=0;
    }
    else{
        /* �Ĥ꤬������ */
        ptr_new=data_buf->buf;
        ptr_old=data_buf->buf+shift_len;
        for(i=0;i<len;i++){
            *ptr_new=*ptr_old;
            ptr_new++;
            ptr_old++;
        }
        data_buf->len=len;
    }

    DB(fprintf(stderr,"size=%d,len=%d : {",data_buf->size,data_buf->len));
    DB(DataPrint(stderr,data_buf->buf,data_buf->len));
    DB(fprintf(stderr,"}\n"));

    return(0);
}

int RecvData(int acc)
{
char   buf[4096];
int    len;
int    end_flag,command_len;

    DB(fprintf(stderr,"RecvData()\n"));

    len=recv(acc,buf,sizeof(buf),0);
    if(len==0){    /* �����å����� */
        DB(fprintf(stderr,"Socket Broken.\n"));
        return(1);
    }
    DB(fprintf(stderr,"recv %d bytes : {",len));
    DB(DataPrint(stderr,buf,len));
    DB(fprintf(stderr,"}\n"));

    /* �����Хåե��˥ǡ�����Ǽ */
    IntoDataBuf(&RecvBuf,buf,len);

    while(1){
        /* ���ޥ�ɼ¹� */
        end_flag=ExecRecvCommand(RecvBuf.buf,RecvBuf.len,&command_len);
        if(end_flag==1){    /* ��λ���ޥ�� */
            break;
        }
        if(command_len==-1){    /* ���ޥ����Ω���� */
            break;
        }
        /* �¹Ԥ������ޥ��ʬ�������Хåե��Υǡ����򤺤餹 */
        ShiftDataBuf(&RecvBuf,command_len);
    }

    if(end_flag==1){    /* ��λ���ޥ�� */
        return(1);
    }
    else{
        return(0);
    }
}

int SendData(int acc)
{
int    len;

    DB(fprintf(stderr,"SendData()\n"));

    len=send(acc,SendBuf.buf,SendBuf.len,0);
    DB(fprintf(stderr,"send %d bytes : {",len));
    DB(DataPrint(stderr,SendBuf.buf,len));
    DB(fprintf(stderr,"}\n"));

    /* �����Ǥ���ʬ�������Хåե��Υǡ����򤺤餹 */
    if(len>0){
        ShiftDataBuf(&SendBuf,len);
    }

    return(0);
}

int ChildLoop(int acc,char *client_address)
{
fd_set    read_mask,write_mask,mask;
int    width;
struct timeval    timeout;
int    end_flag;
char   buf[80];

    DB(fprintf(stderr,"ChildLoop()\n"));

    /* �Υ�֥�å���I/O */
    SetBlock(acc,0);

    /* select()�ѥޥ��������� */
    width=0;
    FD_ZERO(&mask);
    FD_SET(acc,&mask);
    width=acc+1;

    /* ��������ǡ��� */
    sprintf(buf,"%s Ready.\r\n",VersionString());
    IntoSendDataBuf(buf,strlen(buf));

    end_flag=0;
    while(1){
        read_mask=mask;
        if(SendBuf.len>0){    /* �����ǡ����������� */
            write_mask=mask;
        }
        else{
            FD_ZERO(&write_mask);
        }
        timeout.tv_sec=10;    /* 10�äǥ����ॢ���� */
        timeout.tv_usec=0;
        switch(select(width,(fd_set *)&read_mask,&write_mask,NULL,&timeout)){
            case    -1:
                if(errno!=EINTR){
                    perror("select");
                }
                break;
            case    0:
                break;
            default:
                /* �ǡ���������ǥ��ʤ�����������ޥ��ǧ�����¹� */
                if(FD_ISSET(acc,&read_mask)){
                    end_flag=RecvData(acc);
                }
                /* �ǡ���������ǥ��ʤ������Хåե������Ǥʤ�������� */
                if(FD_ISSET(acc,&write_mask)){
                    SendData(acc);
                }
                break;
        }
        /* ���ޥ�ɤ���λ���ޥ�ɤʤ�֥졼�� */
        if(end_flag==1){
            break;
        }
    }

    return(0);
}

