#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <sys/types.h>
#include    <sys/uio.h>
#include    <sys/fcntl.h>
#include    <unistd.h>
#include    <errno.h>

/* ��å��ե�����ǡ�����¤�� */
typedef    struct    {
    int    pid;
    int    flag;
}LOCK_FILE;

/* �ץ������å����� */
typedef struct    {
    int    flag;
    char    table_name[256];
    int    fd;
}LOCK_DATA;
typedef struct    {
    LOCK_DATA    *lock;
    int    lock_size;
    int    lock_no;
}LOCK_MEMORY;
LOCK_MEMORY    LockMemory={NULL,0,0};

LOCK_DATA *LockMemoryGet(char *table_name,int create_flag)
{
int    i,no,free_no;
char   buf[512];

    /* ��å�����θ��� */
    no=-1;
    free_no=-1;
    for(i=0;i<LockMemory.lock_no;i++){
        if(LockMemory.lock[i].flag==0){    /* ���� */
            free_no=i;
        }
        else if(strcmp(LockMemory.lock[i].table_name,table_name)==0){
            no=i;
            break;
        }
    }

    /* ������å�����̵���ǡ������ե饰������ξ�硧��å������ɲ� */
    if(no==-1&&create_flag==1){
        if(free_no==-1){
            if(LockMemory.lock_size==0){
                LockMemory.lock_size=64;
                LockMemory.lock=(LOCK_DATA *)calloc(LockMemory.lock_size,
                                                            sizeof(LOCK_DATA));
            }
            else{
                LockMemory.lock_size+=64;
                LockMemory.lock=(LOCK_DATA *)realloc(LockMemory.lock, 
                                        LockMemory.lock_size*sizeof(LOCK_DATA));
            }
            no=LockMemory.lock_no;
            LockMemory.lock_no++;
        }
        else{
            no=free_no;
        }
        LockMemory.lock[no].flag=1;
        strcpy(LockMemory.lock[no].table_name,table_name);
        sprintf(buf,"%s.lock",table_name);
        LockMemory.lock[no].fd=open(buf,O_RDWR|O_CREAT,0666);
    }

    if(no==-1){
        return(NULL);
    }
    else{
        return(&LockMemory.lock[no]);
    }
}

int LockTable(char *table_name,int flag)
{
LOCK_DATA    *lock_data;
LOCK_FILE    lock_file;

    /* �ץ������å���������� */
    lock_data=LockMemoryGet(table_name,1);

    /* ��å���¾�ץ�������å����Ƥ������Ԥ� */
    lockf(lock_data->fd,F_LOCK,0);

    /* ���ߤΥ�å��ե������������ */
    lseek(lock_data->fd,0,SEEK_SET);
    lock_file.pid=0;
    lock_file.flag=0;
    read(lock_data->fd,&lock_file,sizeof(lock_file));

    if(lock_file.pid==getpid()){    /* ���ץ����ξ��󤬤��� */
        if(lock_file.flag<flag){    /* ����Υ�å����������������Х��å� */
            lock_file.flag=flag;
        }
    }
    else{    /* ¾�ץ����ξ��󤬤��롧���� */
        lock_file.pid=getpid();
        lock_file.flag=flag;
    }

    /* ��å��ե���������񤭹��� */
    lseek(lock_data->fd,0,SEEK_SET);
    write(lock_data->fd,&lock_file,sizeof(lock_file));

    return(1);
}

int UnlockTable(char *table_name,int flag)
{
LOCK_DATA    *lock_data;
LOCK_FILE    lock_file;

    /* �ץ������å���������� */
    lock_data=LockMemoryGet(table_name,0);
    if(lock_data!=NULL){    /* ��å����󤢤� */
        /* ���ߤΥ�å��ե������������ */
        lseek(lock_data->fd,0,SEEK_SET);
        lock_file.pid=0;
        lock_file.flag=0;
        read(lock_data->fd,&lock_file,sizeof(lock_file));

        if(lock_file.pid==getpid()){    /* ���ץ����ξ��󤬤��� */
            if(lock_file.flag<=flag){    /* ������褦�Ȥ���ե饰��Ʊ��������ʲ��ʤ饻�å� */
                lock_file.pid=0;
                lock_file.flag=0;
            }
        }
        else{    /* ¾�ץ����ξ��󤬤��롧���� */
            lock_file.pid=0;
            lock_file.flag=0;
        }

        if(lock_file.flag==0){    /* �����å��ˤʤ��� */
            /* ��å��ե���������񤭹��� */
            lseek(lock_data->fd,0,SEEK_SET);
            write(lock_data->fd,&lock_file,sizeof(lock_file));

            /* �ե�����Υ�å���� */
            lockf(lock_data->fd,F_ULOCK,0);

            /* ��å����󥯥ꥢ */
            close(lock_data->fd);
            lock_data->flag=0;
        }
    }

    return(1);
}

int TestTable(char *table_name)
{
LOCK_DATA    *lock_data;
LOCK_FILE    lock_file;
int    ret;

    /* �ץ������å���������� */
    lock_data=LockMemoryGet(table_name,1);
    ret=lockf(lock_data->fd,F_TEST,0);
    if(ret==-1){    /* ¾�ץ�������å��� */
        /* ��å����󥯥ꥢ */
        close(lock_data->fd);
        lock_data->flag=0;
        return(0);
    }
    else{    /* ��å�����Ƥ��ʤ� */
        /* ���ߤΥ�å��ե������������ */
        lseek(lock_data->fd,0,SEEK_SET);
        lock_file.pid=0;
        lock_file.flag=0;
        read(lock_data->fd,&lock_file,sizeof(lock_file));
        if(lock_file.pid==getpid()){    /* ���ץ�������å��� */
        }
        else{    /* ���ץ����ϥ�å����Ƥ��ʤ� */
            /* ��å����󥯥ꥢ */
            close(lock_data->fd);
            lock_data->flag=0;
        }
        return(1);
    }
}
