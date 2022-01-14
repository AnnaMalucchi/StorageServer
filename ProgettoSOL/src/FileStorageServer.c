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
#include <math.h>
#include <pthread.h>
#include "../include/LogFile.h"
#include "../include/File.h"

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

#define CONTROL(dim, e)\
    if(dim==-1 || dim==0){perror(e); return EXIT_FAILURE;}

#define CONTROL_NULL(string, e)\
    if(!string){perror(e); return EXIT_FAILURE; }

typedef struct node_t {
    int data;
    struct node_t  *next;
} node_t;


typedef struct worker_id{
    int *pip;
    int id;
}worker_id_t;

typedef enum{
    OPEN_FILE,           //0
    READ_FILE,           //1
    READN_FILE,          //2
    WRITE_FILE,          //3
    WRITE_FILE_D,        //4 //caso in cui passo directory
    APPEND_TO_FILE,      //5 
    APPEND_TO_FILED,     //6 //caso in cui passo directory
    LOCK_FILE,           //7
    UNLOCK_FILE,         //8
    CLOSE_FILE,          //9
    REMOVE_FILE,         //10
    CLOSE_CONNECTION,    //11
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
int maxClient=0, ConnectionClient=0, nrFileReplace=0, bytesWrite=0, nrWrite=0, bytesRead=0, nrRead=0; //Variabili per statistiche
char nameFileLog[32];
volatile sig_atomic_t typeClose=OPEN;

node_t *coda = NULL; //CODA DI COMUNICAZIONE MANAGER --> WORKERS / RISORSA CONDIVISA
pthread_mutex_t lock_coda = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutexStorage = PTHREAD_MUTEX_INITIALIZER;

void HelloServer(int nrThread, int nrMaxFile, int dimMaxBytes, char* nameSocketFile, char *nameFileLog){
    printf("---FileStorageServer active---\n");
    printf("\nREADING CONFIGURATION FILE: \n");
    printf("\tNUMBER OF THREADS: %d\n", nrThread);
    printf("\tMAXIMUM NUMBER OF FILES: %d\n", nrMaxFile);
    printf("\tMAXIMUM SIZE IN MBYTES: %d\n", dimMaxBytes);
    printf("\tSOCKET NAME FILE: %s\n", nameSocketFile);
    printf("\tLOG FILE NAME: %s\n", nameFileLog);
    printf("\t---------------------------\n");
    INSERT_OP_MOD("-----OPEN FileStorageServer\n", nameFileLog, NULL);
}


void ReadConfigurationFile(const char* confFile, int *nrThread, int *nrMaxFile, int *dimMaxBytes, char *nameSocketFile, char *nameFileLog){

    struct stat st;

    //------APERTURA DEL FILE "config.txt"------
    int fd=open(confFile, O_RDONLY);

    //Capisco la dimensione del file e alloco un buffer di tale dimensione
    //La stat ritorna 0 in caso di successo, altrimenti -1
    if(stat(confFile, &st)==-1){
        perror("ERROR: Incorrect file reading\n");
        exit(EXIT_FAILURE);
    }

    //Verificare che il file non sia vuoto
    //Se il file è vuoto fornisce al FileStorage una configurazione standard
    if(st.st_size==0 || fd==-1){
        *nrThread=NR_THREAD;
        *nrMaxFile=NR_MAX_FILE;
        *dimMaxBytes=(DIM_MAX*1000000);
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
        if(strcmp(token, "DimMaxMBytes")==0){
            token=strtok(NULL, delim);
            *dimMaxBytes=atoi(token)*pow(10, 6);
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
        token=strtok(NULL, delim);
    }
    free(buffer);
    return;
}

void insertlist (node_t **l, int data) {
    int err;
    //Prendere la lock
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&lock_coda),"Lock");
    node_t *new = malloc (sizeof(node_t));  
    if(new==NULL){
        perror("Error malloc\n");
        return;
    }
    new->data = data;
    new->next=*l;
    //Inserimento in testa
    *l = new;
    //Invio signal
    SYSCALL_PTHREAD(err,pthread_cond_signal(&not_empty),"Signal");
    nr_client++;

    //Rilascio della lock
    pthread_mutex_unlock(&lock_coda);
    //printf("Queue entry -> Client %d\n", data);
}

