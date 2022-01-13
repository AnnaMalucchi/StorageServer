#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <bits/errno.h>
#include <unistd.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <dirent.h>
#include "../include/FileStorageAPI.h"
#include <time.h>
#define SOCKET_NAME "./sockname"
#define MAX 100

typedef struct node{
    char* option;
    char* arg;
    struct node* next;
}node_t;

//Variabili Globali che utilizzero per la configurazione del Client. Inizialmente valori di DEFAULT
int config_stdout=0;
float config_time=0;
char* config_D=NULL;
char* config_d=NULL;
//Variabile globale utilizzata nell'apertura della directory
int cont=0;
static int quiet=0;

#ifndef errno
extern int errno;
#endif

//isNumber: verifica che il carattere passatole come argomento sia un numero. 
//          Se il carattere corrisponde a un numero lo restituisce come intero, altrimenti resiruisce -1.
long isNumber(const char* s) {
    char* e = NULL;
    long val = strtol(s, &e, 0);
  
   if (e != NULL && *e == (char)0) return val; 
   return -1;
}

//insertBack: inserisce un nuovo node_t in fondo alla lista "list" 
void insertBack (node_t ** list,char * cmd, char * arg) {
    node_t* new=malloc(sizeof(node_t));
    if (new==NULL) {
        errno=ENOMEM;
        perror("Error in malloc");
        exit(EXIT_FAILURE);
    }
    //--Inserimento nel nodo del comando
    new->option = malloc(sizeof(cmd));
    if (new->option==NULL) {
        errno=EINVAL;
        perror("Error in malloc"); exit(EXIT_FAILURE);
    }
    strcpy(new->option,cmd);
    //--Inserimento nel nodo degli argomenti
    if (arg!=NULL) {
        new->arg = malloc(PATH_MAX*sizeof(char));
        if (new->arg==NULL) {
            errno=ENOMEM;
            perror("Error in malloc"); exit(EXIT_FAILURE);
        }
        strcpy(new->arg,arg);
    }else 
        new->arg = NULL;
    //--Inseriemento del nodo nella coda
    new->next = NULL;
    node_t* corr = *list;
    if (*list == NULL) {
        *list = new;
        return;
    }
    while (corr->next!=NULL) {
        corr=corr->next;
    }
    corr->next = new;
    return;
}

//ControlDir: permette di verificare se -d viene usata congiuntamente a -r o -R e -D insieme a -w o -W
//             Se trova come opzioni "x" o "y" restituisce 1, altrimenti 0.
int ControlDir(node_t* l, char* x, char* y) {
    node_t* curr = l;
    node_t* prec = NULL;
    int find= 0;
    while (curr!=NULL && find==0) {
        if(strcmp(curr->option,x)==0 || strcmp(curr->option, y)==0){
            find=1;
        }else{
            prec = curr;
            curr = curr->next;
        }
    }
    prec=prec;
    return find;
}

//ContainOp: permette di verificare se la lista "l" contiene un nodo con opzione "cmd".
//           Se durante la lettura della lista trova il comando desiderato restituisce 1, altrimenti 0.
//           Se viene trovato restituisce l'argomento associatogli e procede a eliminare il rispettivo nodo.   
int ContainOp (node_t ** l, char * cmd, char ** arg) {
    node_t* curr = *l;
    node_t* prec = NULL;
    int find= 0;
    while (curr!=NULL && find==0) {
        if(strcmp(curr->option,cmd)==0) 
            find=1;
        else{
            prec = curr;
            curr = curr->next;
        }
    }
    if(find==1){
        if (curr->arg!=NULL) {
            *arg=calloc(strlen(curr->arg)+1,sizeof(char));
            strcpy(*arg, curr->arg);
        }else 
            arg=NULL;

        if (prec == NULL) {
            node_t *tmp = *l;
            *l=(*l)->next;
            free(tmp->arg);
            free(tmp->option);
            free(tmp);
        }else{
            prec->next = curr->next;
            free(curr->arg);
            free(curr->option);
            free(curr);
        }
    }
    return find;
}

//freeOperation: elimina la lista delle Operazioni e dealloca la memoria da lei occupata
void freeOperation (node_t** list) {
    node_t* tmp;
    while (*list!=NULL) {
        tmp = *list;
        free((*list)->arg);
        free((*list)->option);
        (*list)=(*list)->next;
        free(tmp);
    }
}

