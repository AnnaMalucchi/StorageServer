#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include "File.h"

void list_Initialization(){
    root=NULL;
    //da definire
    nrFile=0;
    BytesStorage=0;
}

//da cambiare
int err=0;

//Restituisco 1 se il file esiste, 0 se il file non esiste nello Storage
int find_File(char *pathname){
    listFile_t *corr=root;
    while(corr!=NULL){
        int aux=strcmp((corr->el).path_name, pathname);
        if (aux==0)
            return 1;
        corr=corr->next;
    }
    return 0;
}

//Restituisco il FILE
File_t* return_File(char *pathname){
    listFile_t *corr=root;
    while(corr!=NULL){
        int aux=strcmp((corr->el).path_name, pathname);
        if (aux==0)
            return &(corr->el);
        corr=corr->next;
    }
    return NULL;
}


//Restituisco il FILE che si trova dopo l'ultimo pathname visitato
File_t* return_File_Variation(int *index){
    listFile_t *corr=root;
    int found;
    //Se non ho visitato ancora nessun file, il pathname sarà NULL --> found=1
    if(*index==-1)
        found=1;
    else    
        found=0;
    int count=0;
    while(corr!=NULL){      
        if(found==1){
            *index=count;
            return &(corr->el);
        } 
        if(*index==count){
            corr=corr->next;
            found=1;
        }
        count++;
    }
    return NULL;
}

//Modifico il numero di bytes dello Storage
void dimStorage_Update(){
    listFile_t *corr=root;
    while(corr!=NULL){
        BytesStorage=+(corr->el).size;
        corr=corr->next;
    }
    return;
}

//Restituisco il numero di file presenti sullo storage
int nrFile_Storage(){
    return nrFile;
}

//Restituisco la dimensione complessiva dei file presenti sullo Storage
int dimFile_Storage(){
    return BytesStorage;
}

void changeContentFile(int len, char* content, char* pathname){
    listFile_t *corr=root;
    //Cercare il FILE nella lista
    while(corr!=NULL){
        if ((strcmp((corr->el).path_name, pathname))==0){
                (corr->el).size=len;
                (corr->el).content=calloc(len+1, sizeof(char));
                strcpy((corr->el).content, content);
                strcpy((corr->el).lastAction, "WRITE_FILE\0");
                changeDate(&corr->el);       
                dimStorage_Update();
                Write_File_unlock(&corr->el);
                return;
        }
        corr=corr->next;
    }
    return;
}
//Stampo le informazioni del File
void print_File(File_t file){
    printf("FILE: %s\n", file.path_name);
    printf("\tDate: %s" , (asctime(gmtime(&file.date_lastAction)))); 
    printf("\tAuthor: %d\n", file.author);
    printf("\tSize: %ld\n", file.size);
    printf("\tLast Operation: %s\n", file.lastAction);
    printf("\t\t\tText: %s\n", file.content);

}

//Stampa la lista dei file presenti nello Storage
void print_Storage(){
    printf("ERRORE NASCE QUI\n");
    listFile_t *corr=root;
    printf("----FILE STORAGE:\n");
    while(corr!=NULL){
        print_File(corr->el);
        corr=corr->next;
    }
    printf("----NUMBER OF FILES: %d:\n", nrFile);
    printf("-----------------------\n");
}

File_t* Head_list(){
    listFile_t* corr=root;
    return &(corr->el);
}

void FreeLock(int client){
    listFile_t *corr=root;
    while(corr!=NULL){
        if((corr->el).fd_lock==client){
            changeFdLock(&(corr->el), -1);
            Write_File_unlock(&(corr->el));
        }
        corr=corr->next;
    }
}

void remove_File(char *pathname){
    listFile_t *corr=root;
    listFile_t *aux=NULL;
    listFile_t *prec=NULL;
    if(corr==NULL)
        return;    
    aux=corr->next;
    //Se il pathname è nullo elimino il primo elemento della lista
    //---Utilizzato nelle politiche di rimpiazzamento si elimina il file non utilizzato di recente
    if(pathname==NULL){
        free((corr->el).content);
        pthread_mutex_destroy(&((corr->el).writeFile));
        pthread_mutex_destroy((&(corr->el).openFile));
        root=corr->next;
        free(corr);
        nrFile--;
        print_Storage();
        return;
    }
    //---Utilizzato nella Remove_file
    if(strcmp(((corr->el).path_name), pathname)==0){
        root=root->next;
        free((corr->el).content);
        pthread_mutex_destroy(&((corr->el).writeFile));
        pthread_mutex_destroy((&(corr->el).openFile));
        free(corr);
        nrFile--;
        print_Storage();
        return;
    }

    while(corr!=NULL){
        if(strcmp(((corr->el).path_name), pathname)==0){
            free((corr->el).content);
            pthread_mutex_destroy(&((corr->el).writeFile));
            pthread_mutex_destroy((&(corr->el).openFile));
            if(aux==NULL){
                prec->next=NULL;
                free(corr);
                nrFile--;
                print_Storage();
                return;
            }        
            prec->next=aux;
            aux=aux->next;  
            free(corr);   
            return;
        }
        prec=corr;
        corr=aux;
        aux=aux->next;
    }
    print_Storage();
    return;
}