int removelist_t (node_t ** list) {
    int err;
    //Prendo lock
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&lock_coda),"Lock");
    while (*list==NULL) {
        //printf("Empty queue\n");
        fflush(stdout);
        pthread_cond_wait(&not_empty, &lock_coda);
    }
    int data;
    node_t *curr = *list;
    data=curr->data;
    *list=curr->next;
    free(curr);

    //Rilascio della lock
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&lock_coda),"Lock"); 
    nr_client--;
    return data;
}



//Ritorno l'indice massimo tra i descrittori attivi
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
            //printf("File %s -->already exists\n", pathname.str);
            free(pathname.str);
            SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock"); 
            return EXIT_FAILURE;
        }
        while(nrFile_Storage()==nrMaxFile){
            File_t *f=Head_list();
            INSERT_OP_MOD("REPLACE_FILE ", nameFileLog, f->path_name);
            remove_File(NULL);
            dimStorage_Update();
            nrFileReplace++; //(Statistiche)
            break;
        }
        insertFile(client ,pathname.str);   //-->Operazione nella quale si prende la lock di scrittura
        INSERT_OP_MOD("CREATE FILE ", nameFileLog, pathname.str);
        char value[12];
        sprintf(value, "%d", client);
        INSERT_OP_MOD("WRITE LOCK FILE ", nameFileLog, pathname.str);
        INSERT_OP_MOD("->ClIENT ", nameFileLog, value);

        File_t* file=return_File(pathname.str);
        insertReader(file, client);
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock"); 
    }
    if(flags==0){
        SYSCALL_PTHREAD(err,pthread_mutex_lock(&mutexStorage),"Lock");
        //Verifico che il FILE esista
        if(find_File(pathname.str)==0){
            //printf("File %s -->not exists\n", pathname.str);
            SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");   
            free(pathname.str);
            return EXIT_FAILURE;
        }
        File_t *file;
        file=return_File(pathname.str);
        if(controlReaders(file, client)==1){
            printf("File %s -->already locked (read)\n", pathname.str);
            free(pathname.str);
            SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");  
            return EXIT_SUCCESS;
        } 
        //-->Prende la lock di lettura
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");   
        Read_File_lock(file, client);
        insertReader(file, client);
        char value[12];
        sprintf(value, "%d", client);
        INSERT_OP_MOD("READ LOCK FILE ", nameFileLog, pathname.str);
        INSERT_OP_MOD("OPEN FILE ", nameFileLog, pathname.str);
    }
    free(pathname.str);   
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
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&mutexStorage),"Lock");
    //Verifico che il FILE esista
    if(find_File(pathname.str)==0){
        //printf("File %s -->not exists\n", pathname.str);
        int aux=0;
        if(writen_response(client, &aux, sizeof(int))==-1){
            perror("ERROR\n");
            free(pathname.str);
            SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock"); 
            return EXIT_FAILURE;
        }
        free(pathname.str);
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock"); 
        return EXIT_FAILURE;
    }
    File_t *file;
    file=return_File(pathname.str);
    if(controlReaders(file, client)==0){
        //printf("File %s --> not locked (read)\n", pathname.str);
        if(writen_response(client, 0 , sizeof(size_t))==-1){
            perror("ERROR\n");
            free(pathname.str);
            return EXIT_FAILURE;     
        }
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");       
    }else{
        //Passare il testo al client
        size_t sizeFile=file->size;
        //printf("dimensione file---> %zu\n\n", sizeFile);
        if(writen_response(client, &sizeFile, sizeof(size_t))==-1){
            perror("ERROR\n");
            free(pathname.str);
            return EXIT_FAILURE;
        }
    
        if(sizeFile!=0){
            if(writen_response(client, file->content, file->size*sizeof(char))==-1){
                //Read_File_unlock(file, client);
                perror("ERROR\n");
                free(pathname.str);
                return EXIT_FAILURE;
            }
        }
        changeDate(file);
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock"); 
        INSERT_OP_MOD("READ FILE ", nameFileLog, pathname.str);
        bytesRead=bytesRead+file->size; 
        nrRead++;
    }
    free(pathname.str);
    return EXIT_SUCCESS;
}