//openDirectory: scorre la "directory" e trova "n". Per ogni File che trova procede a fare la writeFile()
void openDirectory( char *directory, int n){
    //---Cerco ricorsivamente all'interno delle cartelle il FILE
    DIR * dir;
    if((dir=opendir(directory))== NULL || cont==n){
        if(!quiet){
            errno=EINVAL;
            perror("ERROR: Open directory\n");
        }
        closedir(dir);
        return;
    }
    struct dirent *file;
    while((file =readdir(dir))!= NULL && cont<n){
        char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s/%s", directory, file->d_name); 
        struct stat statbuf;
        if (stat(path, &statbuf)==-1) {
            if (!quiet) {
                perror("ERROR: Incorrect stat\n");
            }
            return;
        }
        if(S_ISDIR(statbuf.st_mode)) {
	        if (strcmp(file->d_name,".")==0 || strcmp(file->d_name,"..")==0) 
                continue;
	        openDirectory (file->d_name, n);
        }else{

            char * resolvedPath = NULL;
            resolvedPath = realpath(path,resolvedPath);

            //printf("Il file è %s\n", resolvedPath);
            cont++;

            if(openFile(resolvedPath,1)==-1) {   //Creo il file
                //Il file potrebbe essere essere stato creato quindi chiamo la lockFile
                if(lockFile(resolvedPath)==-1){
                    if(config_stdout==1)    printf("ERROR: lock file %s failed. Try again!\n", resolvedPath);
                        closedir(dir);
                        free(resolvedPath);
                        return;
                }else{
                    if(config_stdout==1)    printf("File %s locked\n", resolvedPath);
                }
            }
            if(config_stdout==1)    printf("File %s opened successfully\n", resolvedPath);
            if(writeFile(resolvedPath, config_D)==-1){
                if(config_stdout==1)    printf("ERROR: File %s writing failed\n", resolvedPath);
            }else{
                if(config_stdout==1)    printf("File  %s writing successfully\n", resolvedPath);
            }
            if(unlockFile(resolvedPath)==-1){
                if(config_stdout==1)    printf("ERROR: unlock file %s failed. Try again!\n", resolvedPath);
            }else{
                if(config_stdout==1)    printf("File %s unlocked\n", resolvedPath);
            }
            sleep(config_time);
            free(resolvedPath);
        } 
    }
    closedir(dir);
    return;
}

void helper(){
    printf("Welcome to the client! Not sure how to make your requests to the server? \nRead the list of accepted options written below\n");
    printf("\t-h \t\t\tPrint helper.\n");
    printf("\t-f <sock> \t\tSets socket name to <sock>. One-time operation!\n");
    printf("\t-w <dir>[,<num>] \tSends the content of the directory <dir> (and its subdirectories) to the server. If <num> is specified, at most <num> files will be sent to the server.\n");
    printf("\t-W <file>,[<files>]\tSends the files passed as arguments to the server. Attention comma!\n");
    printf("\t-d <dir> \t\tWrites into directory <dir> the files read from server. Otherwise the files will be lost.\n");
    printf("\t-D <dir>\t\tWrites into directory <dir> all the files expelled. First you must have typed -w or -W\n");
    printf("\t-r <file>[,<files>]\tReads the files specified in the argument l from the server. Attention comma!\n");
    printf("\t-R[<num>] \t\tReads <num> files from the server. If you don't enter <num> it will read them all. Attention space bewteen -R and <num>!\n");
    printf("\t-t <time> \t\tSets the waiting time (in milliseconds) between requests. Default is 0.\n");
    printf("\t-l <file>[,<files>]\tList of filenames on which to acquire the lock\n");
    printf("\t-u <file>[,<files>] \tUnlocks all the files given in the file l.\n");
    printf("\t-c <file>[,<files>] \tList of files to delete from the l if they exist\n");
    printf("\t-p \t\t\tEnable standard output printouts\n");
    printf("\n");
}

