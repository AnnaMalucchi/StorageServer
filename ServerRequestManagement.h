//readn_request
extern int readn_request(int fd, void *buf, size_t n);

//writen_response
extern int writen_response(int fd, void *buf, size_t n);

//
extern void switchRequest(int idRequest, int client, int worker);

extern int OpenFile(int client, int worker);