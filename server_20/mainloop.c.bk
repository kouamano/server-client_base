/*allocation test*/
int MainLoop(){
	int acc;
	struct sockaddr_in from;
	unsigned int len=0;
	char *client_address;
	int client_port=0;
	int pid;
	int i;
	int *test_int;
	int test_size = 1000;
	//int test_size = (1000*1000*1000);
	char *greeting;

	/*alocation*/
	if((client_address = calloc(sizeof(char),  BUFF_SIZE)) == NULL){perror("malloc\n");}
	if((greeting = calloc(sizeof(char) , BUFF_SIZE)) == NULL){perror("malloc\n");}

	DB(fprintf(stderr,"MainLoop()\n"));

	/*test data*/
	if((test_int = malloc(sizeof(int) * test_size)) == NULL){perror("malloc\n");}
	for(i=0;i<test_size;i++){
		test_int[i] = i;
	}
	sprintf(greeting,"Ready[%d]\r\n",test_int[222]);

	/*connection loop*/
	while(1){
		len=sizeof(from);
		/*accept*/
		acc=accept(Soc,(struct sockaddr *)&from,&len);
		DB(fprintf(stderr,"acc:%d:\n",acc));
		if(acc < 0){
			if(errno != EINTR){
				perror("accept\n");
			}
			continue;
		}
		if(BUFF_SIZE < strlen(inet_ntoa(from.sin_addr))){
			fprintf(stderr,"BUFF_SIZE: too short.\n");
			exit(1);
		}
		sprintf(client_address,"%s",inet_ntoa(from.sin_addr));
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
