#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <unistd.h>
#include <pthread.h>
#define DB(x) x
//#include "/home/pub/include/clients.c"
#include "clients.c"
//#include "/home/pub/include/servers.c"
#include "servers.c"
#define HASHSIZE 200
#define LOWERPORT 10000
#define MAXSERVS 200
#define MAXSTATELEN 8192
#define MAXMRETLEN 8192
#define LOCALHOST "127.0.0.1"
#define DEFAULTHOST "150.26.237.19"

struct hash {
	char *type;
	char *name;
	char *var;
};

int is_alnum_char();
int to_maple_statement();
int statement_interpret();
int connect_server();

#include "client_func.c"

int main(int argc, char **argv){
	int i = 0;
	int escape = 0;
	signed char c;
	int hashsize = 0;
	char *statement;
	struct hash *user_vars;
	if((statement = calloc((size_t)sizeof(char),MAXSTATELEN)) == NULL){
		printf("failed : malloc(statement) -- exit.\n");
		exit(1);
	}
	if((user_vars = calloc((size_t)sizeof(struct hash),HASHSIZE)) == NULL){
		printf("failed : malloc(user_vars) -- exit.\n");
		exit(1);
	}
	/* (*main loop */
	printf(" << ");
	while(scanf("%c",&c) != EOF){
		if((escape == 1)&&(c == '\\')){
			statement[i] = c;
			i++;
		}else if((escape == 0)&&(c == '\\')){
			statement[i] = c;
			i++;
			escape = 1;
		}else if((escape == 1)&&(c == '\n')){
			i--;
			escape = 0;
		}else if((escape == 0)&&(c == '\n')){
			statement[i] = '\0';
			i = 0;
			printf("S::%s::\n",statement);
			statement_interpret(statement,&hashsize,user_vars);
			printf(" << ");
		}else if(c != '\n'){
			escape = 0;
			statement[i] = c;
			i++;
		}
	}
	/* *) */
	return(0);
}