int readFile_Variation(File_t *file, int client){
    if(controlReaders(file, client)==0){
        Read_File_lock(file, client);
        INSERT_OP_MOD("READ LOCK FILE ", nameFileLog, file->path_name);
    }  

    //Invio al client del pathname del FILE
    int len_pathname=strlen(file->path_name);
    //printf("dimensione pathname---> %d\n\n", len_pathname);
    if(writen_response(client, &len_pathname, sizeof(int))==-1){
        perror("ERROR\n");
        return EXIT_FAILURE;
    }
    if((writen_response(client, file->path_name, len_pathname*sizeof(char)))==-1){
        perror("ERROR\n");
        return EXIT_FAILURE;
    }

    //Invio al client del contenuto del FILE
    size_t sizeFile=file->size;
    //printf("dimensione file---> %zu\n\n", sizeFile);
    if(writen_response(client, &sizeFile, sizeof(size_t))==-1){
        perror("ERROR\n");
        return EXIT_FAILURE;
    }
    if(writen_response(client, file->content, file->size*sizeof(char))==-1){
        perror("ERROR\n");
        return EXIT_FAILURE;
    }
    INSERT_OP_MOD("READ FILE ", nameFileLog, file->path_name);
    bytesRead=bytesRead+file->size;
    nrRead++;
    Read_File_unlock(file, client);
    INSERT_OP_MOD("READ UNLOCK FILE ", nameFileLog, file->path_name);
    return EXIT_SUCCESS;
   
}

