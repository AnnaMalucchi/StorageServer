#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <errno.h>
#include "../include/FileStorageAPI.h"



#define CONTROL(dim, e)\
    if(dim==-1 || dim==0){perror(e); return -1;}

#define CONTROL_NULL(string, e)\
    if(!string){perror(e); return -1; }


#define CONTROL_NULL_SET_ERRNO(string, e, err)\
    if(string==NULL){perror(e); errno=err; return -1; }


#define CONTROL_CONNECTION_NOT_EXIST(fd_skt)\
    if(fd_skt==-1){ errno=ENOTCONN; perror("The connection does not exist\n"); return -1;}

#define CONTROL_CONNECTION_EXIST(fd_skt)\
    if(fd_skt!=-1){ errno=EISCONN; perror("ERROR: Already existing connection\n"); return -1;}

#define UNIX_PATH_MAX 108
#define SYSCALL(c,e) \
    if(c==-1) { perror(e); exit(EXIT_FAILURE); }

int fd_skt=-1;
static int quiet=0;

//Variabile globale che servirà per gestire nella writen_responseFile "precedente operazione terminata con successo è una openFile"
//Tutte le volte che si effettua OpenFile (CREATE_LOCK and O_LOCK) a lastActionOpen viene assegnato il nome del file. 
//Per tutte le altre operazione viene assegnato una stringa vuota
char lastActionOpen[32];
//define per gestire questa variabile
#define SET_OPEN_EMPTY(lap)\
    {for(int i=0; i<32; i++) lap[i]='\0';}

//---------------------------------------Funzioni ausiliari per la gestione dei FILE------------------------------------
char* TokenizerPathname(const char* realpath){
    const char delimiters[] = "/";
    char *token = strtok((char*)realpath, delimiters);
    char *tokentmp; 
    while (token != NULL)
    {
        tokentmp=token;
        token = strtok(NULL, delimiters);
    }
return tokentmp;
}

FILE* ReturnFile( char *directory, const char *name){
    //---Cerco ricorsivamente all'interno delle cartelle il FILE
    DIR * dir;
    if ((dir=opendir(directory)) == NULL) {
        if(!quiet)
            perror("ERROR: Open directory\n");
        return NULL;
    }else{
        struct dirent *file;
        while(errno==0 &&(file =readdir(dir)) != NULL) {
            struct stat statbuf;
            if (stat(file->d_name, &statbuf)==-1) {
                if (!quiet) {
                    perror("ERROR: Incorrect stat\n");
                    closedir(dir);
                }
                return NULL;
            }
            if(S_ISDIR(statbuf.st_mode)) {
	            if(!isdot(file->d_name)) {
	                if(ReturnFile (file->d_name, name) != 0){
	                    if (chdir("..")==-1) {
	                        perror("ERROR: Cannot go back to the parent directory\n");
	                        return NULL;
	                    }
	                }
	            } 
            }else{
                if (strncmp(file->d_name, name, strlen(name)) == 0) {
                    //Il file esiste
                    FILE *f=fopen(name, "r");
                    closedir(dir);
                    return f;
                }
            }
        }
    }
    closedir(dir);
  return NULL;
}

int isdot(const char dir[]) {
    int l = strlen(dir); 
    if ( (l>0 && dir[l-1] == '.') ) return 1;
    return 0;
}