void freeStorage(){
    listFile_t *corr=root;
    listFile_t *prec;
    //Cercare il FILE nella lista
    while(corr!=NULL){
        prec=corr;
        free((prec->el).content);
        pthread_mutex_destroy(&((prec->el).writeFile));
        pthread_mutex_destroy((&(prec->el).openFile));
       // pthread_cond_init(&(file.condAccess), NULL);
        corr=corr->next;    
        free(prec);
        nrFile--;
    }
    root=NULL;
}

void changeDate(File_t *file){
    time_t date;
    date=time(NULL);
    file->date_lastAction=date;

    listFile_t *start=root;
    root=quicksort(start, NULL);
    print_Storage();
}

void changeFdLock(File_t *file, int client){
    file->fd_lock=client;
    listFile_t *start=root;
    root=quicksort(start, NULL);
}

void insertFile(int author, char *pathname, char* text, int len_text){
   
    //Creare il FILE
    File_t file;
    file.author=author;
    file.size=len_text;
    file.content=malloc(len_text+1*sizeof(char));
    for(int i=0; i<len_text+1; i++){
        file.content[i]='\0';
    }
    strcpy(file.content, text);
    //strcpy(file.content, text);

    strcpy(file.path_name, pathname);
    time_t date;
    date=time(NULL);
    file.date_lastAction=date;
    strcpy(file.lastAction, "OPEN_FILE\0"); 
    
    //Gestione concorrenza
    //->fd_lock differente da author per il fattore che la lock può essere presa da ulteriori client
    //->mentre l'author rimane invariato nel tempo
    file.fd_lock=author;   
    file.Writer=0;
    file.nrReaders=0;
    pthread_mutex_init(&(file.writeFile), NULL);
    pthread_mutex_init(&(file.openFile), NULL);
    pthread_cond_init(&(file.condAccess), NULL);
    //Incremento i contatori
    nrFile++, BytesStorage+=len_text;
    //Prendo la lock del file
    Write_File_lock(&file);
    //Inserimento FILE nella lista   
    listFile_t *new=malloc(sizeof(listFile_t));
    new->el=file;
    new->next=NULL;
    if(root==NULL){
        root=new;
        print_Storage();
        return;
    }
    listFile_t *corr=root;
    while(corr->next!=NULL){
        corr=corr->next;
    }
    corr->next=new;
    listFile_t *start=root;
    root=quicksort(start, NULL);
    print_Storage();
}

listFile_t *quicksort(listFile_t *start, listFile_t *end){
    listFile_t *pivot;
    if(start==end)
        return start;
    pivot=start;
    start=split(start,end);
    start=quicksort(start, pivot);
    pivot->next=quicksort(pivot->next, end);
    return start;
    
}

listFile_t *split(listFile_t *start, listFile_t *end){
    listFile_t *p, *tempstart, *tempp, *pivot;
    if(start==NULL)
        return NULL;
    pivot=start;
    p=start;
    while(p->next!=end){
        if((p->next->el).date_lastAction>(pivot->el).date_lastAction){
            tempp=p->next;
            tempstart=start;
            p->next=tempp->next;
            start=tempp;
            start->next=tempstart;
        }else
            p=p->next;
    }
    return start;
}

//GESTIONE DEGLI ACCESSI CONCORRENTI
void Read_File_lock(File_t* file){

    SYSCALL_PTHREAD(err,pthread_mutex_lock(&(file->openFile)),"Lock Open");
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&(file->writeFile)),"Lock Write");
    while(file->Writer!=0){
        SYSCALL_PTHREAD(err,pthread_cond_wait(&(file->condAccess), &(file->writeFile)),"CondAccess");
    }
    file->nrReaders++;
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&(file->openFile)),"UnLock Open");
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&(file->writeFile)),"UnLock Write");
}

void Read_File_unlock(File_t* file){ 
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&(file->writeFile)),"Lock Write");
    file->nrReaders--;
    if(file->nrReaders==0)
        SYSCALL_PTHREAD(err,pthread_cond_signal(&(file->condAccess)),"CondAccess");
       
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&(file->writeFile)),"UnLock Write");
}

void Write_File_lock(File_t* file){
    
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&(file->openFile)),"Lock Open");
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&(file->writeFile)),"Lock Write");

    while(file->Writer!=0 || file->nrReaders > 0){
        SYSCALL_PTHREAD(err,pthread_cond_wait(&(file->condAccess), &(file->writeFile)),"CondAccess");
    }

    file->Writer++;
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&(file->openFile)),"UnLock Open");
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&(file->writeFile)),"UnLock Write");

}

void Write_File_unlock(File_t* file){
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&(file->writeFile)),"Lock Write");
    file->Writer--;
    SYSCALL_PTHREAD(err,pthread_cond_signal(&(file->condAccess)),"CondAccess");
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&(file->writeFile)),"UnLock Write");
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
    //printf("CORRETTA1\n");
	if (r == 0) return 0;  
    left-=r;
	bufptr+=r;
    }
    //printf("Corretta 2\n");
    return 1;
}


