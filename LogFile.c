
#include "LogFile.h"


//Creazione del file LOG inizialmente vuoto
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


void InsertActionLog(char *txt, char *name){
    //---Apertura del file in modo che scriva in append
    FILE *file=fopen(name, "a");
    if(file==NULL){
        printf("Error insert in LOG file\n");
        return;
    }
    //---Inserimento operazione effettuata
    fwrite(txt, sizeof(char), strlen(txt), file);
    fclose(file);
}

