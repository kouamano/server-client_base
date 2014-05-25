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

#define DB(x) x
#define VERSION "Server version 0.1\n"
#define SHORT_BUFF_SIZE 128
#define BUFF_SIZE 512
#define LONG_BUFF_SIZE 4096
#define HUGE_BUFF_SIZE 500000 //0.5MB
#define TIMEOUT_S 10
#define TIMEOUT_US 0

//typedef
typedef struct {
        char *buf;
        int size;
        int len;
}DATA_BUF;

//init buf
DATA_BUF SendBuf={NULL,0,0};
DATA_BUF RecvBuf={NULL,0,0};
DATA_BUF CommBuf={NULL,0,0};

int Soc=0;

#include "server_func.c"
#include "childloop.c"
#include "mainloop.c"

int main(){
	/*allocation*/
	/*RecvBuf*/
	RecvBuf.len = 0;
	RecvBuf.size = SHORT_BUFF_SIZE;
	if((RecvBuf.buf = malloc(sizeof(char) * RecvBuf.size)) == NULL){perror("malloc"); exit(0);}
	RecvBuf.buf[0] = '\0';
	/*CommBuf*/
	CommBuf.len = 0;
	//CommBuf.size = SHORT_BUFF_SIZE;
	CommBuf.size = LONG_BUFF_SIZE;
	//CommBuf.size = HUGE_BUFF_SIZE;
	if((CommBuf.buf = malloc(sizeof(char) * CommBuf.size)) == NULL){perror("malloc"); exit(0);}
	CommBuf.buf[0] = '\0';

	/*version*/
	PrintVersion();
	/*init parametas*/
	InitParam();
	/*init socket*/
	//InitSocket();
	if(InitSocket() == -1){
		return(-1);
	}
	/*init signal*/
	InitSignal();
	/*main loop*/
	MainLoop();
	/*close socket*/
	CloseSocket();

	return(0);
}
