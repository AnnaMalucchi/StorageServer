#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> //Necessaria per lettura della dimensione "Config.txt"
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <assert.h>
#include <sys/un.h>
#include <errno.h>
#include <pthread.h>
//#include "ServerRequestManagement.h"
#include "File.h"
#include "LogFile.h"

#define NR_MAX_FILE 10
#define DIM_MAX 128
#define NR_THREAD 5
#define SOCKET_NAME "./sockname"
#define LOGFILE "LOG"
#define UNIX_PATH_MAX 32
#define SYSCALL(c,e) \
    if(c==-1) { perror(e);exit(EXIT_FAILURE); }
#define SYSCALL_BREAK(c,e) \
    if(c==-1) { perror(e);exit(EXIT_FAILURE); break; }
#define SYSCALL_PTHREAD(e,c,s) \
    if((e=c)!=0) { errno=e;perror(s);fflush(stdout);exit(EXIT_FAILURE); }


//SPOSTARE
#define CONTROL(dim, e)\
    if(dim==-1 || dim==0){perror(e); return EXIT_FAILURE;}

#define CONTROL_NULL(string, e)\
    if(!string){perror(e); return EXIT_FAILURE; }


typedef struct node {
    int data;
    struct node  * next;
} list_t;



typedef enum{
    OPEN_FILE,           //0
    READ_FILE,           //1
    READN_FILE,          //2
    WRITE_FILE,          //3
    WRITE_FILE_D,        //4 //caso in cui passo directory
    APPENDTO_FILE,       //5
    LOCK_FILE,           //6
    UNLOCK_FILE,         //7
    CLOSE_FILE,          //8
    REMOVE_FILE,         //9
    CLOSE_CONNECTION,    //10
}operation_t;

typedef struct msg {
    int len;
    char *str;
} msg_t;


#define EXIT_ARG_NOT_FOUND -3
#define EXIT_ERROR -2
#define QUICK_CLOSURE 1
#define SLOW_CLOSURE 2
#define OPEN 0

//variabili globali
int nrThread, nrMaxFile, dimMaxBytes, nr_client=0;
char nameFileLog[32];
volatile sig_atomic_t typeClose=OPEN;


list_t * coda = NULL; //CODA DI COMUNICAZIONE MANAGER --> WORKERS / RISORSA CONDIVISA / CODA FIFO 
pthread_mutex_t lock_coda = PTHREAD_MUTEX_INITIALIZER;
//pthread_cond_t signal_coda = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutexStorage = PTHREAD_MUTEX_INITIALIZER;

void HelloServer(int nrThread, int nrMaxFile, int dimMaxBytes, char* nameSocketFile, char *nameFileLog){
    printf("---FileStorageServer active---\n");
    printf("\nREADING CONFIGURATION FILE: \n");
    printf("\tNUMBER OF THREADS: %d\n", nrThread);
    printf("\tMAXIMUM NUMBER OF FILES: %d\n", nrMaxFile);
    printf("\tMAXIMUM SIZE IN BYTES: %d\n", dimMaxBytes);
    printf("\tSOCKET NAME FILE: %s\n", nameSocketFile);
    printf("\tLOG FILE NAME: %s\n", nameFileLog);
    printf("\t---------------------------\n");
}