int readNFiles(int client, int workers){
    int N, dim;
    int err; 

    //---Leggo il numero inviato dal Client
    dim=readn_request(client, &N, sizeof(int));
    CONTROL(dim, "Request failed\n");
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&mutexStorage),"Lock");
    
    //---Controllo che esista almeno un file
    int nrfiles=nrFile_Storage();
    if(nrfiles<=0){
        perror("Empty Storage\n");
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");
        return EXIT_FAILURE;
    }
    
    if(N<=0 || N>nrfiles) 
        N=nrfiles;
    
    //Se il valore è negativo o uguale allo 0 -> lettura intero FileStorage
    int number=-1;
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");
    File_t *file;
    while(N>0){
        file=return_File_Variation(&number);
        if(nrFile_Storage()==number){
            printf("FILE null\n");
            break;
        }
        if(file==NULL){
            printf("FILE null\n");
            break;
        }
        //Controllo che il file non sia vuoto
        if(file->size==0)
            continue;
        //Se il file non è vuoto lo invio al client
        if(readFile_Variation(file, client)!=0){
            perror("ERROR: readFile_Variant");
            return EXIT_FAILURE;
        }
        N--;
    }
    //Invio al client di un carattere speciale che indicia la terminazione della lettura
    int specialint=-1;
    if(writen_response(client, &specialint, sizeof(int))==-1){
        perror("ERROR\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int writen_responseFile(int client, int request){
    msg_t pathname, text;
    //Carattere speciale
    int x=0; //0->va tutto bene -1->ho trovato un errore

    char value[12]; //Utilizzato nella scrittura in LogFile

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
    //---Leggo il testo inviato dal Clientnot exist
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
        x=-1;
    }else{
        File_t *file;
        file=return_File(pathname.str);
        if(file==NULL){
            perror("ERROR: return_File\n");
            free(pathname.str);
            free(text.str);
            x=-1;
        }
        //2 Controllo: verifico che fd_lock sia di proprietà del client
        if(file->fd_lock!=client){
            free(pathname.str);
            free(text.str);
            perror("Wrong order of operations\n");
            x=-1;
        }
        //3 Controllo: verifico che la capienza dello storage sia rispettata
        if(text.len>dimMaxBytes){
            x=-1;   //non posso inserire il file perchè più grande della dimensione dello Storage
        }
        //Quando creo il file, inserisco al suo interno il suo pathname quindi devo sottrarlo alla dimFileStorage in quato verrà sovrascritto
    }
    int aux=dimFile_Storage()+text.len;
    printf ("\n[STORAGE] Check the size of the Storage and end eventualy replace file\n");
    printf("\t[STORAGE] Total dim: %d  Max dim: %d\n", aux, dimMaxBytes);
    printf("\t[STORAGE] Nr file: %d e Nr max file: %d\n", nrFile_Storage(), nrMaxFile);

    while((nrFile_Storage()>nrMaxFile || aux>dimMaxBytes) && x==0){
        //-->Controllo ulteriore per evitare che venga tolto il file su cui si vuole scrivere
        File_t *f=Head_list();
        if(strcmp(f->path_name, pathname.str)==0){
            //printf("Il file vale %d e la memoria tot è %d\n", text.len, dimFile_Storage());
            perror("error in replace! The file you want to write is the one chosen by the replacement policy");
            free(pathname.str);
            free(text.str);
            x=-1;
            break;
        }
        //Scelta implementativa: scelgo ti togliere anche i file vuoti, non cambia la dimensiona ma cambia il numero di file presenti nello Storage
        if(request==WRITE_FILE_D || request==APPEND_TO_FILED){   
            int l=strlen(f->path_name);
            
            //---Invio al Client della dimensione del pathname
            if(writen_response(client, &l, sizeof(int))==-1){
                perror("ERROR\n");
                free(pathname.str);
                free(text.str);
                return EXIT_FAILURE;
            }
            //---Invio al Client del pathname
            if(writen_response(client, f->path_name, l*sizeof(char))==-1){
                free(pathname.str);
                free(text.str);
                perror("ERROR\n");
                return EXIT_FAILURE;
            }
            int x=(int)f->size;

            //---Invio al Client della dimensione del FIlE
            if(writen_response(client, &x, sizeof(int))==-1){
                free(pathname.str);
                free(text.str);
                perror("ERROR\n");
                return EXIT_FAILURE;
            }
            if(x!=0){
                //---Invio al Client del contenuto del FILE
                if(writen_response(client, f->content, f->size*sizeof(char))==-1){
                    perror("ERROR\n");
                    free(pathname.str);
                    free(text.str);
                    return EXIT_FAILURE;
                }    

                char value[12];
                sprintf(value, "%d", x);
                INSERT_OP_MOD("NR BYTES SENT TO CLIENT ", nameFileLog, value);
            }
        }      
        INSERT_OP_MOD("REPLACE_FILE ", nameFileLog, f->path_name);
        printf("[STORAGE] \tVictim file-> %s\n", f->path_name);
        remove_File(NULL);
        nrFileReplace++; //(Statistiche)
        dimStorage_Update();
        int cont=dimFile_Storage();
        //printf("Dimensione nuova Storage %d\n", cont);
        aux=text.len+cont;
        //stampa dei valori aggiornati
        printf("\t[STORAGE] Total dim: %d  Max dim: %d\n", aux, dimMaxBytes);
        printf("\t[STORAGE] Nr file: %d e Nr max file: %d\n", nrFile_Storage(), nrMaxFile);
    }
    if(x!=-1){
        if(request==WRITE_FILE_D || request==WRITE_FILE){
            changeContentFile(text.len, text.str, pathname.str);
            INSERT_OP_MOD("WRITE FILE ", nameFileLog, pathname.str);
            bytesWrite=bytesWrite+text.len;
            nrWrite++;
        }else{
            appendToFile(text.len, text.str, pathname.str);
            INSERT_OP_MOD("APPEND TO FILE ", nameFileLog, pathname.str);
            bytesWrite=bytesWrite+text.len;
            nrWrite++;
        }
        sprintf(value, "%d", text.len);
        INSERT_OP_MOD("BYTES WRITTEN ", nameFileLog, value);
        free(pathname.str);
        free(text.str);
        sprintf(value, "%d", dimFile_Storage());
        INSERT_OP_MOD("NR BYTES STORAGE ", nameFileLog, value);
       
    }
    //---Comunicare al Client che ho terminato con i rimpiazzamenti
    if(writen_response(client, &x, sizeof(int))==-1){
        perror("ERROR\n");
        return EXIT_FAILURE;
    }   
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");
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
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&mutexStorage),"Lock");
    //Verifico che il FILE esista
    if(find_File(pathname.str)==0){
        //printf("File %s -->not exists\n", pathname.str);
        free(pathname.str);
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");  
        return EXIT_FAILURE;
    }
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");  
    File_t*file=return_File(pathname.str);
    //Verifico che l'owner della lock del FILE è colui che l'ha richiesta
    if(file->fd_lock==client){
        free(pathname.str);
        return EXIT_SUCCESS;
    }
    //Caso in cui sia -1 o preso da un altro client
    if(file->fd_lock!=client){
        Write_File_lock(file);
        char value[12];
        sprintf(value, "%d", client);
        INSERT_OP_MOD("WRITE LOCK FILE ", nameFileLog, pathname.str);
        INSERT_OP_MOD("->ClIENT ", nameFileLog, value);

        file->fd_lock=client;
        free(pathname.str);
        insertReader(file, client);
        return EXIT_SUCCESS;
    }
    free(pathname.str);
    return EXIT_FAILURE;
}


