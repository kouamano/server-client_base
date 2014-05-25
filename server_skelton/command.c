#include    <stdio.h>
#include    <string.h>
#include    <ctype.h>
#include    <stdlib.h>
#include    <unistd.h>
#include    <sys/socket.h>
#include    <netinet/in.h>
#include    <netinet/tcp.h>
#include    <netdb.h>
#include    <signal.h>
#include    <errno.h>

#define    DB(x)    x

extern int IntoSendDataBuf(char *add_buf,int add_len);
extern int DataPrint(FILE *fp,char *buf,int len);

#define TOKEN_SEPARATE_COMMAND  " \t\r\n"
#define TOKEN_SEPARATE_POINT_COMMAND    ",()="

#define    TOKEN_ALLOC_SIZE    (128)

typedef    struct    {
    char    **token;
    int    size;
    int    no;
}TOKEN;

#ifdef    SJIS
#define issjiskanji(c)  ((0x81 <= (unsigned char)(c&0xff) &&  \
                         (unsigned char)(c&0xff) <= 0x9f)     \
    || (0xe0 <= (unsigned char)(c&0xff) && (unsigned char)(c&0xff) <= 0xfc))
#else
#define    issjiskanji(c)    0
#endif

int AddToken(TOKEN *token,char *buf,int len)
{
int    pack_flag;

    pack_flag=0;
    if(buf[0]=='"'&&buf[len-1]=='"'){
        pack_flag=1;
    }
    else if(buf[0]=='\''&&buf[len-1]=='\''){
        pack_flag=1;
    }

    if(token->size==0){
        token->size=TOKEN_ALLOC_SIZE;
        token->token=(char **)malloc(token->size*sizeof(char *));
        token->token[token->no]=malloc(len+1);
        if(pack_flag==0){
            memcpy(token->token[token->no],buf,len);
            token->token[token->no][len]='\0';
        }
        else{
            memcpy(token->token[token->no],buf+1,len-2);
            token->token[token->no][len-2]='\0';
        }
        token->no++;
    }
    else{
        if(token->no+1>=token->size){
            token->size+=TOKEN_ALLOC_SIZE;
            token->token=(char **)realloc((char *)token->token,
                                          token->size*sizeof(char *));
        }
        token->token[token->no]=malloc(len+1);
        if(pack_flag==0){
            memcpy(token->token[token->no],buf,len);
            token->token[token->no][len]='\0';
        }
        else{
            memcpy(token->token[token->no],buf+1,len-2);
            token->token[token->no][len-2]='\0';
        }
        token->no++;
    }

    return(0);
}

int GetToken(char *buf,int len,TOKEN *token,
        char *token_separate,char *token_separate_point)
{
int    token_start,token_len;
int    i,j;
char   point[10];

    token->token=NULL;
    token->size=0;
    token->no=0;

    token_start=0;
    for(i=0;i<len;i++){
        /* SJIS����1�Х����ܡ����������� */
        if(issjiskanji(buf[i])||buf[i]=='\\'){
            i++;
        }
        else if(buf[i]=='"'){    /* "ʸ���� */
            for(j=i+1;j<len;j++){
                /* SJIS����1�Х����ܡ����������� */
                if(issjiskanji(buf[j])||buf[j]=='\\'){
                    j++;
                    continue;
                }
                if(buf[j]=='"'){
                    break;
                }
            }
            i=j;
        }
        else if(buf[i]=='\''){    /* 'ʸ���� */
            for(j=i+1;j<len;j++){
                /* SJIS����1�Х����ܡ����������� */
                if(issjiskanji(buf[j])||buf[j]=='\\'){
                    j++;
                    continue;
                }
                if(buf[j]=='\''){
                    break;
                }
            }
            i=j;
        }
        else if(strchr(token_separate,buf[i])!=NULL){    /* ʬΥʸ�� */
            token_len=i-token_start;
            if(token_len>0){
                AddToken(token,&buf[token_start],token_len);
            }
            token_start=i+1;
        }
        else if(strchr(token_separate_point,buf[i])!=NULL){    /* ���ڤ�ʬΥʸ�� */
            token_len=i-token_start;
            if(token_len>0){
                AddToken(token,&buf[token_start],token_len);
            }
            sprintf(point,"%c",buf[i]);
            AddToken(token,point,1);
            token_start=i+1;
        }
    }
    token_len=i-token_start;
    if(token_len>0){
        AddToken(token,&buf[token_start],token_len);
    }

    return(0);
}

int FreeToken(TOKEN *token)
{
int    i;

    for(i=0;i<token->no;i++){
        free(token->token[i]);
    }
    if(token->size>0){
        free(token->token);
    }
    token->token=NULL;
    token->size=0;
    token->no=0;

    return(0);
}

int DebugToken(FILE *fp,TOKEN *token)
{
int    i;
char   buf[512];

    for(i=0;i<token->no;i++){
        sprintf(buf,"[%s]",token->token[i]);
        fprintf(fp,"%s",buf);
        IntoSendDataBuf(buf,strlen(buf));
    }
    fprintf(fp,"\r\n");
    IntoSendDataBuf("\r\n",2);

    return(0);
}

#define COLUMN_ALLOC_SIZE       (128)

typedef struct  {
        char    *column;
        char    *value;
}COLUMN_DATA;
typedef struct  {
        COLUMN_DATA     *column;
        int     column_size;
        int     column_no;
}TABLE_PARAM;

