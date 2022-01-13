#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#define INSERT_OP(ris, text, name){\
    if(ris==0){InsertActionLog(text, name, NULL);};}

#define INSERT_OP_MOD(text, name, pathname){\
   {InsertActionLog(text, name, pathname);};}

extern int CreateFileLog(char *name);

extern void InsertActionLog(char *txt, char *name, char *pathname);