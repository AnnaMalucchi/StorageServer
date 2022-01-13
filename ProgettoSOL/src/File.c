#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include "../include/File.h"

void list_Initialization(){
    root=NULL;
    nrFile=0;
    BytesStorage=0;
    MaxNrFile=0;
    MaxBytesStorage=0;
}

int err=0;

//find_File: verifica la presenza del File "pathname" nel FileStorage. Restituisce 1 se il File viene trovato, altrimenti 0
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

//return_File: cerca il File "pathname" nel FileStorage. Se il File esiste lo restituisce, altrimenti restituisce NULL.
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

//return_File_Variation: restituisce il File successivo all'ultimo visitato, altrimenti NULL.
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

//dimStorage_Update: scorre la lista dei File presenti nel FileStorage incrementando ogni volta la variabile ByteStorage con la dimensione del File. 
//                   Al termine confronta il risultato ottenuto con MaxBytesStorage e se necessario aggiorna il nuovo massimo.
void dimStorage_Update(){
    //Operazione che mi permette di riconteggiare anche il numero di file (utile quando si verificano operazione di remove)
    nrFile=0;
    listFile_t *corr=root;
    BytesStorage=0;
    while(corr!=NULL){
        BytesStorage+=(corr->el).size;
        nrFile++;
        corr=corr->next;
    }
    if(BytesStorage>MaxBytesStorage) 
        MaxBytesStorage=BytesStorage;
    return;
}

//nrFile_Storage: restituisce il numero di File presenti nel FileStorage.
int nrFile_Storage(){
    return nrFile;
}
//nrMaxFile_Storage: restituisce il massimo numero di File memorizzati sul FileStorage. Utilizzato nelle statistiche.
int nrMaxFile_Storage(){
    return MaxNrFile;
}
//dimFile_Storage: restituisce la dimensione complessiva del FileStorage. 
int dimFile_Storage(){
    return BytesStorage;
}
//dimMaxFile_Storage: restituisce la massima dimensione occupata dal FileStorage. Utilizzato nelle statistiche.
int dimMaxFile_Storage(){
    return MaxBytesStorage;
}

//changeContentFile: cambia il contenuto del File "pathname" con il testo passatole come argomento ("content"). 
void changeContentFile(int len, char* content, char* pathname){
    listFile_t *corr=root;
    //Cercare il FILE nella lista
    while(corr!=NULL){
        if ((strcmp((corr->el).path_name, pathname))==0){
            //Controllo che l'ultima operazione e richiesta sia una OpenFile con flag CREATE
            if(strcmp((corr->el).lastAction, "CREATE_FILE\n")){
                //Il file essendo vuoto non ha bisogno di free
                (corr->el).size=len;
                (corr->el).content=calloc(len+1, sizeof(char));
                strcpy((corr->el).content, content);
                strcpy((corr->el).lastAction, "WRITE_FILE\0");
                changeDate(&corr->el);       
                dimStorage_Update();
                return;
            }
        }           
        corr=corr->next;
    }
    return;
}

//appendToFile: inserisce in append al File "pathname" il testo passatole come argomento ("content").
void appendToFile(int len, char* content, char* pathname){
    listFile_t *corr=root;
   
    while(corr!=NULL){
        if ((strcmp((corr->el).path_name, pathname))==0){
            int x=(int)(corr->el).size;
                if(x==0){
                    (corr->el).size=len;
                    (corr->el).content=calloc(len+1, sizeof(char));
                    strcpy((corr->el).content, content);
                    strcpy((corr->el).lastAction, "APPEND_TO_FILE\0");
                    changeDate(&corr->el);       
                    dimStorage_Update();
                    return;
                }else{
                    (corr->el).size=len+(corr->el).size+1;
                    (corr->el).content=realloc((corr->el).content, ((corr->el).size)* sizeof(char));
                    strcat((corr->el).content, content);
                    strcpy((corr->el).lastAction, "APPEND_TO_FILE\0");
                    changeDate(&corr->el);       
                    dimStorage_Update();
                    return;
                }
                
        }
        corr=corr->next;
    }
    return;
}

//printf_File: stampa le informazioni riguardanti al "File" passatele come argomento.
void print_File(File_t file){
    printf("\tFILE: %s\n", file.path_name);
    printf("\tDate: %s" , ctime(&(file.date_lastAction))); 
    printf("\tAuthor: %d\n", file.author);
    printf("\tSize bytes: %ld\n", file.size);
    printf("\tLast Operation: %s\n", file.lastAction);
    printf("\tText: %s\n", file.content);
    printf("---------------------------------------------------------------\n");
}

