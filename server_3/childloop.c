//extern
extern char *VersionString();

//typedef
typedef struct {
	char *buf;
	int size;
	int len;
}DATA_BUF;

//init buf
DATA_BUF SendBuf={NULL,0,0};
DATA_BUF RecvBuf={NULL,0,0};

//mode change func
int SetBlock(int fd, int flag){
	int flags;
	if((flags=fcntl(fd,F_GETFL,0)) == -1){
		perror("fcntl");
		return(1);
	}
	if(flag == 0){
		fcntl(fd,F_SETFL,flags|O_NDELAY);
	}else if(flag == 1){
		fcntl(fd,F_SETFL,flags&~O_NDELAY);
	}
	return(0);
}

//print func
int DataPrint(FILE *fp, char *buf, int len){
	int i;
	for(i=0;i<len;i++){
		if(buf[i] == '['){
			fprintf(fp,"[%02x]",(unsigned int)(buf[i]&0xFF));
		}else if(buf[i] == ']'){
			fprintf(fp,"[%02x]",(unsigned int)(buf[i]&0xFF));
		}else if(isascii(buf[i])&&isprint(buf[i])){
			fputc(buf[i],fp);
		}else{
			fprintf(fp,"[%02x]",(unsigned int)(buf[i]&0xFF));
		}
	}
	return(0);
}

//recieve func
int RecvData(int acc){
	char buf[LONG_BUFF_SIZE];
	//char buf[8];
	int len;
	DB(fprintf(stderr,"RecvData()\n"));
	len=recv(acc,buf,sizeof(buf),0);
	DB(fprintf(stderr,"recv %d byte {",len));
	DB(DataPrint(stderr,buf,len));
	DB(fprintf(stderr,"}\n"));
	if(len == 0||strncmp(buf,"end;",4) == 0){
		return(1);
	}
	return(0);
}

//send func
int SendData(int acc){
	int len;
	DB(fprintf(stderr,"SendData()\n"));
	len=send(acc,SendBuf.buf,SendBuf.len,0);
	DB(fprintf(stderr,"send %d byte {",len));
	DB(DataPrint(stderr,SendBuf.buf,len));
	DB(fprintf(stderr,"}\n"));

	free(SendBuf.buf);
	SendBuf.buf=NULL;
	SendBuf.size=0;
	SendBuf.len=0;
	return(0);
}

//childloop
int ChildLoop(int acc, char *client_address, char *greeting){
	fd_set read_mask;
	fd_set write_mask;
	fd_set mask;
	int width;
	struct timeval timeout;
	int end_flag;
	//char buf[80];
	DB(fprintf(stderr,"ChildLoop\n"));

	//block mode
	SetBlock(acc,0);

	//mask
	width=0;
	FD_ZERO(&mask);
	DB(fprintf(stderr,"sizeof(fd_set):%ld:\n",sizeof(fd_set)));
	DB(fprintf(stderr,"size of fds_bits:%ld:\n",sizeof(mask.__fds_bits)));
	DB(fprintf(stderr,"fds_bits[0]:%ld:\n",mask.__fds_bits[0]));
	DB(fprintf(stderr,"acc before FD_SET:%d:\n",acc));
	FD_SET(acc,&mask);
	DB(fprintf(stderr,"acc after FD_SET:%d:\n",acc));
	width=acc+1; //acc: (maximam during read_mask and write_mask) + 1
	DB(fprintf(stderr,"width (acc+1):%d:\n",width));

	//init data for sending
	SendBuf.size=SHORT_BUFF_SIZE;
	if((SendBuf.buf=calloc(SendBuf.size,sizeof(char))) == NULL){perror("calloc"); exit(1);}
	sprintf(SendBuf.buf,"Ready %s",VersionString());
	SendBuf.len=strlen(SendBuf.buf);

	//send-secv loop
	end_flag=0;
	while(1){
		read_mask=mask;
		if(SendBuf.len>0){
			write_mask=mask;
		}else{
			FD_ZERO(&write_mask);
		}
		timeout.tv_sec=TIMEOUT_S;
		timeout.tv_usec=TIMEOUT_US;
		switch(select(width,(fd_set *)&read_mask,&write_mask,NULL,&timeout)){
			case -1:
			if(errno!=EINTR){
				perror("select");
			}
			break;

			case 0:
			break;

			default:
			if(FD_ISSET(acc,&read_mask)){
				end_flag=RecvData(acc);
			}
			if(FD_ISSET(acc,&write_mask)){
				SendData(acc);
			}
			break;
		}
		if(end_flag == 1){
			break;
		}
	}
	return(0);
}