int unlockFile(int client, int worker){

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
    CONTROL_NULL(pathname.str, "Malloc error\n");
    if(readn_request(client, pathname.str, pathname.len*sizeof(char))==-1){
        free(pathname.str);
        perror("Request read error\n"); 
        return EXIT_FAILURE;
    }
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&mutexStorage),"Lock");
    //Verifico che il FILE esista
    if(find_File(pathname.str)==0){
        //printf("File %s -->not exists\n", pathname.str);
        free(pathname.str);
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");  
        return EXIT_FAILURE;
    }
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");  
    File_t *file;
    file=return_File(pathname.str);
    //Verifico che l'owner della lock del FILE è colui che l'ha richiesta
    if(file->fd_lock!=client){
        //printf("Client %d -->has no rights\n", client);
        free(pathname.str);
        return EXIT_FAILURE;
    }
    //Modifico data e ora dell'ultima modifica
    file->fd_lock=-1;
    Write_File_unlock(file);
   
    char value[12];
    sprintf(value, "%d", client);
    INSERT_OP_MOD("WRITE UNLOCK FILE ", nameFileLog, pathname.str);
    INSERT_OP_MOD("->ClIENT ", nameFileLog, value);
    eliminateReader(file, client);
    free(pathname.str);
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
        return EXIT_FAILURE;
    }
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&mutexStorage),"Lock");
    //---Verificare che il FILE esista
    if(find_File(pathname.str)==0){
        perror("ERROR: File does not exists\n");
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");
        free(pathname.str);
        return EXIT_FAILURE;
    }
    File_t *file=return_File(pathname.str);
    if(op==9){  //CLOSE_FILE
        //---Verificare che il client non abbia la lock di Scrittura
        if(file->fd_lock==client){
            //printf("Error you should do unlock Write\n");
            free(pathname.str);
            return EXIT_FAILURE;
        }
        //---Verificare che il client abbia la lock di Lettura
        if(controlReaders(file, client)==0){
            perror("ERROR: No rights\n");
            free(pathname.str);
            return EXIT_FAILURE;
        }
        strcpy(file->lastAction, "FILE_CLOSE\0");
        INSERT_OP_MOD("CLOSE FILE ", nameFileLog, file->path_name);
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");
        Read_File_unlock(file, client);
        
        char value[12];
        sprintf(value, "%d", client);
        INSERT_OP_MOD("READ UNLOCK FILE ", nameFileLog, pathname.str);
        INSERT_OP_MOD("->ClIENT ", nameFileLog, value);

        eliminateReader(file, client);
        free(pathname.str);
        return EXIT_SUCCESS;
    }
    if(op==10){  //REMOVE_FILE
        //---Verificare che il client abbia la lock di Scrittura
        if(file->fd_lock!=client){
            perror("ERROR: No rights\n");
            free(pathname.str);
            return EXIT_FAILURE; //cambiare
        }
        INSERT_OP_MOD("REMOVE FILE ", nameFileLog, file->path_name);
        INSERT_OP_MOD("WRITE UNLOCK FILE ", nameFileLog, file->path_name);
        remove_File(pathname.str);
        char value[12];        
        sprintf(value, "%d", dimFile_Storage());
        SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");
        INSERT_OP_MOD("NR BYTES STORAGE ", nameFileLog, value);
        free(pathname.str);
        return EXIT_SUCCESS;
    }
    free(pathname.str);
    return EXIT_FAILURE;
}


void CloseConnection(int client){
    int err;
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&mutexStorage),"Lock");  
    //Mi assicuro che le lock prese dal Client vengano liberate
    FreeLock(client);
     SYSCALL_PTHREAD(err,pthread_mutex_unlock(&mutexStorage),"Unlock");  
    printf("Close Client %d\n", client);
}