//print_Storage: Scorre la lista dei File presenti nello Storage e per ognuno di essi invoca print_File. 
//              Risultato finale stampa la lista  e il numero dei file presenti nel FileStorage.
void print_Storage(){
    listFile_t *corr=root;
    printf("----FILE STORAGE:\n");
    while(corr!=NULL){
        print_File(corr->el);
        corr=corr->next;
    }
    printf("----NUMBER OF FILES: %d:\n", nrFile);
    printf("-------------------------------------------------------------\n\n");
}

//print_Storagenotext: stampa il pathname dei file presenti nel FileStorage.
void print_Storagenotext(){
    listFile_t *corr=root;
    printf("\n\n----FILE STORAGE:\n");
    while(corr!=NULL){
        printf("\tFILE: %s\n", (corr->el).path_name);
        corr=corr->next;
    }
   printf("--------------------------\n\n");
}

//Head_list: restituisce il File presente in testa alla lista.
File_t* Head_list(){
    listFile_t* corr=root;
    return &(corr->el);
}

//FreeLock: permette al "client" il rilascio della lock in Lettura. Viene rimosso il "client" dalla lista dei readers e aggiornato fd_lock del File.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   
void FreeLock(int client){
    listFile_t *corr=root;
    while(corr!=NULL){
        //Se il file è lockato in write allora sicuramente nella lista dei readers comparirà il client
        if(((corr->el).fd_lock==client)){
            changeFdLock(&(corr->el), -1);
            eliminateReader(&corr->el, client);
            //----Libero lock  scrittua
            Write_File_unlock(&(corr->el));
        }
        //Caso in cui sia stata acquisita la lock in scrittura. 
        //Con il precedente if mi assicuro che un lettore non può più essere nella lista dei readers
        if(controlReaders(&corr->el, client)==1){
            eliminateReader(&corr->el, client);
            Read_File_unlock(&(corr->el), client);
        }
        corr=corr->next;
    }
}


//remove_File: rimuove il file "pathname" dal FileStorage. Se il "pathname" passatole è nullo elimina il File in testa alla lista. 
//             Prima dell'eliminazione rilascia le lock e dealloca lo spazio occupato dalla lista dei readers chiamando la funzione FreeRead. 
//             Al termine aggiorna il numero dei File memorizzato nel FileStorage e stampa i loro identificativi (print_Storagenotext). 
void remove_File(char *pathname){
    listFile_t *corr=root;
    listFile_t *aux=NULL;
    listFile_t *prec=NULL;
    if(corr==NULL)
        return;    
    aux=corr->next;
    //---Utilizzato nelle politiche di rimpiazzamento si elimina il file non utilizzato di recente
    if(pathname==NULL){
        free((corr->el).content);
        FreeRead(&corr->el);
        Write_File_unlock(&corr->el);
        pthread_mutex_destroy(&((corr->el).writeFile));
        pthread_mutex_destroy((&(corr->el).openFile));
        root=corr->next;
        free(corr);
        nrFile--;
        print_Storagenotext();
        return;
    }
    //---Utilizzato nella Remove_file
    if(strcmp(((corr->el).path_name), pathname)==0){
        root=root->next;
        free((corr->el).content);
        FreeRead(&corr->el);
        Write_File_unlock(&corr->el);
        pthread_mutex_destroy(&((corr->el).writeFile));
        pthread_mutex_destroy((&(corr->el).openFile));
        free(corr);
        nrFile--;
        print_Storagenotext();
        return;
    }

    while(corr!=NULL){
        if(strcmp(((corr->el).path_name), pathname)==0){
            free((corr->el).content);
            FreeRead(&corr->el);
            Write_File_unlock(&corr->el);
            pthread_mutex_destroy(&((corr->el).writeFile));
            pthread_mutex_destroy((&(corr->el).openFile));
            if(aux==NULL){
                prec->next=NULL;
                free(corr);
                nrFile--;
                print_Storagenotext();
                return;
            }        
            prec->next=aux;
            aux=aux->next;  
            free(corr);   
            nrFile--;
            print_Storagenotext();
            return;
        }
        prec=corr;
        corr=aux;
        aux=aux->next;
    }
    dimStorage_Update();
    print_Storagenotext();
    return;
}

