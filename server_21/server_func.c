char *VersionString(){
        return(VERSION);
}

/*
int InitParam(){
	DB(fprintf(stderr,"InitParm()\n"));
	Soc=0;
	return(0);
}
*/

int InitSocket(char *_servname){
	struct servent *se;
	struct sockaddr_in ss;
	int opt;

	DB(fprintf(stderr,"InitSocket()\n"));
	
	/*get service*/
	if((se=getservbyname(_servname,"tcp")) == NULL){
		perror("getservbyname\n");
		return(-1);
	}
	DB(fprintf(stderr,"getservbyname():%d\n",ntohs(se->s_port)));

	/*create socket*/
	if((Soc=socket(AF_INET,SOCK_STREAM,0)) < 0){
		perror("socket\n");
		return(-1);
	}
	DB(fprintf(stderr,"Soc:%d:\n",Soc));

	/*reset socket*/
	opt=1;
	if(setsockopt(Soc,SOL_SOCKET,SO_REUSEADDR,(char *)&opt,sizeof(int)) != 0){
		perror("setsockopt\n");
		return(-1);
	}
	DB(fprintf(stderr,"setsockopt()\n"));

	/*bind*/
	memset((char *)&ss,0,sizeof(struct sockaddr_in));
	ss.sin_family=AF_INET;
	ss.sin_port=se->s_port;
	if(bind(Soc,(struct sockaddr *)&ss,sizeof(ss)) == -1){
		perror("bind\n");
		return(-1);
	}
	DB(fprintf(stderr,"bind()\n"));

	return(0);
}

int CloseSocket(){
	DB(fprintf(stderr,"CloseSocket()\n"));
	if(Soc>0){
		close(Soc);
	}
	Soc=0;
	return(0);
}

void EndSignal(int sig){
	DB(fprintf(stderr,"EndSignal()\n"));
	CloseSocket();
	exit(0);
}

void EndChild(int sig){
	int pid;
	pid=wait(0);
	DB(fprintf(stderr,"EndChild()\n"));
	signal(SIGCLD,EndChild);
}

int InitSignal(){
	DB(fprintf(stderr,"InitSignal()\n"));
	//end main loop
	signal(SIGINT,EndSignal);
	signal(SIGQUIT,EndSignal);
	signal(SIGTERM,EndSignal);
	//end child loop
	signal(SIGCLD,EndChild);
	return(0);
}

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

//buff operation
int InitBuf(DATA_BUF *Buf, size_t size){
	int i;
	Buf->len = 0;
	Buf->size = size;
	Buf->buf = c_alloc_vec(Buf->size);
	for(i=0;i<Buf->size;i++){
		Buf->buf[i] = '\0';
	}
	return(Buf->size);
}

int ClearBuf(DATA_BUF *Buf){
	int i;
	for(i=0;i<Buf->size;i++){
		Buf->buf[i] = '\0';
	}
	Buf->len = 0;
	return(i);
}

int FreeBuf(DATA_BUF *Buf){
	if(Buf->buf != NULL){
		free(Buf->buf);
		Buf->buf = NULL;
		Buf->size = 0;
		Buf->len = 0;
		return(1);
	}
	return(0);
}

//recieve func
int RecvData(int acc, DATA_BUF *Buf){
        DB(fprintf(stderr,"RecvData()\n"));
        int len;
        int i;
        for(i=0;i<Buf->size;i++){
                Buf->buf[i] = '\0';
        }
        len=recv(acc,Buf->buf,(sizeof(char) * Buf->size),0);
        Buf->len = len;
        DB(fprintf(stderr,"recv %d byte {",len));
        DB(DataPrint(stderr,Buf->buf,len));
        DB(fprintf(stderr,"}\n"));
	if(len==0){
		return(1);
	}
        return(0);
}

//send func
int SendData(int acc, DATA_BUF *Buf){
        int len=-2;
        int buf_start=0;
        DB(fprintf(stderr,"SendData()\n"));
        while(Buf->len > 0){
                len=send(acc,Buf->buf+buf_start,Buf->len,0);
                buf_start = buf_start + len;
                Buf->len = Buf->len - len;
        }
        DB(fprintf(stderr,"send %d byte {",len));
        DB(DataPrint(stderr,Buf->buf,len));
        DB(fprintf(stderr,"}\n"));

        free(Buf->buf);
        Buf->buf=NULL;
        Buf->size=0;
        Buf->len=0;
        return(0);
}

//exec comm
int ExecComm(int *_end_flag, int *_comm_send){
        if(CommBuf.len > 0){
		DB(fprintf(stderr,"COMMBUFF:%s:\n",CommBuf.buf));
                if(strcmp(CommBuf.buf,"end;") == 0){
			*_end_flag = 1;
                        return(0);
		}else if(strcmp(CommBuf.buf,"Alive?;") == 0){
			RsltBuf.size = 6;
			RsltBuf.buf = malloc(sizeof(char) * RsltBuf.size);
			sprintf(RsltBuf.buf,"Alive");
                        RsltBuf.len = strlen(RsltBuf.buf);
			*_comm_send = 0;
			*_end_flag = 1;
			return(1);
                }else{
			//test
                        RsltBuf.size = 100;
                        if((RsltBuf.buf = malloc(sizeof(char) * RsltBuf.size)) == NULL){perror("malloc"); exit(1);}
                        sprintf(RsltBuf.buf,"hoge");
                        RsltBuf.len = strlen(RsltBuf.buf);
			*_comm_send = 1;
                        return(1);
                }
        }
        return(0);
}