int ReadTableParam(char *table,TABLE_PARAM *data)
{
FILE    *fp;
char    filename[512];
char    buf[512];
char    *ptr;

    data->column=NULL;
    data->column_size=0;
    data->column_no=0;

    sprintf(filename,"%s.prm",table);
    if((fp=fopen(filename,"r"))==NULL){
        fprintf(stderr,"%s cannot read.\n",filename);
        return(-1);
    }

    data->column_size=COLUMN_ALLOC_SIZE;
    data->column=(COLUMN_DATA *)calloc(data->column_size,sizeof(COLUMN_DATA));

    while(1){
        fgets(buf,sizeof(buf),fp);
        if(feof(fp)){
            break;
        }
        if(buf[0]=='#'){        /* �����ȹ� */
            continue;
        }
        if((ptr=strchr(buf,'\n'))!=NULL){       /* ����ʸ������ */
            *ptr='\0';
        }
        if(strlen(buf)==0){    /* Ĺ������ */
            continue;
        }
        data->column_no++;
        if(data->column_no>=data->column_size){
            data->column_size+=COLUMN_ALLOC_SIZE;
            data->column=(COLUMN_DATA *)realloc(data->column,
                              data->column_size*sizeof(COLUMN_DATA));
        }
        data->column[data->column_no-1].column=strdup(buf);
        data->column[data->column_no-1].value=NULL;
    }

    fclose(fp);

    return(0);
}

int FreeTableParam(TABLE_PARAM *data)
{
int     i;

    for(i=0;i<data->column_no;i++){
        if(data->column[i].column!=NULL){
            free(data->column[i].column);
        }
        if(data->column[i].value!=NULL){
            free(data->column[i].value);
        }
    }
    if(data->column_size>0&&data->column!=NULL){
        free(data->column);
    }

    return(0);
}

/* �ץ������å����� */
#define LOCK_NONE       0
#define LOCK_NORMAL     1
#define LOCK_INTERNAL   2
#define LOCK_COMMAND    3

extern int LockTable(char *table_name,int flag);
extern int UnlockTable(char *table_name,int flag);

FILE *OpenTable(char *table,char *mode)
{
char    filename[512];
FILE    *fp;

    LockTable(table,LOCK_NORMAL);

    sprintf(filename,"%s.dat",table);
    fp=fopen(filename,mode);

    return(fp);
}

int CloseTable(char *table,FILE *fp)
{

    fclose(fp);

    UnlockTable(table,LOCK_NORMAL);

    return(0);
}

typedef struct  {
    char    *column;
    char    *value;
}COLUMN_VALUE;

typedef struct    {
    char    *table;
    COLUMN_VALUE    *c_v;
    int    c_v_size;
    int    c_v_no;
}INSERT_DATA;

int DoInsert(INSERT_DATA *data)
{
int    i,j;
int    find;
TABLE_PARAM    table_param;
char    buf[512];
FILE    *fp;

    DB(fprintf(stderr,"DoInsert()\n"));

    /* �ǥХå�ɽ�� */
    DB(fprintf(stderr,"INSERT:table=%s\n",data->table));
    DB(
    for(i=0;i<data->c_v_no;i++){
        fprintf(stderr,"INSERT:%s:%s\n",data->c_v[i].column,data->c_v[i].value);
    }
    );

    /* �ѥ�᡼���ե������ɤ߹��� */
    if(ReadTableParam(data->table,&table_param)==-1){
        sprintf(buf,"Error:%s table param not available.\r\n",data->table);
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }

    /* ����̾�Υ����å� */
    for(i=0;i<data->c_v_no;i++){
        find=0;
        for(j=0;j<table_param.column_no;j++){
            if(strcmp(data->c_v[i].column,table_param.column[j].column)==0){
                table_param.column[j].value=strdup(data->c_v[i].value);
                find=1;
                break;
            }
        }
        if(find==0){    /* ���ܤ�¸�ߤ��ʤ� */
            fprintf(stderr,"%s:not in table column.\n",data->c_v[i].column);
        }
    }

    /* �ǡ����ե�����˳�Ǽ */
    if((fp=OpenTable(data->table,"a"))==NULL){
        sprintf(buf,"Error:%s table cannot append.\r\n",data->table);
        IntoSendDataBuf(buf,strlen(buf));
        FreeTableParam(&table_param);
        return(-1);
    }
    for(i=0;i<table_param.column_no;i++){
        if(i!=0){
            fprintf(fp,",");
        }
        if(table_param.column[i].value!=NULL){
            fprintf(fp,"\"%s\"",table_param.column[i].value);
        }
        else{
            fprintf(fp,"\"\"");
        }
    }
    fprintf(fp,"\n");
    CloseTable(data->table,fp);

    /* ���� */
    FreeTableParam(&table_param);

    /* ���� */
    sprintf(buf,"1 record inserted.\r\n");
    IntoSendDataBuf(buf,strlen(buf));

    return(0);
}

