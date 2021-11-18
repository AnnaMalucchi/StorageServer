#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "ServerRequestManagement.h"
#include "File.h"


//VEDERE DOVE METTERLE NEL H o C
//Tutte le operazioni eseguibili sul FileStorage enumerate
typedef enum{
    OPEN_FILE,  //0
    CLOSE_FILE,
    READ_FILE,
    WRITE_FILE,
    //ecc
}operation_t;

typedef struct msg {
    int len;
    char *str;
} msg_t;

pthread_mutex_t mutexStorage = PTHREAD_MUTEX_INITIALIZER;
#define SYSCALL_PTHREAD(e,c,s) \
    if((e=c)!=0) { errno=e;perror(s);fflush(stdout);exit(EXIT_FAILURE); }

//Inizializzo la lista contenete i File
//create_List();

//Inolto delle richieste alla funzione in grado di gestirle
void switchRequest(int idRequest, int client, int worker){
    int res;
    switch(idRequest){
        case(OPEN_FILE):
            res=OpenFile(client, worker);
            writen_response(client, &res, sizeof(int));
            break;
        default:
            printf("Invalid receipt request\n");
    
    }   
}


//Evitare Letture Parziali
//-1-->errore, 0 errore durante la lettura, size se torna con successo
int readn_request(int fd, void *buf, size_t n) {
    size_t left = n;
    int r;
    char *bufptr=(char*)buf;
    while(left>0) {
	if ((r=read((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r==0) return 0; 
        left-=r;
	    bufptr+=r;
    }
    return n;
}

//Evita Scritture Parziali
//-1--> errore, 0 errore durante la scrittura la write ritorna 0, 1 la scrittura termina con successo
int writen_response(int fd, void *buf, size_t n) {
    size_t left = n;
    int r;
    char *bufptr=(char*)buf;
    while(left>0) {
	if ((r=write((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r == 0) return 0;  
        left-=r;
	bufptr+=r;
    }
    return 1;
}



int OpenFile(int client, int worker){
    int flags, dim;
    msg_t pathname;
    
    //---Leggo la dimensione del pathname inviato dal Client
    dim=readn_request(client, &pathname.len, sizeof(int));
    if(dim==-1 || dim==0){
        perror("Request failed\n");
        return EXIT_FAILURE;
    }

    //---Leggo il pathname inviato dal Client
    pathname.str=malloc((pathname.len+1)*sizeof(char));
    for(int i=0; i<pathname.len+1; i++){
        pathname.str[i]='\0';
    }
    if(!pathname.str){
        perror("Calloc error\n");
        return EXIT_FAILURE;
    }
    if(readn_request(client, pathname.str, pathname.len*sizeof(char))==-1){
        perror("Request read error\n"); 
        return EXIT_FAILURE;
    }
    printf("pathanme: %s\n", pathname.str);
    
    //---Lettura dei flags inviato dal Client
    dim=readn_request(client, &flags, sizeof(int));
    printf("dimensione flag: %d\n", dim);
    if((dim==-1 || dim==0)){
        perror("Request failed\n");
        return EXIT_FAILURE;
    }  
    printf("flags: %d\n", flags);
    
    lock(mutexStorage);
    if(flags==1){
        if(find_File(pathname.str)==1){
            printf("File %s -->already exists\n", pathname.str);
            unlock(mutexStorage);
            return EXIT_FAILURE;
        }
        //--- Controlli per la creazione File
        if(nrFile_Storage()>=nrMaxFile){
            //ALLORA DEVO TOGLIERLO
        }//DEVO METTERE ANCGE LA DIMENSIONE
        if(dimFile_Storage()>=DIM_MAX){
            //DA FINIRE
        }

        insertFile(client ,pathname.str, NULL);



    }
    print_Storage();
    unlock(mutexStorage);


    
    
    
    
    return 1;
}