//freeStorage: elimina i File presenti nel FileStorage. 
//              Scorre la lista dei File, dealloca lo spazio dedicato al loro contenuto, chiama la funzione FreeRead per eliminare i readers a lui associati.  
//              Al termine dealloca la memoria, aggiorna il numero di File e la dimensione del nuovo FileStorage.
void freeStorage(){
    listFile_t *corr=root;
    listFile_t *prec;
    //Cercare il FILE nella lista
    while(corr!=NULL){
        prec=corr;
        free((prec->el).content);
        FreeRead(&prec->el);
        pthread_mutex_destroy(&((prec->el).writeFile));
        pthread_mutex_destroy((&(prec->el).openFile));
       // pthread_cond_init(&(file.condAccess), NULL);
        corr=corr->next;    
        free(prec);
        nrFile--;
    }
    root=NULL;
    dimStorage_Update();
}

//insertReader: aggiunge il nuovo readers ("client") passatole come argomento alla lista dei readers associati al File "f".
//              Inserimento avviene in testa.
void insertReader(File_t* f, int client){
    listReaders_t *new=malloc(sizeof(listReaders_t));
    if(f->listReaders==NULL){
        new->next=NULL;
        new->reader=client;
    }else{
        new->next=f->listReaders;
        new->reader=client;
    }
    (f->listReaders)=new;
}

//printListReaders: scorre la lista dei readers associati al File "f" stampando il loro identificativo. Se la lista è vuota stampa "Empty list". 
void printListReaders(File_t*f){
    printf("\n\n\nList Readers----\n");
    listReaders_t *tmp=f->listReaders;
    listReaders_t *corr=f->listReaders;
    if(f->listReaders==NULL)    printf("Empty list\n");
    while(corr!=NULL){
        printf("Client in read %d %ls\n", corr->reader, &corr->reader);
        corr=corr->next;
    }
    (f->listReaders)=tmp;
}

//FreeRead: elimina la lista dei readers del File "f", deallocando la memoria.
void FreeRead(File_t* f){
    listReaders_t *corr=f->listReaders;
    if(corr==NULL){
        return;
    }
    while(corr!=NULL){
       f->listReaders=corr->next;
       free(corr);
       corr=f->listReaders;
    }
}

//controlReaders: verifica se nella lista dei readers associati al File "f" sia presente il reader "client" passatole come argomento. 
//                 Se lo individua restituisce 1, altrimenti 0. 
int controlReaders(File_t* f, int client){
    listReaders_t *corr=f->listReaders;
    while(corr!=NULL){
        if(corr->reader==client){
            return 1;
        }
        corr=corr->next;
    }
    return 0;
}


//eliminateReader: elimina, dalla lista dei readers associati al File "f", il reader "client" passatole da parametro.
//                  Prima di effettuare l'eliminazione rilascia la lock di Lettura attraverso la chiamata a Read_File_unlock.
void eliminateReader(File_t* f, int client){
   
    listReaders_t *corr=f->listReaders;
    listReaders_t *prec=NULL;

    if(corr==NULL)
        return;    
    while(corr!=NULL){
        if(corr->reader==client){
            //Read_File_unlock(f, client);
            if(prec==NULL){
                f->listReaders=corr->next;
                free(corr);
                return;
            }        
            prec->next=corr->next;
            free(corr); 
            corr=prec->next;
              
            return;
        }
        prec=corr;
        corr=corr->next;
    }
    return;
}

//changeDate: aggiorna la data e ora del "file" passatole da parametro. Al termina aggiorna l'ordine della lista dei File.
void changeDate(File_t *file){
    file->date_lastAction=time(NULL);
    localtime(&file->date_lastAction);   
    //printf("\tDate: %s" , ctime(&(file->date_lastAction))); 
    listFile_t *start=root;
    root=quicksort(start, NULL);
    //print_Storage();
}

//changeFdLock: sostituisce la fd_lock del File "file" con l'intero "client" passatole da parametro. Al termina aggiorna l'ordine della lista dei File.
void changeFdLock(File_t *file, int client){
    file->fd_lock=client;
    listFile_t *start=root;
    root=quicksort(start, NULL);
}