void switchRequest(int idRequest, int client, int worker){
    int res;
    switch(idRequest){
        case(OPEN_FILE):
            res=OpenFile(client, worker);
            writen_response(client, &res, sizeof(int));  
            break;
        case(READ_FILE):
            res=readFile(client, worker);
            writen_response(client, &res, sizeof(int));
            break;
        case(READN_FILE):
            res=readNFiles(client, worker);
            writen_response(client, &res, sizeof(int));
            break;
        case(WRITE_FILE):
            res=writen_responseFile(client, idRequest);
            writen_response(client, &res, sizeof(int));
            break;
        case(WRITE_FILE_D):
            res=writen_responseFile(client, idRequest);
            writen_response(client, &res, sizeof(int));
            break;
        case(APPEND_TO_FILE):
            res=writen_responseFile(client, idRequest);
            writen_response(client, &res, sizeof(int));
            break;
        case(APPEND_TO_FILED):
            res=writen_responseFile(client, idRequest);
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
            writen_response(client, &res, sizeof(int));
            break;
        case(REMOVE_FILE):
            res=Remove_Close_File(client, idRequest);
            writen_response(client, &res, sizeof(int));
            break;    
        case(CLOSE_CONNECTION):
            CloseConnection(client);
            char value[12];
            sprintf(value, "%d", client);
            INSERT_OP_MOD("CLOSE CONNECTION CLIENT ", nameFileLog, value);
            close(client);
            break;
        default:
            printf("Invalid receipt request\n");
    }   
}

//sigset_t sig_mask;
void Wait_Signal(int signal){
    if(signal==SIGINT || signal==SIGQUIT)
        typeClose=QUICK_CLOSURE;
    else if(signal==SIGHUP)
        typeClose=SLOW_CLOSURE;
}









void *worker(void* arg){
    worker_id_t work= *((worker_id_t *)arg);
    int work_id=work.id;
    int fd_c;
    printf("[STORAGE] Start worker %d\n", work_id);
    while(1){
        //Prelevare il client da servire dalla coda
        fd_c=removelist_t(&coda);
        //printf("cosa rimuovi dalla lista %d \n\n", fd_c);
        if(fd_c==-1){
            printf("[STORAGE] Worker %d remove -1\n", work_id);
            break;
        }
        printf("[STORAGE] Worker %d -> Client: %d\n", work_id, fd_c);

        //Eseguire richiesta del client
        int request;
       
        int dim=readn_request(fd_c, &request, sizeof(int));
        if(dim==-1 || dim==0){
            perror("Request failed\n");
            break;
        }
        printf("[STORAGE] REQUEST RECEIVED %d\n", request);
        char value[12];
        sprintf(value, "%d", work_id);
        INSERT_OP_MOD("REQUEST MADE BY WORKERS: ", nameFileLog, value);
        switchRequest(request, fd_c, work_id);
        if(request!=CLOSE_CONNECTION){
            insertlist(&coda, fd_c);
        }else{
            ConnectionClient--;
            //printf("Numero di client totali -1 %d\n", ConnectionClient);
        }
        fflush(stdout);
    }
    printf("[STORAGE] Close worker %d\n", work_id);
    return 0;
}


void freeCoda(node_t** coda){
    node_t *corr=*coda;
    node_t *prec;
    while(corr!=NULL){
        prec=corr;
        //if(corr->data!=-1)  close(corr->data);
        corr=corr->next;
        free(prec);
    }
    *coda=NULL;
    return;
}


