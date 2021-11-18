#include <stdio.h>
#include <stdlib.h>
#include "SignalHandler.h"

#define SYSCALL(c,e) \
    if(c==-1) { perror(e);exit(EXIT_FAILURE); }
#define SYSCALL_PTHREAD(e,c,s) \
    if((e=c)!=0) { errno=e;perror(s);fflush(stdout);exit(EXIT_FAILURE); }


sigset_t sig_mask;
#define FAST_CLOSURE 1
#define SLOW_CLOSURE 2

void Wait_Signal(int typeClose){

    while(true){
        int signal;
        SYSCALL(sigwait(&sig_mask, &signal), "sigwait");
        if(signal==SIGINT || singal==SIGQUIT){
            printf("Quick closing\n");
            typeClose=FAST_CLOSURE;
        }

        if(signal==SIGHUP){
            printf("Slow closing\n");
            typeClose=SLOW_CLOSURE;
        }
    }
}

int Create_SignalHandler(){
    int err;
    SYSCALL(sigfillset(&sig_mask), "sigfillset");
    SYSCALL_PTHREAD(err, pthread_sigmask(SIG_SETMASK, &sig_mask, NULL), "pthread_sigmask");
    struct sigaction ignore;
    memset(&ignore, 0, sizeof(ignore));
    ignore.sa_handler=SIG_IGN;
    SYSCALL(sigaction(SIGPIPE, &ignore, NULL), "sigaction");
    SYSCALL(sigemptyset(&sig_mask), "sigemptyset");
    SYSCALL(sigaddset(&sig_mask, SIGINT)), "sigaddset");
    SYSCALL(sigaddset(&sig_mask, SIGQUIT), "sigaddset");
    SYSCALL(sigaddset(&sig_mask, SIGHUP), "sigaddset");
    SYSCALL_PTHREAD(err, pthread_sigmask(SIG_SETMASK, &sig_mask, NULL), "pthread_sigmask");


    return EXIT_SUCCESS;
}
