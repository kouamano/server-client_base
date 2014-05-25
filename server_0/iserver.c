#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>

#define DB(X) X
#define PORT 19999
#define BUF_SIZE 1024

int main(){
	int i;
	int fd_1;
	int fd_2;
	struct sockaddr_in saddr;
	struct sockaddr_in caddr;
	unsigned int len;
	int retlen;
	char buf[BUF_SIZE];
	char sendbuf[BUF_SIZE];
	int pid;
	int msglen;

	/*initialize*/
	bzero((char *)&saddr, sizeof(saddr));
	bzero((char *)&caddr, sizeof(caddr));
	
	/*socket*/
	if((fd_1 = socket(AF_INET,SOCK_STREAM,0)) < 0){
		perror("socket\n");
		exit(1);
	}
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_port = htons(PORT);

	/*bind*/
	if(bind(fd_1,(struct sockaddr *)&saddr,sizeof(saddr)) < 0){
		perror("bind");
		exit(1);
	}

	/*listen*/
	if(listen(fd_1,1) < 0){
		perror("listen");
	}

	/*while for accept and fork*/
	len = sizeof(caddr);
	for(;;){
		if((fd_2 = accept(fd_1,(struct sockaddr *)&caddr,&len)) < 0){
			perror("accept");
			exit(1);
		}
		if((pid=fork()) < 0){
			perror("fork");
			exit(1);
		}else if(pid == 0){
			close(fd_1);
			//retlen = read(fd_2,buf,BUF_SIZE);
			while(1){
				buf[0] = '\0';
				sendbuf[0] = '\0';
				retlen = read(fd_2,buf,BUF_SIZE);
				buf[retlen] = '\0';
				for(i=0;i<retlen;i++){
					if(isalpha(buf[i])){
						sendbuf[i] = toupper(buf[i]);
					}
				}
				sendbuf[i] = '\0';
				msglen = strlen(sendbuf);
				write(fd_2,sendbuf,msglen);
				DB(fprintf(stderr,":%s:",buf));
				DB(fprintf(stderr,"[%d]",strncmp(buf,"quit",4)));
				if(strncmp(buf,"quit",4) == 0){
					break;
					//goto EXITLOOP;
				}
				//retlen = read(fd_2,buf,BUF_SIZE);
			}
			exit(0);

		}
		close(fd_2);
	}
	//EXITLOOP:

	return(0);
}