//insertFile: crea un nuovo File, inizialmente con contenuto e lista readers NULL. 
//          Assegna a fd_lock l'identificativo del client "author" che ha creato il file, il quale acquisisce la lock.
//          Incrementa il numero di File presenti nello Storage, aggiorna l'ordine della lista e procede a stamparla.
void insertFile(int author, char *pathname){
   
    //Creare il FILE
    File_t file;
    file.author=author;
    file.size=0;
    file.content=NULL;
    file.listReaders=NULL;
    strcpy(file.path_name, pathname);
    file.date_lastAction=time(NULL);
    localtime(&file.date_lastAction);   
    //printf("date: %s" , ctime(&(file.date_lastAction))); 
    strcpy(file.lastAction, "CREATE_FILE\0"); 
    
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
    nrFile++;
    //Operazione utile alle statistiche
    if(nrFile>MaxNrFile) MaxNrFile=nrFile;
    //Prendo la lock del file
    Write_File_lock(&file);
    //Inserimento FILE nella lista       
    listFile_t *new=malloc(sizeof(listFile_t));
    new->el=file;
    new->next=NULL;
    if(root==NULL){
        root=new;
        print_Storagenotext();
        return;
    }
    listFile_t *corr=root;
    while(corr->next!=NULL){
        corr=corr->next;
    }
    corr->next=new;
    listFile_t *start=root;
    sleep(1);
    root=quicksort(start, NULL);
    //Stampo soltanto i nomi dei file presenti nello storage
    print_Storagenotext();
}

//quicksort: ordina i File presenti nello Storage. L'ordine si basa sulla data di ultimo accesso, ovvero accesso meno recente al File a quello con accesso più recente.
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
        if((p->next->el).date_lastAction<(pivot->el).date_lastAction){
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

//Read_File_lock: il "client" acquisisce la lock in Lettura del File "file".
void Read_File_lock(File_t* file, int client){
    //Write_File_lock posseduta da colui che vuole possedere anche quella di lettura
    if(file->fd_lock==client) return;
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&(file->openFile)),"Lock Open");
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&(file->writeFile)),"Lock Write");
    //printf("FILE WRITERS %d\n",file->Writer);
    while(file->Writer!=0){
        SYSCALL_PTHREAD(err,pthread_cond_wait(&(file->condAccess), &(file->writeFile)),"CondAccess");
    }
    file->nrReaders++;      
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&(file->openFile)),"UnLock Open");
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&(file->writeFile)),"UnLock Write");
}

//Read_File_unlock: il "client" rilascia la lock in Lettura del File "file".
void Read_File_unlock(File_t* file, int client){  
    //Write_File_lock posseduta da colui che vuole rilasciare anche quella di lettura
    if(file->fd_lock==client) return;
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&(file->openFile)),"Lock Write");
    file->nrReaders--;
    if(file->nrReaders==0)
        SYSCALL_PTHREAD(err,pthread_cond_signal(&(file->condAccess)),"CondAccess");
       
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&(file->openFile)),"UnLock Write");
}

//Write_File_lock: il "client" acquisisce la lock in Scrittura del File "file".
void Write_File_lock(File_t* file){
    //printf("prendo lock\n");
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&(file->openFile)),"Lock Open");
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&(file->writeFile)),"Lock Write");
    //printf("%d e %d", file->Writer, file->nrReaders);
   // printf("%d e %d", file->Writer, file->nrReaders);
    //printf("Attendo lock\n");

    while((file->Writer!=0) || ((int)file->nrReaders>0)){
        SYSCALL_PTHREAD(err,pthread_cond_wait(&(file->condAccess), &(file->writeFile)),"CondAccess");
    }
    //printf("FILE WRITER %d\n\n", file->Writer);
    file->Writer++;
    //printf("FILE WRITER %d\n\n", file->Writer);
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&(file->openFile)),"UnLock Open");
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&(file->writeFile)),"UnLock Write");

}

//Write_File_unlock: il "client" rilascia la lock in Scrittura del File "file".
void Write_File_unlock(File_t* file){
    SYSCALL_PTHREAD(err,pthread_mutex_lock(&(file->writeFile)),"Lock Write");
    file->Writer--;
    SYSCALL_PTHREAD(err,pthread_cond_signal(&(file->condAccess)),"CondAccess");
    SYSCALL_PTHREAD(err,pthread_mutex_unlock(&(file->writeFile)),"UnLock Write");
}



//readn_response; evita Letture Parziali.
//              -1-->errore, 0 errore durante la lettura, size se torna con successo.
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

//writen_response: evita Scritture Parziali.
//                  -1--> errore, 0 errore durante la scrittura la write ritorna 0, 1 la scrittura termina con successo.
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