void ReadConfigurationFile(const char* confFile, int *nrThread, int *nrMaxFile, int *dimMaxBytes, char *nameSocketFile, char *nameFileLog){

    struct stat st;

    //------APERTURA DEL FILE "config.txt"------
    int fd=open(confFile, O_RDONLY);
    if(fd==-1){
        perror("ERROR: Incorrect opening of the file\n");
        exit(EXIT_FAILURE);
    }
    
    //Capisco la dimensione del file e alloco un buffer di tale dimensione
    //La stat ritorna 0 in caso di successo, altrimenti -1
    if(stat(confFile, &st)==-1){
        perror("ERROR: Incorrect file reading\n");
        exit(EXIT_FAILURE);
    }

    //Verificare che il file non sia vuoto
    //Se il file è vuoto fornisce al FileStorage una configurazione standard
    if(st.st_size==0){
        *nrThread=NR_THREAD;
        *nrMaxFile=NR_MAX_FILE;
        *dimMaxBytes=DIM_MAX;
        strcpy(nameSocketFile, SOCKET_NAME);
        strcpy(nameFileLog, LOGFILE);
        return;
    }

    char *buffer=(char *)malloc((st.st_size+1)*sizeof(char));
    buffer[st.st_size]='\0';
    int aux=1;
    while(aux>0){
        aux=read(fd, buffer, st.st_size);
    }
    if(aux==-1){
        free(buffer);
        perror("ERROR: Incorrect file reading");
        exit(EXIT_FAILURE);
    }
    close(fd);

    //Devo avere due delimitatori, uno che riguarda la '=' e uno per '/0'
    char delim[2];
    delim[0]='=';
    delim[1]='\n';
    char* token=strtok(buffer, delim);

    while(token!=NULL){
        if(strcmp(token, "NrThread")==0){
            token=strtok(NULL, delim);
            *nrThread=atoi(token);

            //continua con il prossimo token
            token=strtok(NULL, delim);
            continue;
        }
        if(strcmp(token, "NrMaxFile")==0){
            token=strtok(NULL, delim);
            *nrMaxFile=atoi(token);

            //continua con il prossimo token
            token=strtok(NULL, delim);
            continue;
        }
        if(strcmp(token, "DimMaxBytes")==0){
            token=strtok(NULL, delim);
            *dimMaxBytes=atoi(token);

            //continua con il prossimo token
            token=strtok(NULL, delim);
            continue;
        }
        if(strcmp(token, "NameSocketFile")==0){
            token=strtok(NULL, delim);
            strcpy(nameSocketFile, token);

            //continua con il prossimo token
            token=strtok(NULL, delim);
            continue;
        }
        if(strcmp(token, "NameFileLog")==0){
            token=strtok(NULL, delim);
            strcpy(nameFileLog, token);

            //continua con il prossimo token
            token=strtok(NULL, delim);
            continue;
        }

        //se non entra in nessuno degli if
        token=strtok(NULL, delim);
    }

    free(buffer);
    return;
}

void insertlist (list_t ** list, int data) {
    //fflush(stdout);
    int err;
    //Prendere la lock
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&lock_coda),"Lock");
    list_t * new = malloc (sizeof(list_t));
    new->data = data;
    new->next = *list;
    //Inserimento in testa
    *list = new;
    //Invio signal
    SYSCALL_PTHREAD(err,pthread_cond_signal(&not_empty),"Signal");
    //Rilascio della lock
    pthread_mutex_unlock(&lock_coda);
    nr_client++;
}

int removelist_t (list_t ** list) {
    int err;
    //Prendo lock
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&lock_coda),"Lock");
    while (coda==NULL) {
        pthread_cond_wait(&not_empty, &lock_coda);
        //fflush(stdout);
    } 
    //printf("Consumatore Svegliato\n");
    int data;
    list_t * curr = *list;
    list_t * prev = NULL;
    while (curr->next != NULL) {
        prev = curr;
        curr = curr->next;
    }
    data = curr->data;
    //Rimozione nodo
    if (prev == NULL) {
        *list = NULL;
    }else{
        prev->next = NULL;
        free(curr);
    }
    //Rilascio della lock
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&lock_coda),"Lock"); 
    nr_client--;
    return data;
}



// ritorno l'indice massimo tra i descrittori attivi
int updatemax(fd_set set, int fdmax) {
    for(int i=(fdmax-1);i>=0;--i)
	if (FD_ISSET(i, &set)) return i;
    assert(1==0);
    return -1;
}


