#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#define DB(X) X
#define SERVICE_NAME "TestServer"
#define VERSION "Server version 0.1-16\n"
#define SHORT_BUFF_SIZE 128
#define BUFF_SIZE 512
#define LONG_BUFF_SIZE 4096
#define HUGE_BUFF_SIZE 1072693248 //1024 * 1024 * 1023Byte
#define TIMEOUT_S 10
#define TIMEOUT_US 0

//typedef
typedef struct {
        char *buf;
        int size;
        int len;
}DATA_BUF;

//init buf
static DATA_BUF RecvBuf={NULL,0,0}; //static
static DATA_BUF CommBuf={NULL,0,0}; //static
static DATA_BUF SendBuf={NULL,0,0}; //alloc-free
static DATA_BUF RsltBuf={NULL,0,0}; //alloc-free

static int Soc=0;

#include "server_func.c"
#include "childloop.c"
#include "mainloop.c"

int main(){
	//buff allocation
	//RecvBuf
	RecvBuf.len = 0;
	RecvBuf.size = SHORT_BUFF_SIZE;
	if((RecvBuf.buf = malloc(sizeof(char) * RecvBuf.size)) == NULL){perror("malloc"); exit(0);}
	RecvBuf.buf[0] = '\0';
	//CommBuf
	CommBuf.len = 0;
	CommBuf.size = LONG_BUFF_SIZE;
	if((CommBuf.buf = malloc(sizeof(char) * CommBuf.size)) == NULL){perror("malloc"); exit(0);}
	CommBuf.buf[0] = '\0';

	//init sock
	if(InitSocket(SERVICE_NAME) == -1){
		return(-1);
	}

	//init signal
	InitSignal();

	//listen
        listen(Soc,SOMAXCONN);

	//main loop - accept(), fork()
	MainLoop();

	//close socket
	CloseSocket();

	return(0);
}
