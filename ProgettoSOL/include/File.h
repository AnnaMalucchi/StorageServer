
//Lista contenente i nomi dei readers, utilizzata nella gestione delle lock di lettura
typedef struct readers{
    int reader;
    struct readers *next;
}listReaders_t;



//Struttura del File
typedef struct File{
    char path_name[128];
    char *content;
    int author;
    size_t size;
    time_t date_lastAction;   
    char lastAction[32];   
    int fd_lock;    //(scrittura)
    listReaders_t* listReaders;
    unsigned int nrReaders;
    unsigned int Writer;
    pthread_mutex_t openFile;
    pthread_mutex_t writeFile;
    pthread_cond_t condAccess;
}File_t;

//Lista contenente tutti i file
typedef struct element{
    File_t el;
    struct element *next;
}listFile_t;
listFile_t *root;


int nrFile, BytesStorage;


int MaxNrFile, MaxBytesStorage; //Utili per le statistiche
#define SYSCALL_PTHREAD(e,c,s) \
    if((e=c)!=0) { errno=e;perror(s);fflush(stdout);exit(EXIT_FAILURE); }


extern File_t* return_File_Variation(int *index);

extern void list_Initialization();

extern File_t* return_File(char *pathname);

//Verifica se il file esiste nella lista, restituisce 1 se esiste, 0 altrimenti
extern int find_File(char *pathname);

extern int nrFile_Storage();

extern void print_Storagenotext();

extern void printListReaders(File_t*f);

extern File_t* Head_list();

extern void insertReader(File_t* f, int client);

extern int controlReaders(File_t* f, int client);

extern void eliminateReader(File_t* f, int client);

extern int dimFile_Storage();

extern void print_File(File_t file);

extern void print_Storage();

extern int nrMaxFile_Storage();

extern int dimMaxFile_Storage();

extern int readn_request(int fd, void *buf, size_t n);

extern void insertFile(int author, char *pathname);

extern void remove_File(char *pathname);

extern void freeStorage();

extern void dimStorage_Update();

extern void FreeRead(File_t* f);

extern void changeContentFile(int len, char* content, char* pathname);

extern void appendToFile(int len, char* content, char* pathname);

extern void changeDate(File_t *file);

extern void FreeLock(int client);

extern void changeFdLock(File_t *file, int client);

extern int writen_response(int fd, void *buf, size_t n);

//FUNZIONI PER ORDINAMENTO LISTA
extern listFile_t *quicksort(listFile_t *start, listFile_t *end);
extern listFile_t *split(listFile_t *start, listFile_t *end);

//FUNZIONI PER ACCESSO CONCORRENTE AL FILE
extern void Read_File_lock(File_t* file, int client);
extern void Read_File_unlock(File_t* file, int client);
extern void Write_File_lock(File_t* file);
extern void Write_File_unlock(File_t* file);