int OpenFile(int client, int worker){
    int flags, dim;
    msg_t pathname;
    int err; 

    //---Leggo la dimensione del pathname inviato dal Client
    dim=readn_request(client, &pathname.len, sizeof(int));
    CONTROL(dim, "Request failed\n");
    
    //---Leggo il pathname inviato dal Client
    pathname.str=malloc((pathname.len+1)*sizeof(char));
    for(int i=0; i<pathname.len+1; i++){
        pathname.str[i]='\0';
    }
    CONTROL_NULL(pathname.str, "Calloc error\n");
    if(readn_request(client, pathname.str, pathname.len*sizeof(char))==-1){
        free(pathname.str);
        perror("Request read error\n"); 
        return EXIT_FAILURE;
    }
    
    //---Leggo flags inviato dal Client
    dim=readn_request(client, &flags, sizeof(int));
    CONTROL(dim, "Request failes\n");    
    
    //---Gestione O_CREATE E O_LOCK
    if(flags==1){
        SYSCALL_PTHREAD(err,pthread_mutex_lock(&mutexStorage),"Lock");
        //--- Controlli per la creazione File
        if(find_File(pathname.str)==1){
            printf("File %s -->already exists\n", pathname.str);
            free(pathname.str);
            SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Lock");  
            return EXIT_FAILURE;
        }
        while(nrFile_Storage()==nrMaxFile || dimFile_Storage()+pathname.len+1>=dimMaxBytes){
            remove_File(NULL);
            dimStorage_Update();
            break;
        }
        insertFile(client ,pathname.str, pathname.str, pathname.len);
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");  
    }
    if(flags==0){
        SYSCALL_PTHREAD(err,pthread_mutex_lock(&mutexStorage),"Lock");
        //Verifico che il FILE esista
        if(find_File(pathname.str)!=1){
            printf("File %s -->not exists\n", pathname.str);
            free(pathname.str);
            SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");  
            return EXIT_FAILURE;
        }
        File_t *file;
        file=return_File(pathname.str);
        if(file->fd_lock!=-1 && file->fd_lock!=client){
            printf("File %s -->already locked\n", pathname.str);
            free(pathname.str);
            SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");  
            return EXIT_FAILURE;
        }        
        changeFdLock(file, client);
        Write_File_lock(file);
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");  
    }
    free(pathname.str);   
    printf("Fine dell'operazione\n");
    return EXIT_SUCCESS;
}

int readFile(int client, int workers){
    int dim;
    msg_t pathname;
    int err; 

    //---Leggo la dimensione del pathname inviato dal Client
    dim=readn_request(client, &pathname.len, sizeof(int));
    CONTROL(dim, "Request failes\n");
   
    //---Leggo il pathname inviato dal Client
    pathname.str=malloc((pathname.len+1)*sizeof(char));
    for(int i=0; i<pathname.len+1; i++){
        pathname.str[i]='\0';
    }
    CONTROL_NULL(pathname.str, "Malloc error\n");
    if(readn_request(client, pathname.str, pathname.len*sizeof(char))==-1){
        free(pathname.str);
        perror("Request read error\n"); 
        return EXIT_FAILURE;
    }
    printf("pathanme: %s\n", pathname.str);
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&mutexStorage),"Lock");
    //Verifico che il FILE esista
    if(find_File(pathname.str)==0){
        printf("File %s -->not exists\n", pathname.str);
        int aux=0;
        if(writen_response(client, &aux, sizeof(int))==-1){
        perror("ERROR\n");
        free(pathname.str);
        return EXIT_FAILURE;
    }
        free(pathname.str);
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");  
        return EXIT_FAILURE;
    }
    File_t *file;
    file=return_File(pathname.str);
    //Prendo la lock come lettore
    printf("Rimani in attesa della lock\n");
    Read_File_lock(file);
    strcpy(file->lastAction, "READ_FILE\0");
    changeDate(file);

    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");      
    //Passare il testo al client
    size_t sizeFile=file->size;
    printf("dimensione file---> %zu\n\n", sizeFile);
    if(writen_response(client, &sizeFile, sizeof(size_t))==-1){
        perror("ERROR\n");
        free(pathname.str);
        return EXIT_FAILURE;
    }
    if(writen_response(client, file->content, file->size*sizeof(char))==-1){
        perror("ERROR\n");
        free(pathname.str);
        return EXIT_FAILURE;
    }

    Read_File_unlock(file);
    free(pathname.str);
    printf("QUI HAI FATTO TUTTO\n");
    return EXIT_SUCCESS;

}

//DIFFERENZA CHE RESTITUISCE OLTRE AL PATHNAME ANCHE IL CONTENUTO DEL FILE
int readFile_Variation(File_t *file, int client){
    int dim;
    int err; 

    printf("Rimani in attesa della lock\n");
    Read_File_lock(file);
    strcpy(file->lastAction, "READ_FILE\0");
    changeDate(file);

    printf("ENTRI DENTRO LA READ DEL FILE\n");
    //SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");      
   
    //Invio al client del pathname del FILE
    int len_pathname=strlen(file->path_name);
    printf("dimensione pathname---> %d\n\n", len_pathname);
    if(writen_response(client, &len_pathname, sizeof(int))==-1){
        Read_File_unlock(file);
        perror("ERROR\n");
        return EXIT_FAILURE;
    }
    if((writen_response(client, file->path_name, len_pathname*sizeof(char)))==-1){
        Read_File_unlock(file);
        perror("ERROR\n");
        return EXIT_FAILURE;
    }
    //Invio al client del contenuto del FILE
    size_t sizeFile=file->size;
    printf("dimensione file---> %zu\n\n", sizeFile);
    if(writen_response(client, &sizeFile, sizeof(size_t))==-1){
        Read_File_unlock(file);
        perror("ERROR\n");
        return EXIT_FAILURE;
    }
    if(writen_response(client, file->content, file->size*sizeof(char))==-1){
        Read_File_unlock(file);
        perror("ERROR\n");
        return EXIT_FAILURE;
    }

    Read_File_unlock(file);
    printf("QUI HAI FATTO TUTTO\n");
    return EXIT_SUCCESS;

}