int ComInsert(TOKEN *token)
{
INSERT_DATA    data={NULL,NULL,0,0};
int    i,token_ptr,end_flag,c_v_no;
char   buf[512];

    DB(fprintf(stderr,"ComInsert()\n"));

    /* �ǥХå��� */
    DB(DebugToken(stderr,token));

    token_ptr=1;

    /* into������ɥ����å� */
    if(token_ptr<token->no&&strcmp(token->token[token_ptr],"into")==0){
        /* into������� OK */
    }
    else{
        sprintf(buf,"Error:insert into\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    token_ptr++;

    /* �ơ��֥�̾���� */
    if(token_ptr<token->no){
        data.table=token->token[token_ptr];
    }
    else{
        sprintf(buf,"Error:insert into table\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    token_ptr++;

    /* ��(�פ����)�פޤǹ���̾���� */
    data.c_v_size=COLUMN_ALLOC_SIZE;
    data.c_v=(COLUMN_VALUE *)calloc(data.c_v_size,sizeof(COLUMN_VALUE));
    end_flag=0;
    for(i=token_ptr;i<token->no;i++){
        if(i==token_ptr){
            if(strcmp(token->token[i],"(")==0){
                /* ��(��OK */
            }
            else{
                sprintf(buf,"Error:insert into table (\r\n");
                IntoSendDataBuf(buf,strlen(buf));
                free(data.c_v);
                return(-1);
            }
        }
        else if(strcmp(token->token[i],")")==0){
            end_flag=1;
            token_ptr=i+1;
            break;
        }
        else{
            data.c_v_no++;
            if(data.c_v_no>=data.c_v_size){
                data.c_v_size+=COLUMN_ALLOC_SIZE;
                data.c_v=(COLUMN_VALUE *)realloc(data.c_v,
                                 data.c_v_size*sizeof(COLUMN_VALUE));
            }
            data.c_v[data.c_v_no-1].column=token->token[i];
            if(i+1<token->no){
                if(strcmp(token->token[i+1],",")==0){
                    i++;
                }
                else if(strcmp(token->token[i+1],")")==0){
                }
                else{
                    sprintf(buf,"Error:insert into table (column ,\r\n");
                    IntoSendDataBuf(buf,strlen(buf));
                    free(data.c_v);
                    return(-1);
                }
            }
        }
    }
    if(end_flag==0){    /* ��λ�γ�̤��ʤ� */
        sprintf(buf,"Error:insert into table (...)\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        free(data.c_v);
        return(-1);
    }
    else if(data.c_v_no==0){    /* ���ܤλ��꤬���Ĥ�ʤ� */
        sprintf(buf,"Error:insert into table (no column data!)\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        free(data.c_v);
        return(-1);
    }

    /* values������ɥ����å� */
    if(token_ptr<token->no&&strcmp(token->token[token_ptr],"values")==0){
        /* values������� OK */
    }
    else{
        sprintf(buf,"Error:insert into table (...) values\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        free(data.c_v);
        return(-1);
    }
    token_ptr++;

    /* ��(�פ����)�פޤǹ���̾���� */
    end_flag=0;
    c_v_no=0;
    for(i=token_ptr;i<token->no;i++){
        if(i==token_ptr){
            if(strcmp(token->token[i],"(")==0){
                /* ��(��OK */
            }
            else{
                sprintf(buf,"Error:insert into table (...) values (\r\n");
                IntoSendDataBuf(buf,strlen(buf));
                free(data.c_v);
                return(-1);
            }
        }
        else if(strcmp(token->token[i],")")==0){
            end_flag=1;
            token_ptr=i+1;
            break;
        }
        else{
            c_v_no++;
            if(c_v_no>data.c_v_no){
                sprintf(buf,
                     "Error:insert into table (...) values (value no error!)\r\n");
                IntoSendDataBuf(buf,strlen(buf));
                free(data.c_v);
                return(-1);
            }
            data.c_v[c_v_no-1].value=token->token[i];
            if(i+1<token->no){
                if(strcmp(token->token[i+1],",")==0){
                    i++;
                }
                else if(strcmp(token->token[i+1],")")==0){
                }
                else{
                    sprintf(buf,
                       "Error:insert into table (...) values (value ,\r\n");
                    IntoSendDataBuf(buf,strlen(buf));
                    free(data.c_v);
                    return(-1);
                }
            }
        }
    }
    if(end_flag==0){    /* ��λ�γ�̤��ʤ� */
        sprintf(buf,"Error:insert into table (...) values (...)\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        free(data.c_v);
        return(-1);
    }
    else if(data.c_v_no!=c_v_no){    /* ���ܤȿ������פ��ʤ� */
        sprintf(buf,"Error:insert into table (...) values (value no error!)\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        free(data.c_v);
        return(-1);
    }

    /* ��Ͽ�����¹� */
    DoInsert(&data);

    free(data.c_v);

    return(0);
}

typedef    struct  {
    char    *table;
    char    *s_column;
    char    *column;
    char    *cmp;
    char    *value;
}SELECT_DATA;

#define TOKEN_SEPARATE_DATA     ",\r\n"
#define TOKEN_SEPARATE_POINT_DATA       ""

int ReadTableData(TABLE_PARAM *table_param,FILE *fp)
{
int    i;
char   buf[512];
TOKEN    token;

    if(fgets(buf,sizeof(buf),fp)==NULL){
        return(-1);
    }

    GetToken(buf,strlen(buf),&token,TOKEN_SEPARATE_DATA,TOKEN_SEPARATE_POINT_DATA);

    for(i=0;i<table_param->column_no;i++){
        if(i>=token.no){
            table_param->column[i].value=strdup("Data Error");
        }
        else{
            table_param->column[i].value=strdup(token.token[i]);
        }
    }

    FreeToken(&token);

    return(0);
}

int PrintTableParam(char *buf,TABLE_PARAM *table_param)
{
int    i;

    strcpy(buf,"");
    for(i=0;i<table_param->column_no;i++){
        if(i!=0){
            strcat(buf,",");
        }
        strcat(buf,"\"");
        strcat(buf,table_param->column[i].column);
        strcat(buf,"\"");
    }

    return(0);
}

int PrintTableData(char *buf,TABLE_PARAM *table_param)
{
int    i;

    strcpy(buf,"");
    for(i=0;i<table_param->column_no;i++){
        if(i!=0){
            strcat(buf,",");
        }
        strcat(buf,"\"");
        strcat(buf,table_param->column[i].value);
        strcat(buf,"\"");
    }

    return(0);
}

int DoSelect(SELECT_DATA *data)
{
int    i;
int    target_column,select_no;
TABLE_PARAM    table_param;
char    buf[512];
FILE    *fp;

    DB(fprintf(stderr,"DoSelect()\n"));

    /* �ǥХå�ɽ�� */
    DB(fprintf(stderr,"SELECT:table=%s\n",data->table));
    DB(fprintf(stderr,"SELECT:column=%s\n",data->column==NULL?"":data->column));
    DB(fprintf(stderr,"SELECT:cmp=%s\n",data->column==NULL?"":data->cmp));
    DB(fprintf(stderr,"SELECT:value=%s\n",data->column==NULL?"":data->value));

    /* �ѥ�᡼���ե������ɤ߹��� */
    if(ReadTableParam(data->table,&table_param)==-1){
        sprintf(buf,"Error:%s table param not available.\r\n",data->table);
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }

    if(data->column!=NULL){
        /* ����̾�Υ����å� */
        target_column=-1;
        for(i=0;i<table_param.column_no;i++){
            if(strcmp(data->column,table_param.column[i].column)==0){
                target_column=i;
                break;
            }
        }
        if(target_column==-1){    /* ���ܤ�¸�ߤ��ʤ� */
            sprintf(buf,"Error:%s not in table.\r\n",data->column);
            IntoSendDataBuf(buf,strlen(buf));
            FreeTableParam(&table_param);
            return(-1);
        }

        /* ɽ�����ܤΥ����å�:��*�װʳ��Ϥ��� */
        if(strcmp(data->s_column,"*")!=0){
            sprintf(buf,"Error:%s cannot use.(only *)\r\n",data->s_column);
            IntoSendDataBuf(buf,strlen(buf));
            FreeTableParam(&table_param);
            return(-1);
        }

        /* ��ӱ黻�ҤΥ����å�:��=�װʳ��Ϥ��� */
        if(strcmp(data->cmp,"=")!=0){
            sprintf(buf,"Error:%s cannot use.(only =)\r\n",data->cmp);
            IntoSendDataBuf(buf,strlen(buf));
            FreeTableParam(&table_param);
            return(-1);
        }
    }

    /* �ǡ����ե����뤫�鸡�� */
    if((fp=OpenTable(data->table,"r"))==NULL){
        sprintf(buf,"Error:%s table cannot read.\r\n",data->table);
        IntoSendDataBuf(buf,strlen(buf));
        FreeTableParam(&table_param);
        return(-1);
    }
    /* ����̾������ */
    PrintTableParam(buf,&table_param);
    IntoSendDataBuf(buf,strlen(buf));
    IntoSendDataBuf("\r\n",strlen("\r\n"));
    select_no=0;
    while(1){
        if(ReadTableData(&table_param,fp)==-1){
            break;
        }
        if(data->column==NULL
             ||strcmp(table_param.column[target_column].value,data->value)==0){
            select_no++;
            /* ����Ԥ����� */
            PrintTableData(buf,&table_param);
            IntoSendDataBuf(buf,strlen(buf));
            IntoSendDataBuf("\r\n",strlen("\r\n"));
        }
        for(i=0;i<table_param.column_no;i++){
            free(table_param.column[i].value);
            table_param.column[i].value=NULL;
        }
    }
    CloseTable(data->table,fp);

    /* ���� */
    FreeTableParam(&table_param);

    /* ���� */
    sprintf(buf,"%d record(s) selected.\r\n",select_no);
    IntoSendDataBuf(buf,strlen(buf));

    return(0);
}

int ComSelect(TOKEN *token)
{
SELECT_DATA    data={NULL,NULL,NULL,NULL};
int    token_ptr;
char   buf[512];

    DB(fprintf(stderr,"ComSelect()\n"));

    /* �ǥХå��� */
    DB(DebugToken(stderr,token));

    token_ptr=1;

    /* *������ɥ����å� */
    if(token_ptr<token->no){
        data.s_column=token->token[token_ptr];
    }
    else{
        sprintf(buf,"Error:select *\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    token_ptr++;

    /* from������ɥ����å� */
    if(token_ptr<token->no&&strcmp(token->token[token_ptr],"from")==0){
        /* from������� OK */
    }
    else{
        sprintf(buf,"Error:select * from\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    token_ptr++;

    /* �ơ��֥�̾���� */
    if(token_ptr<token->no){
        data.table=token->token[token_ptr];
    }
    else{
        sprintf(buf,"Error:select * from table\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    token_ptr++;

    /* where������ɥ����å� */
    if(token_ptr<token->no){
        if(strcmp(token->token[token_ptr],"where")==0){
            token_ptr++;

            /* ����̾���� */
            if(token_ptr<token->no){
                data.column=token->token[token_ptr];
            }
            else{
                sprintf(buf,"Error:select * from table where column\r\n");
                IntoSendDataBuf(buf,strlen(buf));
                return(-1);
            }
            token_ptr++;

            /* ��ӱ黻�Ҽ��� */
            if(token_ptr<token->no){
                data.cmp=token->token[token_ptr];
            }
            else{
                sprintf(buf,"Error:select * from table where column cmp\r\n");
                IntoSendDataBuf(buf,strlen(buf));
                return(-1);
            }
            token_ptr++;

            /* �ͼ��� */
            if(token_ptr<token->no){
                data.value=token->token[token_ptr];
            }
            else{
                sprintf(buf,
                   "Error:select * from table where column cmp value\r\n");
                IntoSendDataBuf(buf,strlen(buf));
                return(-1);
            }
            token_ptr++;
        }
        else{
            sprintf(buf,"Error:select * from table where\r\n");
            IntoSendDataBuf(buf,strlen(buf));
            return(-1);
        }
    }

    /* ���������¹� */
    DoSelect(&data);

    return(0);
}

int DoDelete(SELECT_DATA *data)
{
int    i;
int    target_column,delete_no;
TABLE_PARAM    table_param;
char    buf[512],tmp_file[512];
FILE    *fpr,*fpw;

    DB(fprintf(stderr,"DoDelete()\n"));

    /* �ǥХå�ɽ�� */
    DB(fprintf(stderr,"DELETE:table=%s\n",data->table));
    DB(fprintf(stderr,"DELETE:column=%s\n",data->column==NULL?"":data->column));
    DB(fprintf(stderr,"DELETE:cmp=%s\n",data->column==NULL?"":data->cmp));
    DB(fprintf(stderr,"DELETE:value=%s\n",data->column==NULL?"":data->value));

    /* �ѥ�᡼���ե������ɤ߹��� */
    if(ReadTableParam(data->table,&table_param)==-1){
        sprintf(buf,"Error:%s table param not available.\r\n",data->table);
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }

    if(data->column!=NULL){
        /* ����̾�Υ����å� */
        target_column=-1;
        for(i=0;i<table_param.column_no;i++){
            if(strcmp(data->column,table_param.column[i].column)==0){
                target_column=i;
                break;
            }
        }
        if(target_column==-1){    /* ���ܤ�¸�ߤ��ʤ� */
            sprintf(buf,"Error:%s not in table.\r\n",data->column);
            IntoSendDataBuf(buf,strlen(buf));
            FreeTableParam(&table_param);
            return(-1);
        }

        /* ��ӱ黻�ҤΥ����å�:��=�װʳ��Ϥ��� */
        if(strcmp(data->cmp,"=")!=0){
            sprintf(buf,"Error:%s cannot use.(only =)\r\n",data->cmp);
            IntoSendDataBuf(buf,strlen(buf));
            FreeTableParam(&table_param);
            return(-1);
        }
    }

    /* �ǡ����ե����뤫���� */
    if((fpr=OpenTable(data->table,"r"))==NULL){
        sprintf(buf,"Error:%s table cannot read.\r\n",data->table);
        IntoSendDataBuf(buf,strlen(buf));
        FreeTableParam(&table_param);
        return(-1);
    }
    sprintf(tmp_file,"test_server.tmp.%d",getpid());
    if((fpw=fopen(tmp_file,"w"))==NULL){
        sprintf(buf,"Error:Tmp-file cannot write.\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        FreeTableParam(&table_param);
        fclose(fpr);
        return(-1);
    }
    delete_no=0;
    while(1){
        if(ReadTableData(&table_param,fpr)==-1){
            break;
        }
        if(data->column==NULL
           ||strcmp(table_param.column[target_column].value,data->value)==0){
            delete_no++;
        }
        else{
            PrintTableData(buf,&table_param);
            fprintf(fpw,"%s\n",buf);
        }
        for(i=0;i<table_param.column_no;i++){
            free(table_param.column[i].value);
            table_param.column[i].value=NULL;
        }
    }
    /* ��֥ե����륯���� */
    fclose(fpw);

    /* �ǡ����ե������ƥ�ݥ�꡼��̾����mv */
    sprintf(buf,"mv -f %s.dat %s.dat.%d",data->table,data->table,getpid());
    system(buf);

    /* ��֥ե������ǡ����ե������̾����mv */
    sprintf(buf,"mv -f %s %s.dat",tmp_file,data->table);
    system(buf);

    /* CloseTable()�ǥǡ����ե�����򥯥��� */
    CloseTable(data->table,fpr);

    /* �ǡ����ե�����Υƥ�ݥ�꡼̾���ԡ���rm */
    sprintf(buf,"rm -f %s.dat.%d",data->table,getpid());
    system(buf);

    /* ���� */
    FreeTableParam(&table_param);

    /* ���� */
    sprintf(buf,"%d record(s) deleted.\r\n",delete_no);
    IntoSendDataBuf(buf,strlen(buf));

    return(0);
}

int ComDelete(TOKEN *token)
{
SELECT_DATA    data={NULL,NULL,NULL,NULL};
int    token_ptr;
char   buf[512];

    DB(fprintf(stderr,"ComDelete()\n"));

    /* �ǥХå��� */
    DB(DebugToken(stderr,token));

    token_ptr=1;

    /* from������ɥ����å� */
    if(token_ptr<token->no&&strcmp(token->token[token_ptr],"from")==0){
        /* from������� OK */
    }
    else{
        sprintf(buf,"Error:delete from\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    token_ptr++;

    /* �ơ��֥�̾���� */
    if(token_ptr<token->no){
        data.table=token->token[token_ptr];
    }
    else{
        sprintf(buf,"Error:delete from table\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    token_ptr++;

    /* where������ɥ����å� */
    if(token_ptr<token->no){
        if(strcmp(token->token[token_ptr],"where")==0){
            token_ptr++;

            /* ����̾���� */
            if(token_ptr<token->no){
                data.column=token->token[token_ptr];
            }
            else{
                sprintf(buf,"Error:delete from table where column\r\n");
                IntoSendDataBuf(buf,strlen(buf));
                return(-1);
            }
            token_ptr++;

            /* ��ӱ黻�Ҽ��� */
            if(token_ptr<token->no){
                data.cmp=token->token[token_ptr];
            }
            else{
                sprintf(buf,"Error:delete from table where column cmp\r\n");
                IntoSendDataBuf(buf,strlen(buf));
                return(-1);
            }
            token_ptr++;

            /* �ͼ��� */
            if(token_ptr<token->no){
                data.value=token->token[token_ptr];
            }
            else{
                sprintf(buf,"Error:delete from table where column cmp value\r\n");
                IntoSendDataBuf(buf,strlen(buf));
                return(-1);
            }
            token_ptr++;
        }
        else{
            sprintf(buf,"Error:delete from table where\r\n");
            IntoSendDataBuf(buf,strlen(buf));
            return(-1);
        }
    }

    /* ��������¹� */
    DoDelete(&data);

    return(0);
}

int DoUpdate(INSERT_DATA *i_data,SELECT_DATA *s_data)
{
int    i,j,find;
int    target_column,update_no;
TABLE_PARAM    table_param,table_param_set;
char    buf[512],tmp_file[512];
FILE    *fpr,*fpw;

    DB(fprintf(stderr,"DoUpdate()\n"));

    /* �ǥХå�ɽ�� */
    DB(fprintf(stderr,"UPDATE:table=%s\n",i_data->table));
    DB(
    for(i=0;i<i_data->c_v_no;i++){
        fprintf(stderr,"UPDATE:%s:%s\n",
             i_data->c_v[i].column,i_data->c_v[i].value);
    }
    );
    DB(fprintf(stderr,"UPDATE:table=%s\n",s_data->table));
    DB(fprintf(stderr,"UPDATE:column=%s\n",
             s_data->column==NULL?"":s_data->column));
    DB(fprintf(stderr,"UPDATE:cmp=%s\n",s_data->column==NULL?"":s_data->cmp));
    DB(fprintf(stderr,"UPDATE:value=%s\n",
             s_data->column==NULL?"":s_data->value));

    /* �ѥ�᡼���ե������ɤ߹��� */
    if(ReadTableParam(i_data->table,&table_param_set)==-1){
        sprintf(buf,"Error:%s table param not available.\r\n",i_data->table);
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    ReadTableParam(i_data->table,&table_param);

    /* ����̾�Υ����å� */
    for(i=0;i<i_data->c_v_no;i++){
        find=0;
        for(j=0;j<table_param_set.column_no;j++){
            if(strcmp(i_data->c_v[i].column,table_param_set.column[j].column)==0){
                table_param_set.column[j].value=strdup(i_data->c_v[i].value);
                find=1;
                break;
            }
        }
        if(find==0){    /* ���ܤ�¸�ߤ��ʤ� */
            fprintf(stderr,"%s:not in table column.\n",i_data->c_v[i].column);
        }
    }

    if(s_data->column!=NULL){
        /* ����̾�Υ����å� */
        target_column=-1;
        for(i=0;i<table_param.column_no;i++){
            if(strcmp(s_data->column,table_param.column[i].column)==0){
                target_column=i;
                break;
            }
        }
        if(target_column==-1){    /* ���ܤ�¸�ߤ��ʤ� */
            sprintf(buf,"Error:%s not in table.\r\n",s_data->column);
            IntoSendDataBuf(buf,strlen(buf));
            FreeTableParam(&table_param_set);
            FreeTableParam(&table_param);
            return(-1);
        }

        /* ��ӱ黻�ҤΥ����å�:��=�װʳ��Ϥ��� */
        if(strcmp(s_data->cmp,"=")!=0){
            sprintf(buf,"Error:%s cannot use.(only =)\r\n",s_data->cmp);
            IntoSendDataBuf(buf,strlen(buf));
            FreeTableParam(&table_param_set);
            FreeTableParam(&table_param);
            return(-1);
        }
    }

    /* �ǡ����ե�����򹹿� */
    if((fpr=OpenTable(i_data->table,"r"))==NULL){
        sprintf(buf,"Error:%s table cannot read.\r\n",i_data->table);
        IntoSendDataBuf(buf,strlen(buf));
        FreeTableParam(&table_param_set);
        FreeTableParam(&table_param);
        return(-1);
    }
    sprintf(tmp_file,"test_server.tmp.%d",getpid());
    if((fpw=fopen(tmp_file,"w"))==NULL){
        sprintf(buf,"Error:Tmp-file cannot write.\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        FreeTableParam(&table_param_set);
        FreeTableParam(&table_param);
        fclose(fpr);
        return(-1);
    }
    update_no=0;
    while(1){
        if(ReadTableData(&table_param,fpr)==-1){
            break;
        }
        if(s_data->column==NULL
           ||strcmp(table_param.column[target_column].value,s_data->value)==0){
            for(i=0;i<i_data->c_v_no;i++){
                for(j=0;j<table_param.column_no;j++){
                    if(strcmp(i_data->c_v[i].column,
                              table_param.column[j].column)==0){
                        free(table_param.column[j].value);
                        table_param.column[j].value=strdup(i_data->c_v[i].value);
                    }
                }
            }
            update_no++;
        }
        PrintTableData(buf,&table_param);
        fprintf(fpw,"%s\n",buf);
        for(i=0;i<table_param.column_no;i++){
            free(table_param.column[i].value);
            table_param.column[i].value=NULL;
        }
    }
    /* ��֥ե����륯���� */
    fclose(fpw);

    /* �ǡ����ե������ƥ�ݥ�꡼��̾����mv */
    sprintf(buf,"mv -f %s.dat %s.dat.%d",i_data->table,i_data->table,getpid());
    system(buf);

    /* ��֥ե������ǡ����ե������̾����mv */
    sprintf(buf,"mv -f %s %s.dat",tmp_file,i_data->table);
    system(buf);

    /* CloseTable()�ǥǡ����ե�����򥯥��� */
    CloseTable(i_data->table,fpr);

    /* �ǡ����ե�����Υƥ�ݥ�꡼̾���ԡ���rm */
    sprintf(buf,"rm -f %s.dat.%d",i_data->table,getpid());
    system(buf);

    /* ���� */
    FreeTableParam(&table_param_set);
    FreeTableParam(&table_param);

    /* ���� */
    sprintf(buf,"%d record(s) updated.\r\n",update_no);
    IntoSendDataBuf(buf,strlen(buf));

    return(0);
}

int ComUpdate(TOKEN *token)
{
INSERT_DATA    i_data={NULL,NULL,0,0};
SELECT_DATA    s_data={NULL,NULL,NULL,NULL};
int    i,token_ptr,where_flag;
char   buf[512];

    DB(fprintf(stderr,"ComUpdate()\n"));

    /* �ǥХå��� */
    DB(DebugToken(stderr,token));

    token_ptr=1;

    /* �ơ��֥�̾���� */
    if(token_ptr<token->no){
        i_data.table=s_data.table=token->token[token_ptr];
    }
    else{
        sprintf(buf,"Error:update table\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    token_ptr++;

    /* set������ɥ����å� */
    if(token_ptr<token->no&&strcmp(token->token[token_ptr],"set")==0){
        /* set������� OK */
    }
    else{
        sprintf(buf,"Error:update table set\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    token_ptr++;

    /* ����̾����=�ס��� ���� */
    i_data.c_v_size=COLUMN_ALLOC_SIZE;
    i_data.c_v=(COLUMN_VALUE *)calloc(i_data.c_v_size,sizeof(COLUMN_VALUE));
    where_flag=0;
    for(i=token_ptr;i<token->no;i++){
        if(strcmp(token->token[i],"where")==0){
            where_flag=1;
            token_ptr=i;
            break;
        }
        else if(strcmp(token->token[i],",")==0){
            continue;
        }
        else{
            i_data.c_v_no++;
            if(i_data.c_v_no>=i_data.c_v_size){
                i_data.c_v_size+=COLUMN_ALLOC_SIZE;
                i_data.c_v=(COLUMN_VALUE *)realloc(i_data.c_v,
                                i_data.c_v_size*sizeof(COLUMN_VALUE));
            }
            i_data.c_v[i_data.c_v_no-1].column=token->token[i];
            if(i+1<token->no){
                if(strcmp(token->token[i+1],"=")==0){
                }
                else{
                    sprintf(buf,"Error:update table set column =\r\n");
                    IntoSendDataBuf(buf,strlen(buf));
                    free(i_data.c_v);
                    return(-1);
                }
            }
            if(i+2<token->no){
                i_data.c_v[i_data.c_v_no-1].value=token->token[i+2];
                i+=2;
            }
            else{
                sprintf(buf,"Error:update table set column = value\r\n");
                IntoSendDataBuf(buf,strlen(buf));
                free(i_data.c_v);
                return(-1);
            }
        }
    }
    if(i_data.c_v_no==0){    /* ���ܤλ��꤬���Ĥ�ʤ� */
        sprintf(buf,"Error:update set table 'no column = value data!'\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        free(i_data.c_v);
        return(-1);
    }

    /* where������ɥ����å� */
    if(where_flag==1){
        if(strcmp(token->token[token_ptr],"where")==0){
            token_ptr++;

            /* ����̾���� */
            if(token_ptr<token->no){
                s_data.column=token->token[token_ptr];
            }
            else{
                sprintf(buf,
                   "Error:update table set column = value where column\r\n");
                IntoSendDataBuf(buf,strlen(buf));
                free(i_data.c_v);
                return(-1);
            }
            token_ptr++;

            /* ��ӱ黻�Ҽ��� */
            if(token_ptr<token->no){
                s_data.cmp=token->token[token_ptr];
            }
            else{
                sprintf(buf,
                   "Error:update table set column = value where column cmp\r\n");
                IntoSendDataBuf(buf,strlen(buf));
                free(i_data.c_v);
                return(-1);
            }
            token_ptr++;

            /* �ͼ��� */
            if(token_ptr<token->no){
                s_data.value=token->token[token_ptr];
            }
            else{
                sprintf(buf,
                   "Error:update table set column = value where column cmp value\r\n");
                IntoSendDataBuf(buf,strlen(buf));
                free(i_data.c_v);
                return(-1);
            }
            token_ptr++;
        }
        else{
            sprintf(buf,
                   "Error:update table set column = value where\r\n");
            IntoSendDataBuf(buf,strlen(buf));
            free(i_data.c_v);
            return(-1);
        }
    }

    /* ���������¹� */
    DoUpdate(&i_data,&s_data);

    free(i_data.c_v);

    return(0);
}

int ComLock(TOKEN *token)
{
int    token_ptr;
char   buf[512];

    DB(fprintf(stderr,"ComLock()\n"));

    /* �ǥХå��� */
    DB(DebugToken(stderr,token));

    token_ptr=1;

    /* �ơ��֥�̾���� */
    if(token_ptr<token->no){
        LockTable(token->token[token_ptr],LOCK_COMMAND);
        sprintf(buf,"%s locked.\n",token->token[token_ptr]);
        IntoSendDataBuf(buf,strlen(buf));
    }
    else{
        sprintf(buf,"Error:lock table\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }

    return(0);
}

int ComUnlock(TOKEN *token)
{
int    token_ptr;
char   buf[512];

    DB(fprintf(stderr,"ComUnlock()\n"));

    /* �ǥХå��� */
    DB(DebugToken(stderr,token));

    token_ptr=1;

    /* �ơ��֥�̾���� */
    if(token_ptr<token->no){
        UnlockTable(token->token[token_ptr],LOCK_COMMAND);
        sprintf(buf,"%s unlocked.\n",token->token[token_ptr]);
        IntoSendDataBuf(buf,strlen(buf));
    }
    else{
        sprintf(buf,"Error:unlock table\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }

    return(0);
}

extern int TestTable(char *table_name);

int ComTest(TOKEN *token)
{
int    token_ptr;
char   buf[512];

    DB(fprintf(stderr,"ComTest()\n"));

    /* �ǥХå��� */
    DB(DebugToken(stderr,token));

    token_ptr=1;

    /* �ơ��֥�̾���� */
    if(token_ptr<token->no){
        if(TestTable(token->token[token_ptr])==0){
            sprintf(buf,"%s locked.\n",token->token[token_ptr]);
        }
        else{
            sprintf(buf,"%s unlocked.\n",token->token[token_ptr]);
        }
        IntoSendDataBuf(buf,strlen(buf));
    }
    else{
        sprintf(buf,"Error:unlock table\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }

    return(0);
}

int ComError(TOKEN *token)
{
char   buf[512];

    DB(fprintf(stderr,"ComError()\n"));

    /* �ǥХå��� */
    DebugToken(stderr,token);

    sprintf(buf,"Error:[%s] command unknown.\r\n",token->token[0]);
    IntoSendDataBuf(buf,strlen(buf));

    return(0);
}

int ExecRecvCommand(char *buf,int len,int *command_len)
{
int    i,j;
TOKEN  token;
int    end_flag;

    DB(fprintf(stderr,"ExecRecvCommand()\n"));

    *command_len=-1;

    for(i=0;i<len;i++){
        /* SJIS����1�Х����ܡ����������� */
        if(issjiskanji(buf[i])||buf[i]=='\\'){
            i++;
            continue;
        }
        if(buf[i]=='"'){    /* "ʸ���� */
            for(j=i+1;j<len;j++){
                /* SJIS����1�Х����ܡ����������� */
                if(issjiskanji(buf[j])||buf[j]=='\\'){
                    j++;
                    continue;
                }
                if(buf[j]=='"'){
                    break;
                }
            }
            i=j;
            continue;
        }
        if(buf[i]=='\''){    /* 'ʸ���� */
            for(j=i+1;j<len;j++){
                /* SJIS����1�Х����ܡ����������� */
                if(issjiskanji(buf[j])||buf[j]=='\\'){
                    j++;
                    continue;
                }
                if(buf[j]=='\''){
                    break;
                }
            }
            i=j;
            continue;
        }
        if(buf[i]==';'){
            *command_len=i+1;
            break;
        }
    }

    if(*command_len==-1){    /* ���ޥ����Ω���� */
        return(0);
    }

    DB(fprintf(stderr,"command_len=%d : {",*command_len));
    DB(DataPrint(stderr,buf,*command_len));
    DB(fprintf(stderr,"}\n"));

    /* �ȡ������ʬΥ */
    GetToken(buf,*command_len-1,&token,TOKEN_SEPARATE_COMMAND,TOKEN_SEPARATE_POINT_COMMAND);
    /* ���ޥ��ʬ�� */
    end_flag=0;
    if(token.no>0){
        if(strcmp(token.token[0],"end")==0){    /* ��λ���ޥ�� */
            end_flag=1;
        }
        else if(strcmp(token.token[0],"insert")==0){    /* ��Ͽ���ޥ�� */
            ComInsert(&token);
        }
        else if(strcmp(token.token[0],"select")==0){    /* �������ޥ�� */
            ComSelect(&token);
        }
        else if(strcmp(token.token[0],"delete")==0){    /* ������ޥ�� */
            ComDelete(&token);
        }
        else if(strcmp(token.token[0],"update")==0){    /* �������ޥ�� */
            ComUpdate(&token);
        }
        else if(strcmp(token.token[0],"lock")==0){      /* ��å����ޥ�� */
            ComLock(&token);
        }
        else if(strcmp(token.token[0],"unlock")==0){    /* �����å����ޥ�� */
            ComUnlock(&token);
        }
        else if(strcmp(token.token[0],"test")==0){      /* ��å��ƥ��ȥ��ޥ�� */
            ComTest(&token);
        }
        else{
            ComError(&token);
        }
    }

    /* �ȡ�������� */
    FreeToken(&token);

    return(end_flag);
}

