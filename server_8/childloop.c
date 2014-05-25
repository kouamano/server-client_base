//extern
//extern char *VersionString();

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
	//char buf[LONG_BUFF_SIZE];
	//char buf[8];
	DB(fprintf(stderr,"RecvData()\n"));
	int len;
	int i;
	for(i=0;i<RecvBuf.size;i++){
		RecvBuf.buf[i] = '\0';
	}
	//len=recv(acc,buf,sizeof(buf),0);
	len=recv(acc,RecvBuf.buf,(sizeof(char) * RecvBuf.size),0);
	RecvBuf.len = len;
	DB(fprintf(stderr,"recv %d byte {",len));
	DB(DataPrint(stderr,RecvBuf.buf,len));
	DB(fprintf(stderr,"}\n"));
	/*
	if(len == 0||strncmp(RecvBuf.buf,"end;",4) == 0){
		return(1);
	}
	*/
	return(0);
}

//send func
int SendData(int acc){
	int len=-2;
	int buf_start=0;
	DB(fprintf(stderr,"SendData()\n"));
	while(SendBuf.len > 0){
		len=send(acc,SendBuf.buf+buf_start,SendBuf.len,0);
		buf_start = buf_start + len;
		SendBuf.len = SendBuf.len - len;
	}
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
	int i;
	int comm_ptr = 0;
	int comm_esc = 0;
	int flag_comm_end = 0;
	int skip = 0;
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
			//recv
			if(FD_ISSET(acc,&read_mask)){
				//end_flag=RecvData(acc);
				RecvData(acc);
			}
			//operation after recv
			DB(fprintf(stderr,"after RecvData() RecvBuf.buf:%s:\n",RecvBuf.buf));
			DB(fprintf(stderr,"after RecvData() RecvBuf.len:%d:\n",RecvBuf.len));
			if((CommBuf.size - CommBuf.len) > RecvBuf.size){
				for(i=0;i<RecvBuf.len;i++){
					if((RecvBuf.buf[i] == ';')&&(comm_esc == 0)){
						CommBuf.buf[comm_ptr + i] = '\0';
						CommBuf.len = comm_ptr + i;
						flag_comm_end = 1;
						comm_esc = 0;
					}else if((RecvBuf.buf[i] == '\\')&&(comm_esc == 0)){
						comm_ptr = comm_ptr - 1;
						comm_esc = 1;
					}else if((RecvBuf.buf[i] == '\\')&&(comm_esc > 0)){
						CommBuf.buf[comm_ptr + i] = '\\';
						CommBuf.len = comm_ptr + i;
						comm_esc = 0;
					}else{
						CommBuf.buf[comm_ptr + i] = RecvBuf.buf[i];
						comm_esc = 0;
					}
					/*
					if(RecvBuf.buf[i] == '\\'){
						comm_esc++;
					}
					*/
					DB(fprintf(stderr,"commesc:%d:%c:",comm_esc,RecvBuf.buf[i]));
				}
				//DB(fprintf(stderr,"COMMSIZE:%d:\n",CommBuf.len));
				//DB(fprintf(stderr,"COMM:"));
				//DB(fprintf(stderr,"%s",CommBuf.buf));
				//DB(fprintf(stderr,":\n"));
			}else{
				flag_comm_end = 2;
			}
			/*
			if(strncmp(CommBuf.buf,"end",3) == 0){
				end_flag = 1;
			}
			*/
			//command operation 
			if(flag_comm_end == 1){
				//preprocess
				skip=0;
				comm_esc = 0;
				for(i=0;i<CommBuf.len;i++){
					if((CommBuf.buf[i] >= 33)&&(CommBuf.buf[i] <= 126)){
						break;
					}else{
						skip++;
					}
				}
				for(i=skip; i<CommBuf.len; i++){
					CommBuf.buf[i - skip] = CommBuf.buf[i];
				}
				CommBuf.buf[i - skip] = '\0';
				CommBuf.len = CommBuf.len - skip;
				DB(fprintf(stderr,"preprocessed COMMSIZE:%d:\n",CommBuf.len));
				DB(fprintf(stderr,"preprocessed COMM:"));
				DB(fprintf(stderr,"%s",CommBuf.buf));
				DB(fprintf(stderr,":\n"));
				//SendData
				if((SendBuf.buf = malloc(sizeof(char) * (CommBuf.len + 40))) == NULL){perror("malloc()"); exit(1);}
				sprintf(SendBuf.buf,"%s","comm:");
				sprintf(SendBuf.buf+5,"%s",CommBuf.buf);
				sprintf(SendBuf.buf+5+CommBuf.len,"%s",":");
				SendBuf.len = strlen(SendBuf.buf);

			}else if(flag_comm_end == 2){
				if((SendBuf.buf = malloc(sizeof(char) * (40))) == NULL){perror("malloc()"); exit(1);}
				sprintf(SendBuf.buf,"%s","error: commsize over.\n");
				SendBuf.len = strlen(SendBuf.buf);
				
			}

			//cleaning
			if(flag_comm_end == 1){
				if((CommBuf.len > 0) && (strncmp(CommBuf.buf,"end",CommBuf.len) == 0)){
					end_flag = 1;
				}
				for(i=0;i<CommBuf.len;i++){
					CommBuf.buf[i] = '\0';
				}
				for(i=0;i<RecvBuf.len;i++){
					RecvBuf.buf[i] = '\0';
				}
				comm_ptr = 0;
				flag_comm_end = 0;
			}else{
				comm_ptr = comm_ptr + i;
			}
			//send
			if(FD_ISSET(acc,&write_mask)){
				SendData(acc);
			}
			break;
		}
		if(end_flag == 1){
			DB(fprintf(stderr,"session break.\n"));
			break;
		}
	}
	return(0);
}