int readNFiles(int client, int workers){
    int N, dim;
    int err; 

    //---Leggo il numero inviato dal Client
    dim=readn_request(client, &N, sizeof(int));
    CONTROL(dim, "Request failed\n");
    printf("File number: %d\n", N);
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&mutexStorage),"Lock");
    
    //---Controllo che esista almeno un file
    int nrfiles=nrFile_Storage();
    if(nrfiles<=0){
        perror("File number: 0\n");
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");
        return EXIT_FAILURE;
    }
    if(N<=0 || N>nrfiles)
        N=nrfiles;
    
    //Se il valore è negativo o uguale allo 0 -> lettura intero FileStorage
    //char *pathname=malloc(32*sizeof(char));
    int number=-1;
    while(N>0){
        File_t *file;
        file=return_File_Variation(&number);
        printf("    ECCO IL NUMERO %d\n\n", number);
        if(readFile_Variation(file, client)!=0){
            SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");
            perror("ERROR: readFile_Variant");
            return EXIT_FAILURE;
        }
        N--;
    }
   
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");
    return EXIT_SUCCESS;
}

int writen_responseFile(int client, int request){
    //Lettura del pathname
    msg_t pathname, text;
    //---Leggo la dimensione del pathname inviato dal Client
    int dim=readn_request(client, &pathname.len, sizeof(int));
    CONTROL(dim, "Request failed\n");
    //---Leggo il pathname inviato dal Client
    pathname.str=malloc((pathname.len+1)*sizeof(char));
    for(int i=0; i<pathname.len+1; i++){
        pathname.str[i]='\0';
    }
    CONTROL_NULL(pathname.str, "Malloc error\n");
    if(readn_request(client, pathname.str, pathname.len*sizeof(char))==-1){
        free(pathname.str);
        perror("Request read error\n"); 
        return EXIT_ERROR;
    }
    int err; 
    //---Leggo la dimensione del testo inviato dal Client
    dim=readn_request(client, &text.len, sizeof(int));
    if(dim==-1 || dim==0){
        free(pathname.str);
        perror("Request failed\n");
        return EXIT_ERROR;
    }
    //---Leggo il testo inviato dal Client
    text.str=malloc((text.len+1)*sizeof(char));
    for(int i=0; i<text.len+1; i++){
        text.str[i]='\0';
    }
    if(!text.str){
        perror("Malloc error\n");
        free(pathname.str);
        return EXIT_ERROR;
    }
    if(readn_request(client, text.str, text.len*sizeof(char))==-1){
        free(pathname.str);
        free(text.str);
        perror("Request read error\n"); 
        return EXIT_ERROR;
    }
    //1 Controllo: verifico che il file sia stato creato 
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&mutexStorage),"Lock");
    if(find_File(pathname.str)==0){
        free(pathname.str);
        free(text.str);
        perror("File does not exist\n");
        //errno
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");
        return EXIT_ARG_NOT_FOUND;
    } 
    File_t *file;
    file=return_File(pathname.str);
    if(file==NULL){
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");
        perror("ERROR: return_File\n");
        free(pathname.str);
        free(text.str);
        return EXIT_ERROR;
    }
    //2 Controllo: verifico che fd_lock sia di proprietà del client 
    //3 Controllo: verifico che l'ultima operazione effettuata con successo sia una OPEN
    if(file->fd_lock!=client && (strcmp(file->lastAction, "OPEN_FILE"))!=0){
        free(pathname.str);
        free(text.str);
        perror("Wrong order of operations\n");
        return EXIT_ARG_NOT_FOUND;
    }
    //4 Controllo: verifico che la capienza dello storage si rispettata
    int x=0;
    if(text.len>dimMaxBytes){
        x=-1;   //non posso inserire il file perchè troppo grande
    }
    //Quando creo il file, inserisco al suo interno il suo pathname quindi devo sottrarlo alla dimFileStorage in quato verrà sovrascritto
    while(nrFile_Storage()>0 && dimFile_Storage()+(text.len-pathname.len)>dimMaxBytes && x==0){
        if(request==4){
            File_t* f=Head_list();
            int l=strlen(f->path_name);
            //---Invio al Client della dimensione del pathname
            if(writen_response(client, &l, sizeof(int))==-1){
                Write_File_unlock(file);
                perror("ERROR\n");
                free(pathname.str);
                free(text.str);
                return EXIT_FAILURE;
            }
            //---Invio al Client del pathname
            if(writen_response(client, f->path_name, l*sizeof(char))==-1){
                free(pathname.str);
                free(text.str);
                Write_File_unlock(file);
                perror("ERROR\n");
                return EXIT_FAILURE;
            }
            int x=(int)f->size;
            //---Invio al Client della dimensione del FIlE
            if(writen_response(client, &x, sizeof(int))==-1){
                free(pathname.str);
                free(text.str);
                Write_File_unlock(file);
                perror("ERROR\n");
                return EXIT_FAILURE;
            }
            //---Invio al Client del contenuto del FILE
            if(writen_response(client, f->content, f->size*sizeof(char))==-1){
                Write_File_unlock(file);
                perror("ERROR\n");
                free(pathname.str);
                free(text.str);
                return EXIT_FAILURE;
            }    
        }      
        remove_File(NULL);
        dimStorage_Update();
        INSERT_OP(0, "REPLACE_FILE\n", nameFileLog);
    }
    //---Comunicare al Client che ho terminato con i rimpiazzamenti
    if(writen_response(client, &x, sizeof(int))==-1){
        Write_File_unlock(file);
        perror("ERROR\n");
        free(pathname.str);
        free(text.str);
        return EXIT_FAILURE;
    }   
    changeContentFile(text.len, text.str, pathname.str);  
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");
    free(pathname.str);
    free(text.str);
    return EXIT_SUCCESS;
}