int main(int argc, char *argv[]){

    char nameSocketFile[32];
    ReadConfigurationFile(argv[1], &nrThread, &nrMaxFile, &dimMaxBytes, nameSocketFile, nameFileLog);
    if(CreateFileLog(nameFileLog)!=0)   
        return EXIT_FAILURE;
    char value[12];
    sprintf(value, "%d", nrThread);
    INSERT_OP_MOD("NR WORKERS: ", nameFileLog, value);
    HelloServer(nrThread, nrMaxFile, dimMaxBytes, nameSocketFile, nameFileLog);
 
    int err;
    int num_fd=0;
    fd_set set;
    fd_set rdset;

    //-----Creazione della PIPE
    int pip[2];
    SYSCALL(pipe(pip), "Create pipe\n");
    
    //-----Creazione THREAD_POOL
    pthread_t * thread_pool=NULL;
    thread_pool=malloc(nrThread*sizeof(pthread_t));
    pthread_t t;
    
    
    worker_id_t array[nrThread-1];
    for(int i=0; i<nrThread; i++){
        array[i].pip=&pip[1];
        array[i].id=i;
    }

    //-----Creazione di nrThread WorkerThread
    for(int i=0; i<nrThread; i++){
        SYSCALL_PTHREAD(err, pthread_create(&t, NULL, worker, ((void*)&array[i])),"create WorkerThread\n");
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

   // printf("numero fd_skt %d\n", fd_skt);
    if(fd_skt>num_fd) 
        num_fd=fd_skt;
    if(pip[0]>num_fd)
        num_fd=pip[0];

    FD_ZERO(&set);
    FD_SET(fd_skt, &set);
    FD_SET(pip[0],&set);


    while(1){
        rdset=set;
        if (select(num_fd+1,&rdset,NULL,NULL,NULL) == -1) {
            if(typeClose==QUICK_CLOSURE){
                printf("[STORAGE] Received SIGINT\n");
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
                        if(nr_client==0){
                            break;
                            perror("accept");
                        }
                    }
                    
                    
                    if(fd_c==-1){
                        perror("ERROR: Attempt to connect with the client failed\n");
                    }
                    FD_SET(fd_c, &set);
                    if(fd_c>num_fd) num_fd=fd_c;
                    printf("[STORAGE] Connection with the client: %d\n", fd_c);
                    //Numero connessioni totali
                    ConnectionClient++;
                    //printf("Numero di client totali %d\n", ConnectionClient );
                    if(ConnectionClient>maxClient)
                        maxClient=ConnectionClient;
                    //printf("Numero di client MASSIMI %d\n", maxClient );
                    char value[12];
                    sprintf(value, "%d", fd_c);
                    INSERT_OP_MOD("OPEN CONNECTION CLIENT ", nameFileLog, value);
                    continue;
                }
                if(fd==pip[0]){
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
        insertlist(&coda, -1);
    }

    //----Terminazione Workers
    for(int i=0; i<nrThread; i++){
        //printf("Aspetto terminazione %d thread\n", i);
        SYSCALL_PTHREAD(err, pthread_join(thread_pool[i], NULL),"closing WorkerThread\n");
        printf("\t[STORAGE] Close thread %d \n", i);
    }

    //sleep(15);
  
    SYSCALL_PTHREAD(err, pthread_mutex_destroy(&lock_coda), "pthread_mutex_destroy\n");
    SYSCALL_PTHREAD(err, pthread_mutex_destroy(&mutexStorage), "pthread_mutex_destroy\n");
    //----Free ThreadPoll
    free(thread_pool);
    if(coda!=NULL){
        freeCoda(&coda);
    }
    if(close(fd_skt)==-1) perror("close");
    remove(nameSocketFile);

   // sleep(30);

    //Aggiornamento file di LOG
    sprintf(value, "%d", nrMaxFile_Storage());
    INSERT_OP_MOD("\n\n\n\nStatistics\n", nameFileLog, NULL);
    INSERT_OP_MOD("NUMBER MAX FILES ", nameFileLog, value);
    sprintf(value, "%f", (float)dimMaxFile_Storage()/(float)1000000);
    InsertActionLog_float("DIMENSION MAX MBYTES ", nameFileLog, value);
    sprintf(value, "%d", maxClient);
    INSERT_OP_MOD("MAXIMUM NUMBER OF CLIENTS CONNECTED AT THE SAME TIME ", nameFileLog, value);
    if(nrWrite==0)
        sprintf(value, "%d",0);
    else
        sprintf(value, "%d",bytesWrite/nrWrite);
    INSERT_OP_MOD("AVERAGE OF BYTES WRITE ", nameFileLog, value);
    if(nrRead==0)
        sprintf(value, "%d",0);
    else
        sprintf(value, "%d",bytesRead/nrRead);
    INSERT_OP_MOD("AVERAGE OF BYTES READ ", nameFileLog, value);


    //----Statistiche
    printf("---FileStorageServer statistics---\n");
    printf("\tMaximum number of stored files:  %d\n", nrMaxFile_Storage());
    printf("\tMaximum space occupied in Mbytes: %f\n", dimMaxFile_Storage()/(float)1000000);
    printf("\tMaximum number of clients connected at the same time: %d\n", maxClient);
    printf("\tNumber of file expulsion: %d\n", nrFileReplace);
    printf("List of files stored:\n"), print_Storage();
    //----Free Storage
    freeStorage();
    //----Chiusura server
    INSERT_OP_MOD("-----CLOSE FileStorageServer\n", nameFileLog, NULL);
    printf("----------Close FileStorageServer------------\n");
    
    return EXIT_SUCCESS;
}


