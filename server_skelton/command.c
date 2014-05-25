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
        /* SJIS漢字1バイト目、エスケープ */
        if(issjiskanji(buf[i])||buf[i]=='\\'){
            i++;
        }
        else if(buf[i]=='"'){    /* "文字列 */
            for(j=i+1;j<len;j++){
                /* SJIS漢字1バイト目、エスケープ */
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
        else if(buf[i]=='\''){    /* '文字列 */
            for(j=i+1;j<len;j++){
                /* SJIS漢字1バイト目、エスケープ */
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
        else if(strchr(token_separate,buf[i])!=NULL){    /* 分離文字 */
            token_len=i-token_start;
            if(token_len>0){
                AddToken(token,&buf[token_start],token_len);
            }
            token_start=i+1;
        }
        else if(strchr(token_separate_point,buf[i])!=NULL){    /* 区切り分離文字 */
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
        if(buf[0]=='#'){        /* コメント行 */
            continue;
        }
        if((ptr=strchr(buf,'\n'))!=NULL){       /* 改行文字を取る */
            *ptr='\0';
        }
        if(strlen(buf)==0){    /* 長さゼロ */
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

/* プロセス内ロック状態 */
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

    /* デバッグ表示 */
    DB(fprintf(stderr,"INSERT:table=%s\n",data->table));
    DB(
    for(i=0;i<data->c_v_no;i++){
        fprintf(stderr,"INSERT:%s:%s\n",data->c_v[i].column,data->c_v[i].value);
    }
    );

    /* パラメータファイル読み込み */
    if(ReadTableParam(data->table,&table_param)==-1){
        sprintf(buf,"Error:%s table param not available.\r\n",data->table);
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }

    /* 項目名のチェック */
    for(i=0;i<data->c_v_no;i++){
        find=0;
        for(j=0;j<table_param.column_no;j++){
            if(strcmp(data->c_v[i].column,table_param.column[j].column)==0){
                table_param.column[j].value=strdup(data->c_v[i].value);
                find=1;
                break;
            }
        }
        if(find==0){    /* 項目が存在しない */
            fprintf(stderr,"%s:not in table column.\n",data->c_v[i].column);
        }
    }

    /* データファイルに格納 */
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

    /* 解放 */
    FreeTableParam(&table_param);

    /* 返答 */
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

    /* デバッグ用 */
    DB(DebugToken(stderr,token));

    token_ptr=1;

    /* intoキーワードチェック */
    if(token_ptr<token->no&&strcmp(token->token[token_ptr],"into")==0){
        /* intoキーワード OK */
    }
    else{
        sprintf(buf,"Error:insert into\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    token_ptr++;

    /* テーブル名取得 */
    if(token_ptr<token->no){
        data.table=token->token[token_ptr];
    }
    else{
        sprintf(buf,"Error:insert into table\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    token_ptr++;

    /* 「(」から「)」まで項目名取得 */
    data.c_v_size=COLUMN_ALLOC_SIZE;
    data.c_v=(COLUMN_VALUE *)calloc(data.c_v_size,sizeof(COLUMN_VALUE));
    end_flag=0;
    for(i=token_ptr;i<token->no;i++){
        if(i==token_ptr){
            if(strcmp(token->token[i],"(")==0){
                /* 「(」OK */
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
    if(end_flag==0){    /* 終了の括弧がない */
        sprintf(buf,"Error:insert into table (...)\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        free(data.c_v);
        return(-1);
    }
    else if(data.c_v_no==0){    /* 項目の指定が１つもない */
        sprintf(buf,"Error:insert into table (no column data!)\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        free(data.c_v);
        return(-1);
    }

    /* valuesキーワードチェック */
    if(token_ptr<token->no&&strcmp(token->token[token_ptr],"values")==0){
        /* valuesキーワード OK */
    }
    else{
        sprintf(buf,"Error:insert into table (...) values\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        free(data.c_v);
        return(-1);
    }
    token_ptr++;

    /* 「(」から「)」まで項目名取得 */
    end_flag=0;
    c_v_no=0;
    for(i=token_ptr;i<token->no;i++){
        if(i==token_ptr){
            if(strcmp(token->token[i],"(")==0){
                /* 「(」OK */
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
    if(end_flag==0){    /* 終了の括弧がない */
        sprintf(buf,"Error:insert into table (...) values (...)\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        free(data.c_v);
        return(-1);
    }
    else if(data.c_v_no!=c_v_no){    /* 項目と数が一致しない */
        sprintf(buf,"Error:insert into table (...) values (value no error!)\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        free(data.c_v);
        return(-1);
    }

    /* 登録処理実行 */
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

    /* デバッグ表示 */
    DB(fprintf(stderr,"SELECT:table=%s\n",data->table));
    DB(fprintf(stderr,"SELECT:column=%s\n",data->column==NULL?"":data->column));
    DB(fprintf(stderr,"SELECT:cmp=%s\n",data->column==NULL?"":data->cmp));
    DB(fprintf(stderr,"SELECT:value=%s\n",data->column==NULL?"":data->value));

    /* パラメータファイル読み込み */
    if(ReadTableParam(data->table,&table_param)==-1){
        sprintf(buf,"Error:%s table param not available.\r\n",data->table);
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }

    if(data->column!=NULL){
        /* 項目名のチェック */
        target_column=-1;
        for(i=0;i<table_param.column_no;i++){
            if(strcmp(data->column,table_param.column[i].column)==0){
                target_column=i;
                break;
            }
        }
        if(target_column==-1){    /* 項目が存在しない */
            sprintf(buf,"Error:%s not in table.\r\n",data->column);
            IntoSendDataBuf(buf,strlen(buf));
            FreeTableParam(&table_param);
            return(-1);
        }

        /* 表示項目のチェック:「*」以外はだめ */
        if(strcmp(data->s_column,"*")!=0){
            sprintf(buf,"Error:%s cannot use.(only *)\r\n",data->s_column);
            IntoSendDataBuf(buf,strlen(buf));
            FreeTableParam(&table_param);
            return(-1);
        }

        /* 比較演算子のチェック:「=」以外はだめ */
        if(strcmp(data->cmp,"=")!=0){
            sprintf(buf,"Error:%s cannot use.(only =)\r\n",data->cmp);
            IntoSendDataBuf(buf,strlen(buf));
            FreeTableParam(&table_param);
            return(-1);
        }
    }

    /* データファイルから検索 */
    if((fp=OpenTable(data->table,"r"))==NULL){
        sprintf(buf,"Error:%s table cannot read.\r\n",data->table);
        IntoSendDataBuf(buf,strlen(buf));
        FreeTableParam(&table_param);
        return(-1);
    }
    /* 項目名を返答 */
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
            /* 選択行を返答 */
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

    /* 解放 */
    FreeTableParam(&table_param);

    /* 返答 */
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

    /* デバッグ用 */
    DB(DebugToken(stderr,token));

    token_ptr=1;

    /* *キーワードチェック */
    if(token_ptr<token->no){
        data.s_column=token->token[token_ptr];
    }
    else{
        sprintf(buf,"Error:select *\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    token_ptr++;

    /* fromキーワードチェック */
    if(token_ptr<token->no&&strcmp(token->token[token_ptr],"from")==0){
        /* fromキーワード OK */
    }
    else{
        sprintf(buf,"Error:select * from\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    token_ptr++;

    /* テーブル名取得 */
    if(token_ptr<token->no){
        data.table=token->token[token_ptr];
    }
    else{
        sprintf(buf,"Error:select * from table\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    token_ptr++;

    /* whereキーワードチェック */
    if(token_ptr<token->no){
        if(strcmp(token->token[token_ptr],"where")==0){
            token_ptr++;

            /* 項目名取得 */
            if(token_ptr<token->no){
                data.column=token->token[token_ptr];
            }
            else{
                sprintf(buf,"Error:select * from table where column\r\n");
                IntoSendDataBuf(buf,strlen(buf));
                return(-1);
            }
            token_ptr++;

            /* 比較演算子取得 */
            if(token_ptr<token->no){
                data.cmp=token->token[token_ptr];
            }
            else{
                sprintf(buf,"Error:select * from table where column cmp\r\n");
                IntoSendDataBuf(buf,strlen(buf));
                return(-1);
            }
            token_ptr++;

            /* 値取得 */
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

    /* 検索処理実行 */
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

    /* デバッグ表示 */
    DB(fprintf(stderr,"DELETE:table=%s\n",data->table));
    DB(fprintf(stderr,"DELETE:column=%s\n",data->column==NULL?"":data->column));
    DB(fprintf(stderr,"DELETE:cmp=%s\n",data->column==NULL?"":data->cmp));
    DB(fprintf(stderr,"DELETE:value=%s\n",data->column==NULL?"":data->value));

    /* パラメータファイル読み込み */
    if(ReadTableParam(data->table,&table_param)==-1){
        sprintf(buf,"Error:%s table param not available.\r\n",data->table);
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }

    if(data->column!=NULL){
        /* 項目名のチェック */
        target_column=-1;
        for(i=0;i<table_param.column_no;i++){
            if(strcmp(data->column,table_param.column[i].column)==0){
                target_column=i;
                break;
            }
        }
        if(target_column==-1){    /* 項目が存在しない */
            sprintf(buf,"Error:%s not in table.\r\n",data->column);
            IntoSendDataBuf(buf,strlen(buf));
            FreeTableParam(&table_param);
            return(-1);
        }

        /* 比較演算子のチェック:「=」以外はだめ */
        if(strcmp(data->cmp,"=")!=0){
            sprintf(buf,"Error:%s cannot use.(only =)\r\n",data->cmp);
            IntoSendDataBuf(buf,strlen(buf));
            FreeTableParam(&table_param);
            return(-1);
        }
    }

    /* データファイルから削除 */
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
    /* 中間ファイルクローズ */
    fclose(fpw);

    /* データファイルをテンポラリーな名前にmv */
    sprintf(buf,"mv -f %s.dat %s.dat.%d",data->table,data->table,getpid());
    system(buf);

    /* 中間ファイルをデータファイルの名前にmv */
    sprintf(buf,"mv -f %s %s.dat",tmp_file,data->table);
    system(buf);

    /* CloseTable()でデータファイルをクローズ */
    CloseTable(data->table,fpr);

    /* データファイルのテンポラリー名コピーをrm */
    sprintf(buf,"rm -f %s.dat.%d",data->table,getpid());
    system(buf);

    /* 解放 */
    FreeTableParam(&table_param);

    /* 返答 */
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

    /* デバッグ用 */
    DB(DebugToken(stderr,token));

    token_ptr=1;

    /* fromキーワードチェック */
    if(token_ptr<token->no&&strcmp(token->token[token_ptr],"from")==0){
        /* fromキーワード OK */
    }
    else{
        sprintf(buf,"Error:delete from\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    token_ptr++;

    /* テーブル名取得 */
    if(token_ptr<token->no){
        data.table=token->token[token_ptr];
    }
    else{
        sprintf(buf,"Error:delete from table\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    token_ptr++;

    /* whereキーワードチェック */
    if(token_ptr<token->no){
        if(strcmp(token->token[token_ptr],"where")==0){
            token_ptr++;

            /* 項目名取得 */
            if(token_ptr<token->no){
                data.column=token->token[token_ptr];
            }
            else{
                sprintf(buf,"Error:delete from table where column\r\n");
                IntoSendDataBuf(buf,strlen(buf));
                return(-1);
            }
            token_ptr++;

            /* 比較演算子取得 */
            if(token_ptr<token->no){
                data.cmp=token->token[token_ptr];
            }
            else{
                sprintf(buf,"Error:delete from table where column cmp\r\n");
                IntoSendDataBuf(buf,strlen(buf));
                return(-1);
            }
            token_ptr++;

            /* 値取得 */
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

    /* 削除処理実行 */
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

    /* デバッグ表示 */
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

    /* パラメータファイル読み込み */
    if(ReadTableParam(i_data->table,&table_param_set)==-1){
        sprintf(buf,"Error:%s table param not available.\r\n",i_data->table);
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    ReadTableParam(i_data->table,&table_param);

    /* 項目名のチェック */
    for(i=0;i<i_data->c_v_no;i++){
        find=0;
        for(j=0;j<table_param_set.column_no;j++){
            if(strcmp(i_data->c_v[i].column,table_param_set.column[j].column)==0){
                table_param_set.column[j].value=strdup(i_data->c_v[i].value);
                find=1;
                break;
            }
        }
        if(find==0){    /* 項目が存在しない */
            fprintf(stderr,"%s:not in table column.\n",i_data->c_v[i].column);
        }
    }

    if(s_data->column!=NULL){
        /* 項目名のチェック */
        target_column=-1;
        for(i=0;i<table_param.column_no;i++){
            if(strcmp(s_data->column,table_param.column[i].column)==0){
                target_column=i;
                break;
            }
        }
        if(target_column==-1){    /* 項目が存在しない */
            sprintf(buf,"Error:%s not in table.\r\n",s_data->column);
            IntoSendDataBuf(buf,strlen(buf));
            FreeTableParam(&table_param_set);
            FreeTableParam(&table_param);
            return(-1);
        }

        /* 比較演算子のチェック:「=」以外はだめ */
        if(strcmp(s_data->cmp,"=")!=0){
            sprintf(buf,"Error:%s cannot use.(only =)\r\n",s_data->cmp);
            IntoSendDataBuf(buf,strlen(buf));
            FreeTableParam(&table_param_set);
            FreeTableParam(&table_param);
            return(-1);
        }
    }

    /* データファイルを更新 */
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
    /* 中間ファイルクローズ */
    fclose(fpw);

    /* データファイルをテンポラリーな名前にmv */
    sprintf(buf,"mv -f %s.dat %s.dat.%d",i_data->table,i_data->table,getpid());
    system(buf);

    /* 中間ファイルをデータファイルの名前にmv */
    sprintf(buf,"mv -f %s %s.dat",tmp_file,i_data->table);
    system(buf);

    /* CloseTable()でデータファイルをクローズ */
    CloseTable(i_data->table,fpr);

    /* データファイルのテンポラリー名コピーをrm */
    sprintf(buf,"rm -f %s.dat.%d",i_data->table,getpid());
    system(buf);

    /* 解放 */
    FreeTableParam(&table_param_set);
    FreeTableParam(&table_param);

    /* 返答 */
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

    /* デバッグ用 */
    DB(DebugToken(stderr,token));

    token_ptr=1;

    /* テーブル名取得 */
    if(token_ptr<token->no){
        i_data.table=s_data.table=token->token[token_ptr];
    }
    else{
        sprintf(buf,"Error:update table\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    token_ptr++;

    /* setキーワードチェック */
    if(token_ptr<token->no&&strcmp(token->token[token_ptr],"set")==0){
        /* setキーワード OK */
    }
    else{
        sprintf(buf,"Error:update table set\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        return(-1);
    }
    token_ptr++;

    /* 項目名、「=」、値 取得 */
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
    if(i_data.c_v_no==0){    /* 項目の指定が１つもない */
        sprintf(buf,"Error:update set table 'no column = value data!'\r\n");
        IntoSendDataBuf(buf,strlen(buf));
        free(i_data.c_v);
        return(-1);
    }

    /* whereキーワードチェック */
    if(where_flag==1){
        if(strcmp(token->token[token_ptr],"where")==0){
            token_ptr++;

            /* 項目名取得 */
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

            /* 比較演算子取得 */
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

            /* 値取得 */
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

    /* 更新処理実行 */
    DoUpdate(&i_data,&s_data);

    free(i_data.c_v);

    return(0);
}

int ComLock(TOKEN *token)
{
int    token_ptr;
char   buf[512];

    DB(fprintf(stderr,"ComLock()\n"));

    /* デバッグ用 */
    DB(DebugToken(stderr,token));

    token_ptr=1;

    /* テーブル名取得 */
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

    /* デバッグ用 */
    DB(DebugToken(stderr,token));

    token_ptr=1;

    /* テーブル名取得 */
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

    /* デバッグ用 */
    DB(DebugToken(stderr,token));

    token_ptr=1;

    /* テーブル名取得 */
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

    /* デバッグ用 */
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
        /* SJIS漢字1バイト目、エスケープ */
        if(issjiskanji(buf[i])||buf[i]=='\\'){
            i++;
            continue;
        }
        if(buf[i]=='"'){    /* "文字列 */
            for(j=i+1;j<len;j++){
                /* SJIS漢字1バイト目、エスケープ */
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
        if(buf[i]=='\''){    /* '文字列 */
            for(j=i+1;j<len;j++){
                /* SJIS漢字1バイト目、エスケープ */
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

    if(*command_len==-1){    /* コマンド成立せず */
        return(0);
    }

    DB(fprintf(stderr,"command_len=%d : {",*command_len));
    DB(DataPrint(stderr,buf,*command_len));
    DB(fprintf(stderr,"}\n"));

    /* トークンに分離 */
    GetToken(buf,*command_len-1,&token,TOKEN_SEPARATE_COMMAND,TOKEN_SEPARATE_POINT_COMMAND);
    /* コマンド分岐 */
    end_flag=0;
    if(token.no>0){
        if(strcmp(token.token[0],"end")==0){    /* 終了コマンド */
            end_flag=1;
        }
        else if(strcmp(token.token[0],"insert")==0){    /* 登録コマンド */
            ComInsert(&token);
        }
        else if(strcmp(token.token[0],"select")==0){    /* 検索コマンド */
            ComSelect(&token);
        }
        else if(strcmp(token.token[0],"delete")==0){    /* 削除コマンド */
            ComDelete(&token);
        }
        else if(strcmp(token.token[0],"update")==0){    /* 更新コマンド */
            ComUpdate(&token);
        }
        else if(strcmp(token.token[0],"lock")==0){      /* ロックコマンド */
            ComLock(&token);
        }
        else if(strcmp(token.token[0],"unlock")==0){    /* アンロックコマンド */
            ComUnlock(&token);
        }
        else if(strcmp(token.token[0],"test")==0){      /* ロックテストコマンド */
            ComTest(&token);
        }
        else{
            ComError(&token);
        }
    }

    /* トークン解放 */
    FreeToken(&token);

    return(end_flag);
}

