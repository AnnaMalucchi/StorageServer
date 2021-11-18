
//Struttura del File
typedef struct File{
    char path_name[32];
    char *content;
    int author;
    size_t size;
    time_t date_lastAction;   
    char lastAction[32];   
    //accesso e scrittura concorrente
    int fd_lock;
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

#define SYSCALL_PTHREAD(e,c,s) \
    if((e=c)!=0) { errno=e;perror(s);fflush(stdout);exit(EXIT_FAILURE); }


extern File_t* return_File_Variation(int *index);

extern void list_Initialization();

extern File_t* return_File(char *pathname);

//Verifica se il file esiste nella lista, restituisce 1 se esiste, 0 altrimenti
extern int find_File(char *pathname);

extern int nrFile_Storage();

File_t* Head_list();

extern int dimFile_Storage();

extern void print_File(File_t file);

extern void print_Storage();

extern int readn_request(int fd, void *buf, size_t n);

extern void insertFile(int author, char *pathname, char* text, int len_text);

extern void remove_File(char *pathname);

extern void freeStorage();

extern void dimStorage_Update();

extern void changeContentFile(int len, char* content, char* pathname);

extern void changeDate(File_t *file);

extern void FreeLock(int client);

extern void changeFdLock(File_t *file, int client);

extern int writen_response(int fd, void *buf, size_t n);

//FUNZIONI PER ORDINAMENTO LISTA
extern listFile_t *quicksort(listFile_t *start, listFile_t *end);
extern listFile_t *split(listFile_t *start, listFile_t *end);

//FUNZIONI PER ACCESSO CONCORRENTE AL FILE
extern void Read_File_lock(File_t* file);
extern void Read_File_unlock(File_t* file);
extern void Write_File_lock(File_t* file);
extern void Write_File_unlock(File_t* file);

