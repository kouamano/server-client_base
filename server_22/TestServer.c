//TestServer
//include
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

//define
#define DB(X) X
#define SERVICE_NAME "TestServer"
#define VERSION "Server version 0.1-22\n"
#define TIMEOUT_S 10
#define TIMEOUT_US 0
#define SHORT_BUF_SIZE 256
#define BUF_SIZE 1024
#define LONG_BUF_SIZE 4096
#define HUGE_BUF_SIZE 1072693248 //1024 * 1024 * 1023

//typedef
typedef struct {
	char *buf;
	size_t size;
	int len;
}DATA_BUF;

//global vars
static DATA_BUF RecvBuf={NULL,0,0}; //static
static DATA_BUF CommBuf={NULL,0,0}; //static
static DATA_BUF SendBuf={NULL,0,0}; //alloc-free
static DATA_BUF RsltBuf={NULL,0,0}; //alloc-free
static int Soc = 0;

//my include
#include "/home/pub/include/alloc.c"
#include "server_func.c"
#include "childloop.c"
#include "mainloop.c"

//top main
int main(int argc, char **argv){
	//init global vars
	InitBuf(&RecvBuf,SHORT_BUF_SIZE);
	InitBuf(&CommBuf,LONG_BUF_SIZE);

	//init socket
	if(InitSocket(SERVICE_NAME) == -1){
		return(-1);
	}

	//init signal - bind()
	InitSignal();

	//listen
	listen(Soc,SOMAXCONN);

	//mainloop - accept(), fork(), ChildLoop()
	MainLoop();

	//close socket
	CloseSocket();

	return(0);
}
