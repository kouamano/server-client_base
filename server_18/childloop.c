//childloop
int ChildLoop(int acc, char *client_address, char *greeting){
	fd_set read_mask;
	fd_set write_mask;
	fd_set mask;
	int width;
	struct timeval timeout;
	int flag_loop_end;
	int i;
	int comm_ptr = 0;
	int comm_esc = 0;
	int flag_comm_end = 0;
	int skip = 0;
	int is_exec = 0;
	int comm_send = 0;
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
	flag_loop_end=0;
	while(1){
		is_exec = 0;
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
				flag_loop_end=RecvData(acc,&RecvBuf);
			}
			//operation after recv
			DB(fprintf(stderr,"after RecvData() RecvBuf.buf:%s:\n",RecvBuf.buf));
			DB(fprintf(stderr,"after RecvData() RecvBuf.len:%d:\n",RecvBuf.len));
			if((CommBuf.size - CommBuf.len) > RecvBuf.size){
				for(i=0;i<RecvBuf.len;i++){
					if((RecvBuf.buf[i] == 0x04)&&(comm_esc == 0)){
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
						CommBuf.len = comm_ptr + i;
						comm_esc = 0;
					}
					//DB(fprintf(stderr,"commesc:%d:%c:",comm_esc,RecvBuf.buf[i]));
				}
			}else{
				flag_comm_end = 2;
			}
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
	
				//exec comm and write to RsltBuf
				is_exec = ExecComm(&flag_loop_end,&comm_send);

				//create SendData
				if((SendBuf.buf = malloc(sizeof(char) * (CommBuf.len + 40 + RsltBuf.len))) == NULL){perror("malloc()"); exit(1);}
				if(comm_send > 0){
					sprintf(SendBuf.buf,"%s","comm:");
					sprintf(SendBuf.buf+5,"%s",CommBuf.buf);
					sprintf(SendBuf.buf+5+CommBuf.len,"%s",":\n");
					SendBuf.len = strlen(SendBuf.buf);
				}else{
					SendBuf.len = 0;
				}
				if(is_exec > 0){
					sprintf(SendBuf.buf+SendBuf.len,"%s",RsltBuf.buf);
					SendBuf.len = strlen(SendBuf.buf);
					if(RsltBuf.buf != NULL){
						free(RsltBuf.buf);
						RsltBuf.buf = NULL;
						RsltBuf.size = 0;
						RsltBuf.len = 0;
					}
				}

			}else if(flag_comm_end == 2){
				if((SendBuf.buf = malloc(sizeof(char) * (40))) == NULL){perror("malloc()"); exit(1);}
				sprintf(SendBuf.buf,"%s","error: commsize over.\n");
				SendBuf.len = strlen(SendBuf.buf);
				
			}

			//cleaning
			if(flag_comm_end == 1){
				if((CommBuf.len > 0) && (strncmp(CommBuf.buf,"end;",4) == 0)){
					flag_loop_end = 1;
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
				SendData(acc,&SendBuf);
			}
			break;
		}
		if(flag_loop_end == 1){
			DB(fprintf(stderr,"session break.\n"));
			break;
		}
	}
	return(0);
}

