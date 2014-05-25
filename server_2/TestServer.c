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
#define VERSION "Server version 0.1"
#define BUFF_SIZE 512
#define LONG_BUFF_SIZE 4096

int Soc=0;

#include "server_func.c"

int main(){
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
