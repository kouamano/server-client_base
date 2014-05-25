char *VersionString(){
        return(VERSION);
}

int PrintVersion(){
	DB(fprintf(stderr,"PrintVersion()\n"));
	printf("%s\n",VersionString());
	return(0);
}

int InitParam(){
	DB(fprintf(stderr,"InitParm()\n"));
	Soc=0;
	return(0);
}

int InitSocket(){
	struct servent *se;
	struct sockaddr_in me;
	int opt;

	DB(fprintf(stderr,"InitSocket()\n"));
	
	/*get service*/
	if((se=getservbyname("TestServer","tcp")) == NULL){
		perror("getservbyname\n");
		return(-1);
	}
	DB(fprintf(stderr,"getservbyname():%d\n",ntohs(se->s_port)));

	/*create socket*/
	if((Soc=socket(AF_INET,SOCK_STREAM,0)) < 0){
		perror("socket\n");
		return(-1);
	}
	DB(fprintf(stderr,"socket():%d\n",Soc));

	/*reset socket*/
	opt=1;
	if(setsockopt(Soc,SOL_SOCKET,SO_REUSEADDR,(char *)&opt,sizeof(int)) != 0){
		perror("setsockopt\n");
		return(-1);
	}
	DB(fprintf(stderr,"setsockopt():OK\n"));

	/*bind*/
	//memset((char *)&me,0,sizeof(me));
	memset((char *)&me,0,sizeof(struct sockaddr_in));
	me.sin_family=AF_INET;
	me.sin_port=se->s_port;
	if(bind(Soc,(struct sockaddr *)&me,sizeof(me)) == -1){
		perror("bind\n");
		return(-1);
	}
	DB(fprintf(stderr,"bind():OK\n"));

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

int InitSignal(){
	DB(fprintf(stderr,"InitSignal()\n"));
	/*set signal*/
	signal(SIGINT,EndSignal);
	signal(SIGQUIT,EndSignal);
	signal(SIGTERM,EndSignal);
	return(0);
}

int ChildLoop(int acc, char *client_address, char *greeting){
	char *buf_rec;
	char *buf_cp;
	int len;
	int glen;

	/*allocation*/
	if((buf_rec=malloc(sizeof(char) * BUFF_SIZE)) == NULL){perror("malloc\n");}
	if((buf_cp=malloc(sizeof(char) * BUFF_SIZE)) == NULL){perror("malloc\n");}

	glen=strlen(greeting);
	send(acc,greeting,glen,0);
	//send(acc,"Ready\r\n",7,0);
	while(1){
		/*recieve*/
		DB(fprintf(stderr,"buf size:%ld:\n",sizeof(buf_rec) * BUFF_SIZE));
		len=recv(acc,buf_rec,sizeof(char) * BUFF_SIZE,0);
		buf_rec[len]='\0';
		if(len>0){
			if(strncmp(buf_rec,"end",3) == 0){
				break;
			}
			sprintf(buf_cp,"%s:[%s]\n",client_address,buf_rec);

			/*send*/
			send(acc,buf_cp,strlen(buf_cp),0); /*send for client*/
			DB(fprintf(stderr,"PID=%d:%s",getpid(),buf_cp)); /*output for server*/
		}
	}
	return(0);
}

/*allocation test*/
int MainLoop(){
	int acc;
	struct sockaddr_in from;
	unsigned int len=0;
	char *client_address;
	int client_port=0;
	int pid;
	char *cname;
	int i;
	int *test_int;
	int test_size = (1000*1000*1000);
	char *greeting;

	/*alocation*/
	if((client_address = calloc(sizeof(char),  BUFF_SIZE)) == NULL){perror("malloc\n");};
	if((cname = calloc(sizeof(char) , BUFF_SIZE)) == NULL){perror("malloc\n");};
	if((greeting = calloc(sizeof(char) , BUFF_SIZE)) == NULL){perror("malloc\n");};

	DB(fprintf(stderr,"MainLoop()\n"));

	/*test data*/
	if((test_int = malloc(sizeof(int) * test_size)) == NULL){perror("malloc\n");}
	for(i=0;i<test_size;i++){
		test_int[i] = i;
	}
	sprintf(greeting,"Ready[%d]\r\n",test_int[222]);

	/*listen*/
	listen(Soc,SOMAXCONN);

	/*connection loop*/
	while(1){
		len=sizeof(from);
		/*accept*/
		acc=accept(Soc,(struct sockaddr *)&from,&len);
		if(acc < 0){
			if(errno != EINTR){
				perror("accept\n");
			}
			continue;
		}
		sprintf(cname,"%s",inet_ntoa(from.sin_addr));
		strcpy(client_address,cname);
		//strcpy(client_address,inet_ntoa(from.sin_addr)); /*warning*/
		client_port=ntohs(from.sin_port);
		DB(fprintf(stderr,"%s:%d\n",client_address,client_port));
		if((pid=fork()) == 0){
			close(Soc);
			Soc=0;
			/*operation for client*/
			ChildLoop(acc,client_address,greeting);
			exit(0);
		}
		/*close socket*/
		close(acc);
		acc=0;
		//sleep(1);
	}
	return(0);
}