int lockFile(int client, int worker){
    int dim, err;
    msg_t pathname;
    
    //---Leggo la dimensione del pathname inviato dal Client
    dim=readn_request(client, &pathname.len, sizeof(int));
    CONTROL(dim, "Request failed\n");

    //---Leggo il pathname inviato dal Client
    pathname.str=malloc((pathname.len+1)*sizeof(char));
    for(int i=0; i<pathname.len+1; i++){
        pathname.str[i]='\0';
    }
    CONTROL_NULL(pathname.str, "Malloc error\n")
    if(!pathname.str){
        perror("Calloc error\n");
        return EXIT_FAILURE;
    }
    if(readn_request(client, pathname.str, pathname.len*sizeof(char))==-1){
        free(pathname.str);
        perror("Request read error\n"); 
        return EXIT_FAILURE;
    }
    printf("pathname--> : %s\n", pathname.str);
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&mutexStorage),"Lock");
    //Verifico che il FILE esista
    if(find_File(pathname.str)==0){
        printf("File %s -->not exists\n", pathname.str);
        free(pathname.str);
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");  
        return EXIT_FAILURE;
    }
   
    File_t *file;
    file=return_File(pathname.str);
    Read_File_lock(file);
    //Verifico che l'owner della lock del FILE è colui che l'ha richiesta
    if(file->fd_lock==client){
        free(pathname.str);
       // Read_File_unlock(file);
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");  
        return EXIT_SUCCESS;
    }
    //Caso in cui sia -1 o preso da un altro client
    if(file->fd_lock!=client){
       // Read_File_unlock(file);
        Write_File_lock(file);
        file->fd_lock=client;
        free(pathname.str);
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");  
        return EXIT_SUCCESS;
    }
    free(pathname.str);
    return EXIT_FAILURE;
}