int EnterDataFolder(const char* dirname, char *name, char* text, long unsigned int dim){
    char cwd[40];
    getcwd(cwd, sizeof(cwd));
    DIR* directory;
    directory=opendir(dirname);
    if(!directory){
        printf("Directory does not exist.\n Directory creation\n");
        mkdir(dirname, 0777);
        directory=opendir(dirname);
    }
    if(chdir(dirname)==-1){
        errno=ENOTDIR;
        perror("ERROR: Directory does not exist");
        return EXIT_FAILURE;
    }
    FILE *file=fopen(name, "wt");
    if(file==NULL){
        //printf("File %s not saved\n", name);
        closedir(directory);
        //Proseguo la lettura del prossimo file
        return 0;
    }
    fwrite(text, sizeof(char), strlen(text), file);
    fclose(file);
    closedir(directory);
    chdir(cwd);
    return EXIT_SUCCESS;    
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
//-1--> errore, 0 errore durante la scrittura la writen_response ritorna 0, 1 la scrittura termina con successo
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

//-----------------------------------------------------Funzioni API-----------------------------------------
//Apertura connessione AF_UNIX al socket file sockname
int openConnection(const char*sockname, int msec, const struct timespec abstime){
    //Verificare che i dati passati siano corretti
    if(sockname==NULL || msec<0){
        errno=EINVAL;
        perror("ERROR: Incorrect arguments\n");
        return -1;
    }
    //Verificare che il Client non sia gia connesso
    CONTROL_CONNECTION_EXIST(fd_skt);
    //Nr millesecondi che aspetto tra i tentativi di connessione
    double second=(double)msec/1000;
    //Due strutture dati utili a contenere l'orari
    time_t t=time(NULL);
    struct tm start_time;
    int nr_sec_start_time;
    struct tm cycle_time;
    int nr_sec_cycle_time=0;

    struct sockaddr_un sa;
    strncpy(sa.sun_path, sockname, UNIX_PATH_MAX);
    sa.sun_family=AF_UNIX;
    if ((fd_skt = socket(AF_UNIX,SOCK_STREAM,0)) == -1) {
        perror("ERROR: Socket\n");
        fd_skt=-1;
        return -1;
    }
    int okConnection=0;

    //Salva l'ora di inizio richiesta connessione al server
    start_time=*localtime(&t);
    nr_sec_start_time=start_time.tm_sec+start_time.tm_min*60+start_time.tm_hour*3600;
    
    while(!okConnection && (nr_sec_cycle_time-nr_sec_start_time)<=abstime.tv_sec){
        if(connect(fd_skt,(struct sockaddr *)&sa,sizeof(sa))==-1){
            perror("ERROR: Connection with server failed. Wait a few second\n");
            sleep(second);
        }else{
            okConnection=1;
        }
        t=time(NULL);
        cycle_time=*localtime(&t);
        nr_sec_cycle_time=cycle_time.tm_sec+cycle_time.tm_min*60+cycle_time.tm_hour*3600;
    }

    if(okConnection==0){
        fd_skt=-1;
        errno=ENOTCONN;
        return -1;
    }else{
        return 0;
    }    
}

int openFile(const char*pathname, int flags){
    //Gestione lastActionOpen
    SET_OPEN_EMPTY(lastActionOpen);
    //Verifica che i dati passati come argomenti siano corretti
    CONTROL_NULL_SET_ERRNO(pathname, "ERROR: Incorrect arguments\n", EINVAL);
    //Verificare che i flag siano o 0 o 1
    if((flags!=0) & (flags!=1)){
        errno=EINVAL;
        perror("ERROR: Incorrect flags\n");
        return -1;
    }
    //Verificare che vi sia la connessione
    CONTROL_CONNECTION_NOT_EXIST(fd_skt);
    //---Passaggio al server dell'operazione da effettuare
    int request=0;
    if(writen_response(fd_skt, &request, sizeof(int))==-1){
        errno=EBADE;
        perror("ERROR\n");
        return -1;
    }
    //---Passaggio al server del pathanme
    int len=strlen(pathname);
    if(writen_response(fd_skt, &len, sizeof(int))==-1){
        errno=EBADE;
        perror("ERROR\n");
        return -1;
    }
    if(writen_response(fd_skt, (char *)pathname, len*sizeof(char))==-1){
        errno=EBADE;
        perror("ERROR\n");
        return -1;
    }
    //---Passaggio al server dei flags
    if(writen_response(fd_skt, &flags, sizeof(int))==-1){
        errno=EBADE;
        perror("ERROR\n");      
        return -1;
    }
    int ris, dim;
    dim=readn_request(fd_skt, &ris, sizeof(int));
    CONTROL(dim, "Received response failed\n");
    if(ris==0){
        strcpy(lastActionOpen, pathname);
        return 0;
    }else
       return -1;
}

int readFile(const char* pathname, void **buf, size_t* size){
    //Gestione lastActionOpen
    SET_OPEN_EMPTY(lastActionOpen);
    //Verifica che i dati passati come argomenti siano corretti
    CONTROL_NULL_SET_ERRNO(pathname, "ERROR: Incorrect arguments\n", EINVAL);
    //Verificare che vi sia la connessione
    CONTROL_CONNECTION_NOT_EXIST(fd_skt);
    //---Passaggio al server dell'operazione da effettuare
    int request=1;
    if(writen_response(fd_skt, &request, sizeof(int))==-1){
        errno=EBADE;
        perror("ERROR\n");
        return -1;
    }
    //---Passaggio al server del pathanme
    int len=strlen(pathname);
    if(writen_response(fd_skt, &len, sizeof(int))==-1){
        errno=EBADE;
        perror("ERROR\n");
        return -1;
    }
    if(writen_response(fd_skt, (char*)pathname, len*sizeof(char))==-1){
        errno=EBADE;       
        perror("ERROR\n");
        return -1;
    }
    //---Lettura dimensione del FILE inviato dal server
    int dim=readn_request(fd_skt, &size, sizeof(size_t));
    CONTROL(dim, "Request failed\n");
    int tmp=(unsigned long int)size;
    if(tmp!=0){
        int tmp=(long unsigned int)size;
        //---Lettura contenuto FILE inviato dal server
        if(tmp>0){
            CONTROL_NULL((char*)buf, "Malloc error\n");
            if(readn_request(fd_skt, (char*)*buf, tmp*sizeof(char))==-1){
                perror("Request readn_request error\n"); 
                return -1;
            }
        }
    }
    int ris;
    dim=readn_request(fd_skt, &ris, sizeof(int));
    CONTROL(dim, "Received response failed\n");
    if(ris==0){
        return 0;
    }else
       return -1;
return EXIT_SUCCESS;
}



int readNFiles(int N, const char* dirname){
    //Gestione lastActionOpen
    SET_OPEN_EMPTY(lastActionOpen);
    //Verificare che vi sia la connessione
    CONTROL_CONNECTION_NOT_EXIST(fd_skt);
    //---Passaggio al server dell'operazione da effettuare
    int request=2;
    if(writen_response(fd_skt, &request, sizeof(int))==-1){
        errno=EBADE;
        perror("ERROR writen_response\n");
        return -1;
    }
    //---Passaggio al server del numero di FILE
    if(writen_response(fd_skt, &N, sizeof(int))==-1){
        errno=EBADE;
        perror("ERROR writen_response\n");
        return -1;
    }

    int dim, len;
    long unsigned int tmp;
    size_t size;

    while(N>0){
        char *pathname;
        //---Lettura del nome del FILE
        dim=readn_request(fd_skt, &len, sizeof(int));
        if(len==1 || len==0 || len==-1){
            return EXIT_SUCCESS;
        }
        CONTROL(dim, "Request failed\n");
        pathname=malloc(len+1*sizeof(char));
        for(int i=0; i<len+1; i++){
            pathname[i]='\0';
        }
        CONTROL_NULL(pathname, "Calloc error\n");
        if(readn_request(fd_skt, (char *)pathname,len*sizeof(char))==-1){
            perror("Request readn_request error\n"); 
            free(pathname);
            return -1;
        }
        //---Lettura del contenuto del FILE
        dim=readn_request(fd_skt, &size, sizeof(size_t));
        if(dim==-1 || dim==0){
            free(pathname);
            //printf("Request failed. File empty\n");
            return -1;
        }
        tmp=(long unsigned int)size;
        char* buf=malloc(tmp+1*sizeof(char));
        for(int i=0; i<tmp+1; i++){
            buf[i]='\0';
        }
        if(!(buf)){
            free(buf);
            free(pathname);
            perror("Calloc error\n");
            return -1;
        }
        if(readn_request(fd_skt, buf, size*sizeof(char))==-1){
            free(buf);
            free(pathname);
            perror("ERROR: Request readn_request error\n"); 
            return -1;
        }
        //printf("ReadNFile->Contenuto del file %s:\n%s\n", pathname, buf);
        //Ricavo dal percorso assoluto il nome del FILE--> pathname1
        char* pathname1=malloc(32*sizeof(char));
        strcpy(pathname1,TokenizerPathname(pathname));
        if((EnterDataFolder(dirname, pathname1, buf, tmp))!=0){
            free(buf);
            free(pathname);
            return -1;
        }
        free(pathname);
        free(pathname1);
        free(buf);
        N--;
    }
    int ris;
    dim=readn_request(fd_skt, &ris, sizeof(int));
    CONTROL(dim, "Received response failed\n");
    if(ris==0 || ris==-1){
        return 0;
    }
    return -1;
}

int writeFile(const char* pathname, const char* dirname){
    //Verifica che i dati passati come argomenti siano corretti
    CONTROL_NULL_SET_ERRNO(pathname, "ERROR: Incorrect arguments\n", EINVAL);
    //Verificare che vi sia la connessione
    CONTROL_CONNECTION_NOT_EXIST(fd_skt);
    //Verifica di apertura del FILE (!=NULL) 
    FILE *f=fopen(pathname, "r");
    CONTROL_NULL_SET_ERRNO(f, "ERROR: File does not exist. Try again with another file!\n", ENOENT);
    

    if(strcmp(pathname, lastActionOpen)!=0){
        printf("ERROR: Wrong order of operations. Return -1\n");
        fclose(f);
        SET_OPEN_EMPTY(lastActionOpen);
        return -1; 
    }
    //---Lettura dimensione FILE
    fseek(f, 0, SEEK_END);
    int dimText=ftell(f);
    dimText=dimText/sizeof(char);
    if(dimText==0){
        printf("ERROR: File %s is empty. Try again with another file!", pathname);
        return -1;
    }
    char *text=malloc(dimText+1*sizeof(char));
    if(text==NULL){
        perror("ERROR: buffer\n");
        return -1;
    }
    for(int i=0; i<dimText+1; i++)
        text[i]='\0';
    fseek(f, 0, SEEK_SET);
    fread(text, sizeof(char), dimText, f);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   
    fclose(f);
   
    int request;
    if(dirname==NULL)
        request=3;
    else    
        request=4;

    //---Passaggio al server dell'operazione da effettuare
    if(writen_response(fd_skt, &request, sizeof(int))==-1){
        free(text);
        errno=EBADE;
        perror("ERROR: writen_response\n");
        return -1;
    }
    //---Passaggio al server del pathname del FILE
    int len=strlen(pathname);      
    if(writen_response(fd_skt, &len, sizeof(int))==-1){
        free(text);
        errno=EBADE;
        perror("ERROR\n");
        return -1;
    }
    if(writen_response(fd_skt, (char*)pathname, len*sizeof(char))==-1){
        free(text);
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    //---Passaggio al server del la dimensione del FILE
    if(writen_response(fd_skt, &dimText, sizeof(int))==-1){
        free(text);
        errno=EBADE;
        perror("ERROR\n");
        return -1;
    }
    //---Passaggio al server del contenuto del FILE
    if(writen_response(fd_skt, (char*)text, dimText*sizeof(char))==-1){
        free(text);
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }   

    //Se ricevo -1 significa che non ho trovato il file altrimenti se è 0 ho finto. Se fosse un altro valore significa che è la lunghezza del pathname
    int res;
    if(readn_request(fd_skt, &res, sizeof(int))==-1){
        free(text);
        perror("ERROR: Request readn_request error\n"); 
        return -1;
    }
    if(res==-1 || res==1) {
        int ris, dim;
        dim=readn_request(fd_skt, &ris, sizeof(int));
        free(text);
        CONTROL(dim, "Received response failed\n");
        if(ris==0)
            return 0;
        else
            return -1;
    }
    if(request==4){
        
        while(res!=0){  
            //--->E' stata effettuata una politica di rimpiazzamento
            char *namefile=malloc(res+1*sizeof(char));
            for(int i=0; i<res+1; i++)
                namefile[i]='\0';
            if(readn_request(fd_skt, (char*) namefile, res*sizeof(char))==-1){
                free(text);
                free(namefile);
                perror("ERROR: Request readn_request error\n"); 
                return -1;
            }
            int dimtextfile;
            if(readn_request(fd_skt, &dimtextfile, sizeof(int))==-1){
                free(text);
                free(namefile);
                perror("ERROR: Request readn_request error\n"); 
                return -1;
            }
            char* pathname1=malloc(32*sizeof(char));
            strcpy(pathname1,TokenizerPathname(namefile));
            
            if(dimtextfile==0){
                EnterDataFolder(dirname, pathname1, NULL, dimtextfile);
                free(namefile);
            }else{
                char *textfile=calloc(dimtextfile+1, sizeof(char));
                if(readn_request(fd_skt, (char*)textfile, dimtextfile*sizeof(char))==-1){
                    free(text);
                    free(namefile);
                    free(textfile);
                    free(pathname1);
                    perror("ERROR: Request readn_request error\n"); 
                    return -1;
                }
                EnterDataFolder(dirname, pathname1, textfile, dimtextfile);
                free(textfile);
                free(namefile);
                free(pathname1);
            }
            if(readn_request(fd_skt, &res, sizeof(int))==-1){
                perror("ERROR: Request readn_request error\n"); 
                free(text);
                return -1;
            }
            if(res==-1){
                break;
            }        
        }
    }
    int ris, dim;
    dim=readn_request(fd_skt, &ris, sizeof(int));
    free(text);
    CONTROL(dim, "Received response failed\n");
    if(ris==0)
        return 0;
    else
        return -1;
}


int appendToFile(const char* pathname, void *buf, size_t size, const char *dirname){
    //Gestione lastActionOpen
    SET_OPEN_EMPTY(lastActionOpen);
    //Verifica che i dati passati come argomenti siano corretti
    CONTROL_NULL_SET_ERRNO(pathname, "ERROR: Incorrect arguments\n", EINVAL);
    //Verificare che vi sia la connessione
    CONTROL_CONNECTION_NOT_EXIST(fd_skt);
    if((char*)buf==NULL)
        return -1;
    int request;
    if(dirname==NULL)
        request=5;
    else    
        request=6;

    //printf("il buffer passato è %s\n", (char *)buf);
    //---Passaggio al server dell'operazione da effettuare
    if(writen_response(fd_skt, &request, sizeof(int))==-1){
        errno=EBADE; 
        perror("ERROR: writen_response\n");
        return -1;
    }
    //---Passaggio al server del pathname del FILE
    int len=strlen(pathname);
    if(writen_response(fd_skt, &len, sizeof(int))==-1){
        errno=EBADE;
        perror("ERROR\n");
        return -1;
    }
    if(writen_response(fd_skt, (char*)pathname, len*sizeof(char))==-1){
        errno=EBADE; 
        perror("ERROR\n");
        return -1;
    }
    int x=(unsigned int)size;
    //---Passaggio al server del la dimensione del testo
    if(writen_response(fd_skt, &x, sizeof(int))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    //---Passaggio al server del contenuto del testo
    if(writen_response(fd_skt, (char*)buf, size*sizeof(char))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }   
    //Se ricevo -1 significa che non ho trovato il file altrimenti se è 0 ho finto. Se fosse un altro valore significa che è la lunghezza del pathname
    int res;
    if(readn_request(fd_skt, &res, sizeof(int))==-1){
        perror("ERROR: Request readn_request error\n"); 
        return EXIT_FAILURE;
    }
    if(res==-1) {
      if(res==1) {
        int ris, dim;
        dim=readn_request(fd_skt, &ris, sizeof(int));
        CONTROL(dim, "Received response failed\n");
        if(ris==0)
            return 0;
        else
            return -1;
        }
    }
    if(request==6){
        while(res!=0){   
            if(readn_request(fd_skt, &res, sizeof(int))==-1){
                perror("ERROR: Request readn_request error\n"); 
                return EXIT_FAILURE;
            }
            if(res<=0)
                continue;
            //---E' stata effettuata una politica di rimpiazzamento
            char *namefile=malloc(res+1*sizeof(char));
            for(int i=0; i<res+1; i++)
                namefile[i]='\0';
        
            if(readn_request(fd_skt, (char*)namefile, res*sizeof(char))==-1){
                free(namefile);
                perror("ERROR: Request readn_request error\n"); 
                return -1;
            }
            //printf("Stampare il pathname %s\n", namefile);
            int dimtextfile;
            if(readn_request(fd_skt, &dimtextfile, sizeof(int))==-1){
                free(namefile);
                perror("ERROR: Request readn_request error\n"); 
                return -1;
            }

             char* pathname1=malloc(32*sizeof(char));
            strcpy(pathname1,TokenizerPathname(namefile));
            if(dimtextfile==0){
                EnterDataFolder(dirname, pathname1, NULL, dimtextfile);
                free(namefile);
            }else{
                char *textfile=calloc(dimtextfile+1, sizeof(char));
                if(readn_request(fd_skt, (char*)textfile, dimtextfile*sizeof(char))==-1){
                    free(namefile);
                    free(textfile);
                    free(pathname1);
                    perror("ERROR: Request readn_request error\n"); 
                    return -1;
                }
                //printf("ecco cosa arriva %s\n", textfile);
                EnterDataFolder(dirname, pathname1, textfile, dimtextfile);
                free(namefile);
                free(textfile);
                free(pathname1);
            }

            if(readn_request(fd_skt, &res, sizeof(int))==-1){
                perror("ERROR: Request readn_request error\n"); 
                return -1;
            }
            if(res==-1){
                int ris, dim;
                dim=readn_request(fd_skt, &ris, sizeof(int));
                CONTROL(dim, "Received response failed\n");
                if(ris==0)
                    return 0;
                else
                   return -1;
             }
        }
       
    }
    //printf("Valore di res: %d\n", res);
    int ris, dim;
    dim=readn_request(fd_skt, &ris, sizeof(int));
    CONTROL(dim, "Received response failed\n");
    if(ris==0 && res!=-1)
        return 0;
    else
        return -1;
}

int lockFile(const char* pathname){
    //Gestione lastActionOpen
    SET_OPEN_EMPTY(lastActionOpen);
    //Verifica che i dati passati come argomenti siano corretti
    CONTROL_NULL_SET_ERRNO(pathname, "ERROR: Incorrect arguments\n", EINVAL);
    //Verificare che vi sia la connessione
    CONTROL_CONNECTION_NOT_EXIST(fd_skt);
    //---Passaggio al server dell'operazione da effettuare
    int request=7;
    if(writen_response(fd_skt, &request, sizeof(int))==-1){
        errno=EBADE;
        perror("ERROR\n");
        return -1;
    }
    //---Passaggio al server del pathanme
    int len=strlen(pathname);
    if(writen_response(fd_skt, &len, sizeof(int))==-1){
        errno=EBADE;
        perror("ERROR\n");
        return -1;
    }
    if(writen_response(fd_skt, (char*)pathname, len*sizeof(char))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }

    int result, dim;
    dim=readn_request(fd_skt, &result, sizeof(int));
    CONTROL(dim, "Received response failed\n");
    if(result==0){
        return 0;
    }else
       return -1;
}

int unlockFile(const char* pathname){
    //Gestione lastActionOpen
    SET_OPEN_EMPTY(lastActionOpen);
    //Verifica che i dati passati come argomenti siano corretti
    CONTROL_NULL_SET_ERRNO(pathname, "ERROR: Incorrect arguments\n", EINVAL);
    //Verificare che vi sia la connessione
    CONTROL_CONNECTION_NOT_EXIST(fd_skt);
    //---Passaggio al server dell'operazione da effettuare
    int request=8;
    if(writen_response(fd_skt, &request, sizeof(int))==-1){
        errno=EBADE;
        perror("ERROR\n");
        return -1;
    }
    //---Passaggio al server del pathanme
    int len=strlen(pathname);
    if(writen_response(fd_skt, &len, sizeof(int))==-1){
        errno=EBADE;
        perror("ERROR\n");
        return -1;
    }
    if(writen_response(fd_skt, (char *)pathname, len*sizeof(char))==-1){
        errno=EBADE;
        perror("ERROR\n");
        return -1;
    }
    int result, dim;
    dim=readn_request(fd_skt, &result, sizeof(int));
    CONTROL(dim, "Received response failed\n");
    if(result==0){
        return 0;
    }else
       return -1;
}

int closeFile(const char* pathname){
    //Gestione lastActionOpen
    SET_OPEN_EMPTY(lastActionOpen);
    //Verifica che i dati passati come argomenti siano corretti
    CONTROL_NULL_SET_ERRNO(pathname, "ERROR: Incorrect arguments\n", EINVAL);
    //Verificare che vi sia la connessione
    CONTROL_CONNECTION_NOT_EXIST(fd_skt);
     //---Passaggio al server dell'operazione da effettuare
    int request=9;
    if(writen_response(fd_skt, &request, sizeof(int))==-1){
        errno=EBADE; 
        perror("ERROR\n");
        return -1;
    }
    //---Passaggio al server del pathanme
    int len=strlen(pathname);
    if(writen_response(fd_skt, &len, sizeof(int))==-1){
        errno=EBADE;
        perror("ERROR\n");
        return -1;
    }
    if(writen_response(fd_skt, (char*)pathname, len*sizeof(char))==-1){
        errno=EBADE;
        perror("ERROR\n");
        return -1;
    }
    int result, dim;
    dim=readn_request(fd_skt, &result, sizeof(int));
    CONTROL(dim, "Received response failed\n");
    if(result==0){
        return 0;
    }else
       return -1;
}

int removeFile(const char* pathname){
    //Gestione lastActionOpen
    SET_OPEN_EMPTY(lastActionOpen);
    //Verifica che i dati passati come argomenti siano corretti
    CONTROL_NULL_SET_ERRNO(pathname, "ERROR: Incorrect arguments\n", EINVAL);
    //Verificare che vi sia la connessione
    CONTROL_CONNECTION_NOT_EXIST(fd_skt);
     //---Passaggio al server dell'operazione da effettuare
    int request=10;
    if(writen_response(fd_skt, &request, sizeof(int))==-1){
        errno=EBADE;
        perror("ERROR\n");
        return -1;
    }
    //---Passaggio al server del pathanme
    int len=strlen(pathname);
    if(writen_response(fd_skt, &len, sizeof(int))==-1){
        errno=EBADE;
        perror("ERROR\n");
        return -1;
    }
    if(writen_response(fd_skt, (char*)pathname, len*sizeof(char))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    int result;
    int dim=readn_request(fd_skt, &result, sizeof(int));
    CONTROL(dim, "Received response failed\n");
    if(result==0){
        return 0;
    }else
       return -1;
}


int closeConnection(const char* sockname){
    int request=11;
    if(writen_response(fd_skt, &request, sizeof(int))==-1){
        errno=EBADE;
        perror("ERROR\n");
        return -1;
    }
    //Verifica che la connessione sia stata instaurata
    CONTROL_CONNECTION_NOT_EXIST(fd_skt);
    close(fd_skt);
    fd_skt=-1;
return 0;
}
