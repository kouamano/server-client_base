#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "/home/pub/include/alloc.c"
#include "/home/pub/include/debug.c"

typedef union {
	unsigned int i;
	char c[8];
}qq;


typedef union {
	int i;
	char c[4];
}q;

int main(int argc, char **argv){
	q qr;
	char c[5] = "ABCD";
	int i = "CBA";
	qr.c[0] = c[0];
	qr.c[1] = 'B';
	qr.c[2] = c[2];
	qr.c[3] = 'D';
	fprintf(stdout,"%s\n",c);
	fprintf(stdout,"%0x\n",c[0]);
	fprintf(stdout,"%0x\n",c[1]);
	fprintf(stdout,"%0x\n",c[2]);
	fprintf(stdout,"%0x\n",c[3]);
	//fprintf(stdout,"%0x\n",(int)c);
	fprintf(stdout,"%0x\n",i);
	fprintf(stdout,"%0x=%d\n",qr.i,qr.i);
	fprintf(stdout,"size q:%d:\n",sizeof(q));
	fprintf(stdout,"size qr:%d:\n",sizeof(qr));
	fprintf(stdout,"size qq:%d:\n",sizeof(qq));

	return(0);
}