int unlockFile(int client, int worker){

    int dim, err;
    msg_t pathname;
    printf("QUI ENTRI\n");
    //---Leggo la dimensione del pathname inviato dal Client
    dim=readn_request(client, &pathname.len, sizeof(int));
    CONTROL(dim, "Request failed\n");

    //---Leggo il pathname inviato dal Client
    pathname.str=malloc((pathname.len+1)*sizeof(char));
    for(int i=0; i<pathname.len+1; i++){
        pathname.str[i]='\0';
    }
    CONTROL_NULL(pathname.str, "Malloc error\n");
    if(readn_request(client, pathname.str, pathname.len*sizeof(char))==-1){
        free(pathname.str);
        perror("Request read error\n"); 
        return EXIT_FAILURE;
    }
    printf("pathanme: %s\n", pathname.str);
    printf("ATTENDO LA LOCK DELLO STORAGE\n");
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&mutexStorage),"Lock");
    //Verifico che il FILE esista
    if(find_File(pathname.str)==0){
        printf("File %s -->not exists\n", pathname.str);
        free(pathname.str);
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");  
        return EXIT_FAILURE;
    }
   
    File_t *file;
    file=return_File(pathname.str);
    //Verifico che l'owner della lock del FILE è colui che l'ha richiesta
    if(file->fd_lock!=client){
        printf("Client %d -->has no rights\n", client);
        free(pathname.str);
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");  
        return EXIT_FAILURE;
    }
    //Modifico data e ora dell'ultima modifica
    file->fd_lock=-1;
    changeDate(file);
    Write_File_unlock(file);
    free(pathname.str);
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");  
    return EXIT_SUCCESS;
}


int Remove_Close_File(int client, int op){

    int err;
    //Lettura del pathname
    msg_t pathname;
    //---Leggo la dimensione del pathname inviato dal Client
    int dim=readn_request(client, &pathname.len, sizeof(int));
    CONTROL(dim, "Request failed\n");
    //---Leggo il pathname inviato dal Client
    pathname.str=calloc((pathname.len+1), sizeof(char));
    CONTROL_NULL(pathname.str, "Calloc error\n");
    if(readn_request(client, pathname.str, pathname.len*sizeof(char))==-1){
        perror("Request read error\n"); 
        free(pathname.str);
        return EXIT_ERROR;
    }
    printf("pathanme: %s\n", pathname.str);
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&mutexStorage),"Lock");
    //---Verificare che il FILE esista
    if(find_File(pathname.str)==0){
        perror("ERROR: File does not exists\n");
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");
        free(pathname.str);
        return EXIT_ARG_NOT_FOUND;
    }
    //---Verificare che il client abbia la lock
    File_t *file=return_File(pathname.str);
    if(file->fd_lock!=client){
        perror("ERROR: No rights\n");
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");
        free(pathname.str);
        return EXIT_ARG_NOT_FOUND; //cambiare
    }
    if(op==8){  //CLOSE_FILE
        changeFdLock(file,-1);
        strcpy(file->lastAction, "FILE_CLOSE\0");
        changeDate(file);
        Write_File_unlock(file);
        free(pathname.str);
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");
        return EXIT_SUCCESS;
    }
    if(op==9){  //REMOVE_FILE
        remove_File(pathname.str);
        //Write_File_unlock(file);
        free(pathname.str);
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");
        return EXIT_SUCCESS;
    }
    free(pathname.str);
    return EXIT_FAILURE;
}


void CloseConnection(int client){
    int err;
    //Mi assicuro che le lock prese dal Client vengano liberate
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&mutexStorage),"Lock");
    FreeLock(client);
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");
    printf("Chiusura di tutto avvenuta\n");
}




//Inolto delle richieste alla funzione in grado di gestirle

//Si può levare worker!!
void switchRequest(int idRequest, int client, int worker){
    int res;
    switch(idRequest){
        case(OPEN_FILE):
            res=OpenFile(client, worker);
            printf("RES %d\n\n\n\n", res);
            INSERT_OP(res, "OPEN_FILE\n", nameFileLog);
            writen_response(client, &res, sizeof(int));  
            break;
        //altri casi da inserire
        case(READ_FILE):
            res=readFile(client, worker);
            INSERT_OP(res, "READ_FILE\n", nameFileLog);
            writen_response(client, &res, sizeof(int));
            break;
        case(READN_FILE):
            res=readNFiles(client, worker);
            INSERT_OP(res, "READN_FILE\n", nameFileLog);
            writen_response(client, &res, sizeof(int));
            break;
        case(WRITE_FILE):
            res=writen_responseFile(client, idRequest);
            INSERT_OP(res, "WRITE_FILE\n", nameFileLog);
            writen_response(client, &res, sizeof(int));
            break;
        case(WRITE_FILE_D):
            res=writen_responseFile(client, idRequest);
              INSERT_OP(res, "WRITE_FILE\n", nameFileLog);
            writen_response(client, &res, sizeof(int));
            break;
        case(LOCK_FILE):
            res=lockFile(client, worker);
              INSERT_OP(res, "LOCK_FILE\n", nameFileLog);
            writen_response(client, &res, sizeof(int));
            break;
        case(UNLOCK_FILE):
            res=unlockFile(client, worker);
              INSERT_OP(res, "UNLOCK_FILE\n", nameFileLog);
            writen_response(client, &res, sizeof(int));
            break;
        case(CLOSE_FILE):
            res=Remove_Close_File(client, idRequest);
            INSERT_OP(res, "CLOSE_FILE\n", nameFileLog);
            writen_response(client, &res, sizeof(int));
            break;
        case(REMOVE_FILE):
            res=Remove_Close_File(client, idRequest);
            INSERT_OP(res, "REMOVE_FILE\n", nameFileLog);
            writen_response(client, &res, sizeof(int));
            break;    
        case(CLOSE_CONNECTION):
            CloseConnection(client);
            close(client);
            break;
        default:
            printf("Invalid receipt request\n");
    }   
}