int main(int argc, char* argv[]){

    printf("---Client active---\n\n"); 
    char operation;
    //Variabili necessarie al controllo che una data operazione sis stata richiesta un unica volta
    int hcheck=0;
    int fcheck=0;
    int pcheck=0;
    int dcheck=0;
    int Dcheck=0;
    //se si verifica un errore in un comando digitato non si prosegue la lettura
    int control=0;
    char *socketname=NULL;
    node_t* lOperation=NULL;

    //-----Parsing dei comandi
    while( ((operation = getopt(argc, argv, ":hf:t:pw:W:D:r:R::d:l:u:c:")) != -1 ) && control==0){
        switch(operation){
            case 'h':{
                if(hcheck==0){
                    hcheck=1;
                    insertBack(&lOperation, "h", NULL);
                }
                break;
            }
            case 'f':{
                if(fcheck==0){
                    //Operazioni che posso eseguire una sola volta
                    fcheck=1;
                    socketname=optarg;
                    insertBack(&lOperation, "f", optarg);
                }
                break;
            }
            case 'w':{
                insertBack(&lOperation, "w", optarg);
                break;
            }
            case 'W':{
                insertBack(&lOperation, "W", optarg);
                break;
            }
            case 'D':{
                Dcheck=1;
                insertBack(&lOperation, "D", optarg);
                break;
            }
            case 'r':{
                insertBack(&lOperation, "r", optarg);
                break;
            }
            case 'R':{
                insertBack(&lOperation, "R", optarg);
                break;
            }
            case 'd':{
                dcheck=1;
                insertBack(&lOperation, "d", optarg);
                break;
            }
            case 't':{
                insertBack(&lOperation, "t", optarg);
                break;
            }
            case 'l':{
                insertBack(&lOperation, "l", optarg);
                break;               
            }
            case 'u':{
                insertBack(&lOperation, "u", optarg);
                break;    
            }
            case 'c':{
                insertBack(&lOperation, "c", optarg);
                break;    
            }
            case 'p':{
                if(pcheck==0){
                    config_stdout=1;
                    pcheck=1;
                }
                break;
            }
            case ':':{
                fprintf(stdout, "Error in parsing options: option -%c requires an argument.\n", optopt);
                break;
            }
            case '?':{
                fprintf(stdout, "Error in parsing options: option -%c is not recognized.\n", optopt);
                break;
            }
        }
    }

    char*aux=NULL;
    //-----Controllare i valori precedentemente inseriti nella lOperation
    if(ContainOp(&lOperation, "h", NULL)==1){
        helper();
        freeOperation(&lOperation);
    }
    if(ContainOp(&lOperation, "f", &aux)==1){
        struct timespec abstime;
        abstime.tv_sec=8;
        int msec=3000;
        if(fcheck==1){
            if(openConnection(socketname, msec, abstime)==-1){
                if(config_stdout==1) 
                    printf("ERROR: Failed to connect.  Unable to communicate with the server\n");
            }else{
                if(config_stdout==1)
                    printf("Connected with FileStorageServer\n");
        
            }
        }
        free(aux);
    }
    if(ContainOp(&lOperation, "d", &aux)==1){       
        //Verifico che -d sia usato congiuntamente a -r o -R
        if(ControlDir(lOperation, "r", "R")==0){
            if(config_stdout==1)
                printf("ERROR: Request <d> not accepted. Try again with -r/-R\n");
        }else{
            //Nello stesso comando possono esserci più flag -d. Quando se ne incontra uno nuovo si sovrascrivono
            if(dcheck==1){
                free(config_d);
            }
            config_d=malloc(strlen(aux)+1*sizeof(char));
            strcpy(config_d, aux);
            if(config_stdout==1)
                printf("Request <d> accepted\n");
        }
        free(aux);
    }
    if(ContainOp(&lOperation, "D", &aux)==1){       
        //Verifico che -D sia usato congiuntamente a -w o-W
        if(ControlDir(lOperation, "w", "W")==0){
            if(config_stdout==1)
                printf("ERROR: Request <D> not accepted. Try again with -w/-W\n");
        }else{
            //Nello stesso comando possono esserci più flag -D. Quando se ne incontra uno nuovo si sovrascrivono
            if(Dcheck==1){
                free(config_D);
            }
            config_D=malloc(strlen(aux)+1*sizeof(char));
            strcpy(config_D, aux);
            if(config_stdout==1)
                printf("Request <D> accepted\n");
        }
        free(aux);
    }
    
    node_t *corr=lOperation;
    while(corr!=NULL){
        //printf("L'opzione che stiamo considerando %s\n\n", corr->option);
        //printf("L'opzione ha argomento %s\n\n", corr->arg);
        sleep(config_time); //tempo di attesa tra un operazione e l'altra. default=0
        //->Gestione w
        if(strcmp(corr->option, "w")==0){
            char* string=NULL;
            char* token=strtok_r(corr->arg, ",", &string);
            char* namedir=token;
            int n;
            //Verifico che sia una directory
            struct  stat directory;
            //stat() mostra le informazioni della directory
            if(stat(namedir, &directory)==-1){
                if(config_stdout==1)
                    printf("Error-> Try again with another directory!\n");
            }else{
                //Cerco il numero di file da scrivere
                if(S_ISDIR(directory.st_mode)){
                  //  printf("Ecco il token %s\n", string);
                    if(string!=NULL){
                        n=isNumber(string);
                        if(n==-1 || n==0)
                            n=MAX;
                    }
                    openDirectory(namedir, n);
                    cont=0;
                }else
                    if(config_stdout==1)    printf("Try again with another directory\n");
            }
        }
        //->Gestione W
        if(strcmp(corr->option, "W")==0){
            char* t= NULL;
            char* token=strtok_r(corr->arg,",",&t);
    
            while(token){       
                char * resolvedPath = NULL;
                resolvedPath = realpath(token,resolvedPath);
                //Il controllo che il pathname da inviare sia realmente un FILE viene fatto in writeFile
                if(openFile(resolvedPath,1)==-1) {   //Creo il file
                    //Il file potrebbe essere già stato creato quindi chiamo la lockFile
                    if(lockFile(resolvedPath)==-1){
                        if(config_stdout==1)    printf("ERROR: lock file %s failed. Try again!\n", resolvedPath);
                    }else{
                        if(config_stdout==1)    printf("File %s locked\n", resolvedPath);
                        }
                }
                if(config_stdout==1)    printf("File %s opened successfully\n", resolvedPath);
                if(writeFile(resolvedPath, config_D)==-1){
                    if(config_stdout==1)    printf("ERROR: File %s writing failed. Try to append\n", resolvedPath);
                    //Potrebbe fallire perchè non è stato superato il controllo dell'ultima operazione "openFile"
                    //A quel punto se il file contiene del materiale, le scritture vengono fatte in append
                    //Apertura del file
                    FILE *f=fopen(token, "r");
                    if(f!=NULL){
                        //---Lettura dimensione FILE
                        fseek(f, 0, SEEK_END);
                        int dimText=ftell(f);
                        dimText=dimText/sizeof(char);
                        if(dimText==0){
                            printf("ERROR: File %s is empty. Try again with another file!", resolvedPath);
                            return EXIT_FAILURE;
                        }
                        char *text=malloc(dimText+1*sizeof(char));
                        if(text==NULL){
                            perror("ERROR: buffer\n");
                            return EXIT_FAILURE;
                        }
                        for(int i=0; i<dimText+1; i++)
                            text[i]='\0';
                        fseek(f, 0, SEEK_SET);
                        fread(text, sizeof(char), dimText, f);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   
                        fclose(f);
                        if(appendToFile(resolvedPath, (void*)text,dimText ,config_D)==-1){
                            if(config_stdout==1)    printf("ERROR: append failed. Try again!\n");
                        }else{
                            if(config_stdout==1)    printf("Append to File %s successfully\n", resolvedPath);
                        }
                        free(text);
                    }
                }else{
                    if(config_stdout==1)    printf("File %s writing successfully\n", resolvedPath);
                }
                if(unlockFile(resolvedPath)==-1){
                    if(config_stdout==1)    printf("ERROR: unlock file %s failed. Try again!\n", resolvedPath);
                }else{
                    if(config_stdout==1)    printf("File %s unlocked\n", resolvedPath);
                }
                sleep(config_time);
                free(resolvedPath);
                token=strtok_r(NULL,",",&t);
            }
        }
        //->Gestione r
        if(strcmp(corr->option, "r")==0){
            char* t= NULL;
            char* token=strtok_r(corr->arg,",",&t);
            while(token!=NULL){
                char * resolvedPath = NULL;
                resolvedPath = realpath(token,resolvedPath);
                char *file=token;
                //Apertura del File in lettura 
                if(openFile(resolvedPath, 0)==-1){
                    if(config_stdout==1)    printf("ERROR: File opening %s failed. Try again!\n", resolvedPath);
                }else{
                    if(config_stdout==1)    printf("File opened %s successfully\n", resolvedPath);
                    char* buf=(char*)malloc(1024*sizeof(char));
                    for(int i=0; i<1024; i++)
                        buf[i]='\0';
                    size_t x;
                    
                    if (readFile(resolvedPath, (void**)(&buf), &x)==-1) { 
                       if(config_stdout==1)     
                            printf("ERROR: File %s reading failed. Try again!\n", resolvedPath);
                    }else{
                        if(config_stdout==1)    
                            printf("File %s reading successfully\n", resolvedPath);
                        //Se il file è stato letto correttamente viene salvato nella cartella d (se specificata tramite flag -d)
                        if(config_d!=NULL) {
                            int tmp=(long unsigned int)x;
                            EnterDataFolder(config_d, file, buf, tmp);
                        } 
                    } 
                    if(buf!=NULL)   
                            printf("ReadFile->Text of the file %s:\n\t%s\n", file, buf);
                    buf=buf;
                    free(buf);
                }
                if(closeFile(resolvedPath)==-1){
                    if(config_stdout==1)    printf("ERROR: File %s close failed\n", resolvedPath); 
                }else{
                   if(config_stdout==1)    printf("File closed %s successful\n", resolvedPath);  
                }
                sleep(10);
                token=strtok_r(NULL,",",&t);
                free(resolvedPath);
            }
        }
        //->Gestione R
        if(strcmp(corr->option, "R")==0){
            int x;
            if(corr->arg==NULL)
                x=-1;
            else
                x=isNumber(corr->arg);
            if(x==-1 || x==0)
                x=MAX;
            //non inserito il nome della directory in cui scrivere i file letti 
            if(dcheck==0)   
                printf("ERROR: Operation <-R> wants a directory. Try again!\n");
            else{
                if(readNFiles(x, config_d)==-1){
                    if(config_stdout==1)    printf("ERROR: Files reading failed. Try again!\n");
                }else{
                    if(config_stdout==1)    printf("Files reading successfully\n");
                }
            }
        }
        //->Gestione t
        if(strcmp(corr->option, "t")==0){
            int x=isNumber(corr->arg);
                if(x==-1){
                if(config_stdout==1)
                    printf("ERROR: Operation <-t> wants a number. Try again!\n");
            }else{
                //Convertire ma millisecondi a secondi
                config_time=(float)x/1000;
                if(config_stdout==1)    
                    printf("Request <t> accepted\n");
            }
        }
        //->Gestione l
        if(strcmp(corr->option, "l")==0){
            char* t= NULL;
            char* token=strtok_r(corr->arg,",",&t);
           
            while(token){
                char * resolvedPath = NULL;
                resolvedPath = realpath(token,resolvedPath);
                if(lockFile(resolvedPath)==-1){
                    if(config_stdout==1)    printf("ERROR: lock file %s failed. Try again!\n", resolvedPath);
                }else{
                    if(config_stdout==1)    printf("File locked %s\n", resolvedPath);
                }
                token=strtok_r(NULL,",",&t);
                free(resolvedPath);
            }
        }
        //->Gestione u
        if(strcmp(corr->option, "u")==0){
            char* t= NULL;
            char* token=strtok_r(corr->arg,",",&t);
           
            while(token){
                char * resolvedPath = NULL;
                resolvedPath = realpath(token,resolvedPath);
                if(unlockFile(resolvedPath)==-1){
                    if(config_stdout==1)    printf("ERROR: File unlocked  %s failed. Try again!\n", resolvedPath);
                }else{
                    if(config_stdout==1)    printf("File unlocked  %s\n", resolvedPath);
 
                }
                token=strtok_r(NULL,",",&t);
                free(resolvedPath);
            }
        }
        //->Gestione c
        if(strcmp(corr->option, "c")==0){
            char* t= NULL;
            char* token=strtok_r(corr->arg,",",&t);
            while(token){
                char * resolvedPath = NULL;
                resolvedPath = realpath(token,resolvedPath);
                if(lockFile(resolvedPath)==-1){
                    if(config_stdout==1){
                        printf("ERROR: lock file %s failed. Try again!\n", resolvedPath);
                        printf("ERROR: File %s remove failed\n", resolvedPath);
                    }
                }else{
                    if(config_stdout==1)    printf("File %s locked\n", resolvedPath);
                    if(removeFile(resolvedPath)==-1){
                        if(config_stdout==1)    printf("ERROR: File %s remove failed\n", resolvedPath);
                    }else{
                        if(config_stdout==1)    printf("File %s remove successful\n", resolvedPath);
                    }
                }
                token=strtok_r(NULL,",",&t);
                free(resolvedPath);
            }

        }
        corr = corr->next;
    }
    if(dcheck==1) 
        free(config_d);
    if(Dcheck==1)
        free(config_D);
    freeOperation(&lOperation);
    if(socketname!=NULL && hcheck==0)    
        closeConnection(socketname);
    printf("Close Client\n");
    return 0;
}











