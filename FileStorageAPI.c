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
#include "FileStorageAPI.h"



//roba da mettere nella libreria condivisa
#define CONTROL(dim, e)\
    if(dim==-1 || dim==0){perror(e); return EXIT_FAILURE;}

#define CONTROL_NULL(string, e)\
    if(!string){perror(e); return EXIT_FAILURE; }


#define CONTROL_NULL_SET_ERRNO(string, e, err)\
    if(string==NULL){perror(e); errno=err; return -1; }



#define CONTROL_CONNECTION_NOT_EXIST(fd_skt)\
    if(fd_skt==-1){ perror("The connection does not exist\n"); errno=ENOTCONN;  return -1;}

#define CONTROL_CONNECTION_EXIST(fd_skt)\
    if(fd_skt!=-1){ perror("ERROR: Already existing connection\n"); errno=EISCONN; return -1;}

#define UNIX_PATH_MAX 108
#define SYSCALL(c,e) \
    if(c==-1) { perror(e);exit(EXIT_FAILURE); }

int fd_skt=-1;
static int quiet=0;


//---------------------------------------Funzioni ausiliari per la gestione dei FILE------------------------------------
FILE* ReturnFile( char *directory, const char *name){
    //---Cerco ricorsivamente all'interno delle cartelle il FILE
    DIR * dir;
    if ((dir=opendir(".")) == NULL) {
        if(!quiet)
            perror("ERROR: Open directory %s\n");
        return NULL;
    }else{
        struct dirent *file;
        while((errno=0, file =readdir(dir)) != NULL) {
            struct stat statbuf;
            if (stat(file->d_name, &statbuf)==-1) {
                if (!quiet) {
                    perror("ERROR: Incorrect stat\n");
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
    if (errno != 0) perror("readdir");
    closedir(dir);
  }
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
        printf("File %s not saved\n", name);
        closedir(directory);
        //Proseguo la lettura del prossimo file
        return 0;
    }
    fwrite(text, sizeof(char), dim, file);
    fclose(file);
    closedir(directory);
    chdir(cwd);
    return EXIT_SUCCESS;
}

//-----------------------------------------------------Funzioni API-----------------------------------------
//TODO: sistemare tutti errno
//Apertura connessione AF_UNIX al socket file sockname
int openConnection(const char*sockname, int msec, const struct timespec abstime){
    //Verificare che i dati passati siano corretti
    if(sockname==NULL || msec<0){
        perror("ERROR: Incorrect arguments\n");
        errno=EINVAL;
        return -1;
    }
    //Verificare che il Client non sia già connesso
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

int closeConnection(const char* sockname){
    int request=10;
    if(write(fd_skt, &request, sizeof(int))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    //Verifica che la connessione sia stata instaurata
    CONTROL_CONNECTION_NOT_EXIST(fd_skt);
    close(fd_skt);
    fd_skt=-1;
return 0;
}


int openFile(const char*pathname, int flags){
    //0-> O_CREATE
    //1-> O_LOCK    

    //Verifica che i dati passati come argomenti siano corretti
    CONTROL_NULL_SET_ERRNO(pathname, "ERROR: Incorrect arguments\n", EINVAL);
    //Verificare che i flag siano o 0 o 1
    if(flags!=0 & flags!=1){
        perror("ERROR: Incorrect flags\n");
        errno=EINVAL;
        return -1;
    }
    //Verificare che vi sia la connessione
    CONTROL_CONNECTION_NOT_EXIST(fd_skt);
    //---Passaggio al server dell'operazione da effettuare
    int request=0;
    if(write(fd_skt, &request, sizeof(int))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    //---Passaggio al server del pathanme
    int len=strlen(pathname);
    if(write(fd_skt, &len, sizeof(int))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    if(write(fd_skt, pathname, len*sizeof(char))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    //---Passaggio al server dei flags
    if(write(fd_skt, &flags, sizeof(int))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    int ris, dim;
    dim=read(fd_skt, &ris, sizeof(int));
    CONTROL(dim, "Received response failed\n");
    if(ris==0){
        return 0;
    }else
       return -1;
}

int readFile(const char* pathname, void **buf, size_t* size){
    //Verifica che i dati passati come argomenti siano corretti
    CONTROL_NULL_SET_ERRNO(pathname, "ERROR: Incorrect arguments\n", EINVAL);
    //Verificare che vi sia la connessione
    CONTROL_CONNECTION_NOT_EXIST(fd_skt);
    //---Passaggio al server dell'operazione da effettuare
    int request=1;
    if(write(fd_skt, &request, sizeof(int))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    //---Passaggio al server del pathanme
    int len=strlen(pathname);
    if(write(fd_skt, &len, sizeof(int))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    if(write(fd_skt, pathname, len*sizeof(char))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    //---Lettura dimensione del FILE inviato dal server
    int dim=read(fd_skt, &size, sizeof(size_t));
    CONTROL(dim, "Request failed\n");
    int tmp=(long unsigned int)size;
    if(tmp==0)
        printf("File %s not saved in Storage\n", pathname);
    else{
        //---Lettura contenuto FILE inviato dal server
        buf=calloc(tmp+1, sizeof(char));
        CONTROL_NULL(buf, "Calloc error\n");
        if(read(fd_skt, (char *)buf, tmp*sizeof(char))==-1){
            free(buf);
            perror("Request read error\n"); 
            return EXIT_FAILURE;
        }
        printf("Text: %s\n", (char *)buf);
        free(buf);
    }
    int ris;
    dim=read(fd_skt, &ris, sizeof(int));
    CONTROL(dim, "Received response failed\n");
    if(ris==0){
        return 0;
    }else
       return -1;
return EXIT_SUCCESS;
}



int readNFiles(int N, const char* dirname){
    //Verificare che vi sia la connessione
    CONTROL_CONNECTION_NOT_EXIST(fd_skt);
    //---Passaggio al server dell'operazione da effettuare
    int request=2;
    if(write(fd_skt, &request, sizeof(int))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    //---Passaggio al server del numero di FILE
    if(write(fd_skt, &N, sizeof(int))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }

    int dim, len;
    long unsigned int tmp;
    size_t size;

    while(N>0){
        char *pathname;
        //---Lettura del nome del FILE
        dim=read(fd_skt, &len, sizeof(int));
        CONTROL(dim, "Request failed\n");
        pathname=malloc(len+1*sizeof(char));
        for(int i=0; i<len+1; i++){
            pathname[i]='\0';
        }
        CONTROL_NULL(pathname, "Calloc error\n");
        if(read(fd_skt, pathname,len*sizeof(char))==-1){
            perror("Request read error\n"); 
            free(pathname);
            return EXIT_FAILURE;
        }

        //---Lettura del contenuto del FILE
        dim=read(fd_skt, &size, sizeof(size_t));
        if(dim==-1 || dim==0){
            free(pathname);
            perror("Request failed\n");
            return EXIT_FAILURE;
        }
        tmp=(long unsigned int)size;
        char* buf=malloc(tmp+1*sizeof(char));
        memset(buf,'\0', sizeof(char));
        if(!(buf)){
            free(buf);
            free(pathname);
            perror("Calloc error\n");
            return EXIT_FAILURE;
        }
        if(read(fd_skt, buf, size*sizeof(char))==-1){
            free(buf);
            free(pathname);
            perror("ERROR: Request read error\n"); 
            return EXIT_FAILURE;
        }
        //EnterDataFolder(dirname, namefile, buf,tmp);
        if((EnterDataFolder(dirname, pathname, buf,tmp))!=0){
            free(buf);
            free(pathname);
            return EXIT_FAILURE;
        }
        free(pathname);
        free(buf);
        N--;
    }
    int ris;
    dim=read(fd_skt, &ris, sizeof(int));
    CONTROL(dim, "Received response failed\n");
    if(ris==0){
        return 0;
    }else
       return -1;
    //DEVO FERMARE  IL SERVER SE VA MALE LA LETTURA
}

int writeFile(const char* pathname, const char* dirname){
    //DA SCRIVERE BENE CHE IL FILE VIENE CERCATO TRA TUTTI I FILE APPARTENENTI AL CLIENT E LA DIRNAME SERVE SOLO PER IL RIMPIAZZAMENTO
   
   
    //Verifica che i dati passati come argomenti siano corretti
    CONTROL_NULL_SET_ERRNO(pathname, "ERROR: Incorrect arguments\n", EINVAL);
    //Verificare che vi sia la connessione
    CONTROL_CONNECTION_NOT_EXIST(fd_skt);
    //---Trovare nelle directory del Client il FILE
    char cwd[128];
    getcwd(cwd, sizeof(cwd));
    FILE *f=ReturnFile(cwd, pathname);
    CONTROL_NULL_SET_ERRNO(f, "ERROR: File does not exist. Try again with another file!\n", ENOENT);
    //---Lettura dimensione FILE
    fseek(f, 0, SEEK_END);
    int dimText=ftell(f);
    dimText=dimText/sizeof(char);
    if(dimText==0){
        printf("ERROR: File %s is empty. Try again with another file!", pathname);
        //errno
        return EXIT_FAILURE;
    }
    char *text=malloc(dimText+1*sizeof(char));
    if(text==NULL){
        perror("ERROR: buffer\n");
        //errno
        return EXIT_FAILURE;
    }
    for(int i=0; i<dimText+1; i++)
        text[i]='\0';
    fseek(f, 0, SEEK_SET);
    fread(text, sizeof(char), dimText, f);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   
    fclose(f);
    chdir(cwd); 
    printf("Dimensione del file %d\n", dimText);
    printf("Contenuto del file \n%s\n", text);
    
    int request;
    if(dirname==NULL)
        request=3;
    else    
        request=4;
    //---Passaggio al server dell'operazione da effettuare
    if(write(fd_skt, &request, sizeof(int))==-1){
        free(text);
        perror("ERROR: write\n");
        errno=EBADE;
        return -1;
    }
    //---Passaggio al server del pathname del FILE
    int len=strlen(pathname);
    if(write(fd_skt, &len, sizeof(int))==-1){
        free(text);
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    if(write(fd_skt, pathname, len*sizeof(char))==-1){
        free(text);
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    //---Passaggio al server del la dimensione del FILE
    if(write(fd_skt, &dimText, sizeof(int))==-1){
        free(text);
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    //---Passaggio al server del contenuto del FILE
    if(write(fd_skt, text, dimText*sizeof(char))==-1){
        free(text);
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }   
    if(request==4){
        int res=1;
     
        while(res!=0){   
            if(read(fd_skt, &res, sizeof(int))==-1){
                free(text);
                perror("ERROR: Request read error\n"); 
                return EXIT_FAILURE;
            }
            if(res==0)
                continue;
            //---E' stata effettuata una politica di rimpiazzamento
            char *namefile=malloc(res+1*sizeof(char));
            for(int i=0; i<res+1; i++)
                namefile[i]='\0';
            printf ("qui passi dopo malloc %s\n", namefile);
            if(read(fd_skt, namefile, res*sizeof(char))==-1){
                free(text);
                free(namefile);
                perror("ERROR: Request read error\n"); 
                return EXIT_FAILURE;
            }
            printf("Stampare il pathname %s\n", namefile);
            int dimtextfile;
            if(read(fd_skt, &dimtextfile, sizeof(int))==-1){
                free(text);
                free(namefile);
                perror("ERROR: Request read error\n"); 
                return EXIT_FAILURE;
            }
            char *textfile=calloc(dimtextfile+1, sizeof(char));
            if(read(fd_skt, textfile, dimtextfile*sizeof(char))==-1){
                free(text);
                free(namefile);
                free(textfile);
                perror("ERROR: Request read error\n"); 
                return EXIT_FAILURE;
            }

            EnterDataFolder(dirname, namefile, textfile, dimtextfile);
            printf("uscita while\n");
            free(namefile);
            free(textfile);
        }
       
    }
    int ris, dim;
    dim=read(fd_skt, &ris, sizeof(int));
    CONTROL(dim, "Received response failed\n");
    free(text);
    if(ris==0)
        return 0;
    else
        return -1;
}

int lockFile(const char* pathname){
    //Verifica che i dati passati come argomenti siano corretti
    CONTROL_NULL_SET_ERRNO(pathname, "ERROR: Incorrect arguments\n", EINVAL);
    //Verificare che vi sia la connessione
    CONTROL_CONNECTION_NOT_EXIST(fd_skt);
    //---Passaggio al server dell'operazione da effettuare
    int request=6;
    if(write(fd_skt, &request, sizeof(int))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    //---Passaggio al server del pathanme
    int len=strlen(pathname);
    if(write(fd_skt, &len, sizeof(int))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    if(write(fd_skt, pathname, len*sizeof(char))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }

    int result, dim;
    dim=read(fd_skt, &result, sizeof(int));
    CONTROL(dim, "Received response failed\n");
    printf("Result: %d\n", result);
    if(result==0){
        return 0;
    }else
       return -1;
}

int unlockFile(const char* pathname){
    //Verifica che i dati passati come argomenti siano corretti
    CONTROL_NULL_SET_ERRNO(pathname, "ERROR: Incorrect arguments\n", EINVAL);
    //Verificare che vi sia la connessione
    CONTROL_CONNECTION_NOT_EXIST(fd_skt);
    //---Passaggio al server dell'operazione da effettuare
    int request=7;
    if(write(fd_skt, &request, sizeof(int))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    //---Passaggio al server del pathanme
    int len=strlen(pathname);
    if(write(fd_skt, &len, sizeof(int))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    if(write(fd_skt, pathname, len*sizeof(char))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    int result, dim;
    dim=read(fd_skt, &result, sizeof(int));
    CONTROL(dim, "Received response failed\n");
    printf("Result: %d\n", result);
    if(result==0){
        return 0;
    }else
       return -1;
}

int closeFile(const char* pathname){
    //Verifica che i dati passati come argomenti siano corretti
    CONTROL_NULL_SET_ERRNO(pathname, "ERROR: Incorrect arguments\n", EINVAL);
    //Verificare che vi sia la connessione
    CONTROL_CONNECTION_NOT_EXIST(fd_skt);
     //---Passaggio al server dell'operazione da effettuare
    int request=8;
    if(write(fd_skt, &request, sizeof(int))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    //---Passaggio al server del pathanme
    int len=strlen(pathname);
    if(write(fd_skt, &len, sizeof(int))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    if(write(fd_skt, pathname, len*sizeof(char))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    int result, dim;
    dim=read(fd_skt, &result, sizeof(int));
    CONTROL(dim, "Received response failed\n");
    printf("Result: %d\n", result);
    if(result==0){
        return 0;
    }else
       return -1;

}

int removeFile(const char* pathname){
    //Verifica che i dati passati come argomenti siano corretti
    CONTROL_NULL_SET_ERRNO(pathname, "ERROR: Incorrect arguments\n", EINVAL);
    //Verificare che vi sia la connessione
    CONTROL_CONNECTION_NOT_EXIST(fd_skt);
     //---Passaggio al server dell'operazione da effettuare
    int request=9;
    if(write(fd_skt, &request, sizeof(int))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    //---Passaggio al server del pathanme
    int len=strlen(pathname);
    if(write(fd_skt, &len, sizeof(int))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    if(write(fd_skt, pathname, len*sizeof(char))==-1){
        perror("ERROR\n");
        errno=EBADE;
        return -1;
    }
    int result;
    int dim=read(fd_skt, &result, sizeof(int));
    CONTROL(dim, "Received response failed\n");
    printf("Result: %d\n", result);
    if(result==0){
        return 0;
    }else
       return -1;
}