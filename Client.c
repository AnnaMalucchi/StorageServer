#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "FileStorageAPI.h"
#include <time.h>
#define SOCKET_NAME "./sockname"



int main(){

    struct timespec abstime;
    abstime.tv_sec=8;
    int msec=3000;

    printf("---Client active---\n\n"); 
    if(openConnection(SOCKET_NAME, msec, abstime)==-1)
        printf("ERROR: Failed to connect.  Unable to communicate with the server\n");
    else
        printf("Connected with FileStorageServer\n");

    if(openFile("pluto", 1)==-1)    //1 tutte e due 0 solo
        printf("ERROR: File opening failed. Try again!\n");
    else    
        printf("File opened successfully\n");

    if(unlockFile("pluto")==-1)
        printf("ERROR: File unlocked failed. Try again!\n");
    else
        printf("File unlocked \n");

    if(openFile("pippo", 1)==-1)    //1 tutte e due 0 solo
        printf("ERROR: File opening failed. Try again!\n");
    else    
        printf("File opened successfully\n");

    if(unlockFile("pippo")==-1)
        printf("ERROR: File unlocked failed. Try again!\n");
    else
        printf("File unlocked \n");

    if(readNFiles(2, "./es")==-1)    //1 tutte e due 0 solo
        printf("ERROR: File opening failed. Try again!\n");
    else    
        printf("File saved successfully\n");
    
  
    if(openFile("zorro", 1)==-1)    //1 tutte e due 0 solo
        printf("ERROR: File opening failed. Try again!\n");
    else    
        printf("File opened successfully\n");

    if(writeFile("zorro", "./es")==-1)
        printf("ERROR: File writing failed\n");
    else
        printf("File writing successful\n");
    
    if(readFile("zorro", NULL, NULL)==-1)
        printf("ERROR: File reading failed\n");
    else    
        printf("File reading successfully\n");

    if(openFile("pippo", 0)==-1)    //1 tutte e due 0 solo
        printf("ERROR: File opening failed. Try again!\n");
    else    
        printf("File opened successfully\n");
    
    if(closeFile("zorro")==-1)
        printf("ERROR: File close failed\n");
    else
        printf("File closed successful\n");
    

    if(removeFile("pippo")==-1)
        printf("ERROR: File remove failed\n");
    else
        printf("File remove successful\n");
       
    if(readFile("pippo", NULL, NULL)==-1)    //1 tutte e due 0 solo
        printf("ERROR: File opening failed. Try again!\n");
    else    
        printf("File read successfully\n");

    if(lockFile("pluto")==-1)
        printf("ERROR: lock file failed. Try again!\n");
    else
        printf("File locked\n");

    if(openFile("pippo", 1)==-1)    //1 tutte e due 0 solo
        printf("ERROR: File opening failed. Try again!\n");
    else    
        printf("File opened successfully\n");
        //mettere anche nome file aperto


    if(closeConnection(SOCKET_NAME)==-1)
        printf("ERROR: Connection closure failure. Unable to communicate with the server\n");
    else
        printf("Connected terminated with FileStorageServer\n");
    

    return 0;
}