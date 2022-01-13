#include "../include/LogFile.h"

//CreateFileLog: crea il File LOG inizialmente vuoto
int CreateFileLog(char *name){
    //---Creazione di un file di LOG inizialmente vuoto
    FILE *file=fopen(name, "wt");
    if(file==NULL){
        printf("Error in creating LOG file\n");
        return EXIT_FAILURE;
    }
    fclose(file);
    return EXIT_SUCCESS;
}

//InsertActionLog: apre il File LOG e vi inserisce in append la nuova riga di testo "txt" con valore int
void InsertActionLog(char *txt, char *name, char *pathname){
    //---Apertura del file in modo che scriva in append
    FILE *file=fopen(name, "a");
    if(file==NULL){
        printf("Error insert in LOG file\n");
        return;
    }
    //---Inserimento operazione effettuata
    fwrite(txt, sizeof(char), strlen(txt), file);
    if(pathname!=NULL){
        //Controllare che pathname non sia un numero
        if(isdigit(pathname[0])){
            int x=atoi(pathname);
            fprintf(file,"%d",x);
        }else{   
            fwrite(pathname, sizeof(char), strlen(pathname), file);
        }
        fwrite("\n", sizeof(char), strlen("\n"), file);         
    }
    fclose(file);
    return;
}

//InsertActionLog_float: apre il File LOG e vi inserisce in append la nuova riga di testo "txt" con valore float
void InsertActionLog_float(char *txt, char *name, char *pathname){
    //---Apertura del file in modo che scriva in append
    FILE *file=fopen(name, "a");
    if(file==NULL){
        printf("Error insert in LOG file\n");
        return;
    }
    //---Inserimento operazione effettuata
    fwrite(txt, sizeof(char), strlen(txt), file);
    if(pathname!=NULL){
        //Controllare che pathname non sia un numero
        if(isdigit(pathname[0])){
            float x=strtof(pathname, NULL);
            fprintf(file,"%f",x);
        }else{   
            fwrite(pathname, sizeof(char), strlen(pathname), file);
        }
        fwrite("\n", sizeof(char), strlen("\n"), file);         
    }
    fclose(file);
    return;
}