//sigset_t sig_mask;
static void Wait_Signal(int signal){
    if(signal==SIGINT || signal==SIGQUIT)
        typeClose=QUICK_CLOSURE;
    else if(signal==SIGHUP)
        typeClose=SLOW_CLOSURE;
}

void *worker(void* arg){
    int pdf = *((int *)arg);
    int fd_c;
    //TODO: controllo flag chiusura

    printf("Welcome to worker\n");
    while(1){
        //Prelevare il client da servire dalla coda
        fd_c=removelist_t(&coda);
        if(fd_c==-1)
            break;
        printf("Worker %d -> Client: %d\n", pdf, fd_c);
        if(fd_c==-1) break;
        
        //Eseguire richiesta del client
        int request;
        int dim=readn_request(fd_c, &request, sizeof(int));
        if(dim==-1 || dim==0){
            perror("Request failed\n");
            //////////////TODO AGGIORNARE MASSIMOOOOOOO//////////////
            break;
        }
        printf("REQUEST RECEIVED %d\n", request);
       
        switchRequest(request, fd_c, pdf);
        if(request!=CLOSE_CONNECTION)
            insertlist(&coda, fd_c);
       
       
    }
    //chiudere worker
    //close(fd_c);
    return 0;
}


void freeCoda(list_t** coda){
    list_t *corr=*coda;
    list_t *prec=NULL;
    while(corr!=NULL){
        prec=corr;
        close(corr->data);
        corr=corr->next;
        free(prec);
    }
    *coda=NULL;
}


