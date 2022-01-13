extern int writen_response(int fd, void *buf, size_t n);

extern int readn_request(int fd, void *buf, size_t n);

extern int openConnection(const char*sockname, int msec, const struct timespec abstime);

extern char* TokenizerPathname(const char* realpath);

extern int openFile(const char*pathname, int flags);

extern int readFile(const char* pathname, void **buf, size_t* size);

extern int readNFiles(int N, const char* dirname);

extern int writeFile(const char* pathname, const char* dirname);

extern int appendToFile(const char* pathname, void *buf, size_t size, const char *dirname);

extern int lockFile(const char* pathname);

extern int unlockFile(const char* pathname);

extern int closeFile(const char* pathname);

extern int removeFile(const char* pathname);

extern FILE* ReturnFile(char *directory,const char *name);

extern int isdot(const char dir[]);

extern int EnterDataFolder(const char* dirname, char *name, char* text, long unsigned int dim);


/*closeConnection: viene chiusa la connessione associata al socket file 'sockname'. 
Restituisce 0 in caso di successo, -1 altrimenti.*/
extern int closeConnection(const char* sockname);
