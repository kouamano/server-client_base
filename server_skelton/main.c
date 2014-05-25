#include    <stdio.h>
#include    <string.h>
#include    <ctype.h>
#include    <stdlib.h>
#include    <sys/types.h>
#include    <sys/socket.h>
#include    <sys/wait.h>
#include    <netinet/in.h>
#include    <netinet/tcp.h>
#include    <arpa/inet.h>
#include    <netdb.h>
#include    <signal.h>
#include    <unistd.h>
#include    <errno.h>

#define    DB(x)    x

#define    VERSION    "Server version 0.1"

int ChildLoop(int acc,char *client_address);

int    Soc=0;

char *VersionString()
{
    return(VERSION);
}

int PrintVersion()
{
    DB(fprintf(stderr,"PrintVersion()\n"));

    printf("%s\n",VersionString());

    return(0);
}

int InitParam()
{
    DB(fprintf(stderr,"InitParam()\n"));

    return(0);
}

int InitSocket()
{
struct  servent    *se;
struct  sockaddr_in    me;
int     opt;

    DB(fprintf(stderr,"InitSocket()\n"));

    /* サービス情報の取得 */
    if((se=getservbyname("TestServer","tcp"))==NULL){
        perror("getservbyname");
        return(-1);
    }
    DB(fprintf(stderr,"getservbyname:%d\n",ntohs(se->s_port)));

    /* ソケットの生成 */
    if((Soc=socket(AF_INET,SOCK_STREAM,0))<0){
        perror("socket");
        return(-1);
    }
    DB(fprintf(stderr,"socket:%d\n",Soc));

    /* ソケットオプションのセット */
    opt=1;
    if(setsockopt(Soc,SOL_SOCKET,SO_REUSEADDR,(char *)&opt,sizeof(int))!=0){
        perror("setsockopt");
        return(-1);
    }
    DB(fprintf(stderr,"setsockopt:OK\n"));

    /* バインド */
    memset((char *)&me,0,sizeof(me));
    me.sin_family=AF_INET;
    me.sin_port=se->s_port;
    if(bind(Soc,(struct sockaddr *)&me,sizeof(me))==-1){
        perror("bind");
        return(-1);
    }
    DB(fprintf(stderr,"bind:OK\n"));

    return(0);
}

int CloseSocket()
{
    DB(fprintf(stderr,"CloseSocket()\n"));

    if(Soc>0){
        /* ソケットのクローズ */
        close(Soc);
    }
    Soc=0;

    return(0);
}

void EndSignal(int sig)
{
    DB(fprintf(stderr,"EndSignal()\n"));

    CloseSocket();

    exit(0);
}

void EndChild(int sig)
{
int    pid;

    pid=wait(0);
    DB(fprintf(stderr,"wait:pid=%d\n",pid));

    signal(SIGCLD,EndChild);
}

int InitSignal()
{
    DB(fprintf(stderr,"InitSignal()\n"));

    /* シグナルの設定 */
    signal(SIGINT,EndSignal);
    signal(SIGQUIT,EndSignal);
    signal(SIGTERM,EndSignal);
    signal(SIGCLD,EndChild);

    return(0);
}

int MainLoop()
{
int    acc;
struct sockaddr_in    from;
int    len;
char   client_address[80];
int    client_port;
int    pid;

    DB(fprintf(stderr,"MainLoop()\n"));

    /* ソケット接続受付準備 */
    listen(Soc,SOMAXCONN);

    /* 接続受付ループ */
    while(1){
        len=sizeof(from);
        /* 接続受付 */
        acc=accept(Soc,(struct sockaddr *)&from,&len);
        if(acc<0){
            if(errno!=EINTR){
                /* エラーで割り込み以外の場合 */
                perror("accept");
            }
            continue;
        }
	strcpy(client_address,inet_ntoa(from.sin_addr));
        client_port=ntohs(from.sin_port);
        DB(fprintf(stderr,"accept:%s:%d\n",client_address,client_port));
        if((pid=fork())==0){
            /* 子プロセス */
            close(Soc);
            Soc=0;
            /* 対クライアント処理 */
            ChildLoop(acc,client_address);
            exit(0);
        }
        /* 親プロセスではクライアントと通信しないのですぐクローズ */
        close(acc);
        acc=0;
    }

    return(0);
}

int main()
{
    /* バージョンの表示 */
    PrintVersion();

    /* パラメータ設定 */
    InitParam();

    /* ソケットの初期設定 */
    if(InitSocket()==-1){
        return(-1);
    }

    /* シグナルの設定 */
    InitSignal();

    /* メインループ */
    MainLoop();

    /* ソケットのクローズ */
    CloseSocket();

    return(0);
}