void main(int argc, char *argv[]){
    
    
    char nameSocketFile[32];
    ReadConfigurationFile("Config.txt", &nrThread, &nrMaxFile, &dimMaxBytes, nameSocketFile, nameFileLog);
    if(CreateFileLog(nameFileLog)!=0)   return;
    HelloServer(nrThread, nrMaxFile, dimMaxBytes, nameSocketFile, nameFileLog);
 
    int fd, err;
    int num_fd=0;
    fd_set set;
    fd_set rdset;

    char buf_pipe[4];

    //-----Creazione della PIPE
    int pip[2];
    SYSCALL(pipe(pip), "Create pipe\n");
    
    //-----Creazione THREAD_POOL
    pthread_t * thread_pool=malloc(nrThread*sizeof(pthread_t));
    pthread_t t;
    
    //-----Creazione di nrThread WorkerThread
    for(int i=0; i<nrThread; i++){
        SYSCALL_PTHREAD(err, pthread_create(&t, NULL, worker, (void*)(&pip[1])),"create WorkerThread\n");
        thread_pool[i]=t;
    }

    //-----Gestione dei Segnali
    struct sigaction s;
    sigset_t mask;
    SYSCALL(sigfillset(&mask), "sigfillset");
    SYSCALL(pthread_sigmask(SIG_SETMASK, &mask, NULL), "pthread_sigmask");
    memset(&s, 0, sizeof(s));
    s.sa_handler=Wait_Signal;

    SYSCALL(sigaction(SIGINT, &s, NULL), "sigaction");
    SYSCALL(sigaction(SIGQUIT, &s, NULL), "sigaction");
    SYSCALL(sigaction(SIGHUP, &s, NULL), "sigaction");

    s.sa_handler=SIG_IGN;
    SYSCALL(sigaction(SIGPIPE, &s, NULL), "sigaction");
    SYSCALL(sigemptyset(&mask), "sigemptyset");
    SYSCALL_PTHREAD(err,pthread_sigmask(SIG_SETMASK, &mask, NULL), "pthread_sigmask");
    
    //-----Creazione della connessione client-server
    unlink(nameSocketFile);
    int fd_skt;
    struct sockaddr_un sa;
    strncpy(sa.sun_path, nameSocketFile, 32);
    sa.sun_family=AF_UNIX;


    //TODO: vedere se metterla con SYSCALL
    if ((fd_skt = socket(AF_UNIX,SOCK_STREAM,0)) == -1) {
        perror("ERROR: Socket\n");
        exit(EXIT_FAILURE);
    }
    SYSCALL(bind(fd_skt,(struct sockaddr *)&sa,sizeof(sa)),"Bind");
    SYSCALL(listen(fd_skt,SOMAXCONN),"Listen");

    if(fd_skt>num_fd) 
        num_fd=fd_skt;
    printf("numero fd: %d\n", num_fd);
    if(pip[0]>num_fd)
        num_fd=pip[0];

    FD_ZERO(&set);
    FD_SET(fd_skt, &set);
    FD_SET(pip[0],&set);


    while(1){
        rdset=set;
        if (select(num_fd+1,&rdset,NULL,NULL,NULL) == -1) {
            if(typeClose==QUICK_CLOSURE){
                SYSCALL_PTHREAD(err, pthread_mutex_lock(&lock_coda), "LOCK CODA SBAGLIATO\n");
                freeCoda(&coda);
                SYSCALL_PTHREAD(err, pthread_mutex_unlock(&lock_coda), "UNLOCK SBAGLIATO\n");
                break;
            }
            else if(typeClose==SLOW_CLOSURE) {
                if(nr_client==0)
                    break;
                else{
                    FD_CLR(fd_skt, &set);
                    if(fd_skt==num_fd)
                        num_fd=updatemax(set, num_fd);
                    close(fd_skt);
                    rdset=set;
                    SYSCALL_BREAK(select(num_fd+1, &rdset, NULL, NULL, NULL), "select");
                }
            }else{
                perror("Select");
                break;
            }
        }            
        int fd_c;
        int fd;
        printf ("\nWaiting for Clients....\n");
        for(fd=0; fd<=num_fd; fd++){
            if(FD_ISSET(fd, &rdset)==1){                
                if(fd==fd_skt){
                    fd_c=accept(fd_skt, NULL, 0);
                    //Controllo dei segnali
                    if(typeClose==QUICK_CLOSURE)
                        break;
                    else if(typeClose==SLOW_CLOSURE){
                        if(nr_client==0)
                            break;
                        else
                            perror("accept");
                    }
                    
                    
                    if(fd_c==-1){
                        //TODO: da aggiungere cose poi per il worker+manca roba per chiusura forzata
                        perror("ERROR: Attempt to connect with the client failed\n");
                    }
                    FD_SET(fd_c, &set);
                    if(fd_c>num_fd) num_fd=fd_c;
                    printf("Connection with the client: %d\n", fd_c);
                    continue;
                }
                if(fd==pip[0]){
                    //Client da reinserire nel set
                    int fd_c1;
                    FD_SET(fd_c1, &set);
                    if(fd_c>num_fd) num_fd=fd_c;
                }else{
                    insertlist(&coda, fd);
                    FD_CLR(fd, &set);
                }
           }
        }
    }

    printf("----Closing FileStorageServer-----\n");
    for (int i=0;i<nrThread;i++) {
        insertlist(&coda,-1);
    }
  
    //----Terminazione Workers
    for(int i=0; i<nrThread; i++){
        SYSCALL_PTHREAD(err, pthread_join(thread_pool[i], NULL),"closing WorkerThread\n");
    }    
    
    SYSCALL_PTHREAD(err, pthread_mutex_destroy(&lock_coda), "pthread_mutex_destroy\n");
    SYSCALL_PTHREAD(err, pthread_mutex_destroy(&mutexStorage), "pthread_mutex_destroy\n");
  
    free(thread_pool);
    freeCoda(&coda);
    //free(coda);
    if(close(fd_skt)==-1) perror("close");
    remove(nameSocketFile);
    freeStorage();
        //CAMBIARE POSIZIONE
        //close(fd_c);
    printf("Chiuso il server\n");
    return;
}


