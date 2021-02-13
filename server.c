#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#define IP_ADDRESS "127.0.0.1"
#define PORT_NO 5193
#define DATALIMIT 511

#include "./server_folder/server_child.h"

typedef void Sigfunc(int);

Sigfunc *signal(int signum, Sigfunc *func){
    struct sigaction act, oact;
    
    act.sa_handler = func;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if (signum != SIGALRM){
        act.sa_flags |= SA_RESTART;
    }
    if (sigaction(signum, &act, &oact) < 0){
        return(SIG_ERR);
    }
    return(oact.sa_handler);
}

void sig_chld_handler(int signum){
    int status;
    pid_t pid;

    while((pid = waitpid(-1, &status, WNOHANG)) > 0){
        //printf(" child %d terminato\n", pid);
    }
    return;
}

int main(int argc, char *argv[]) {
    
    int sockfd;
    struct sockaddr_in srv_addr;
    
    struct sockaddr_in cli_addr;
    unsigned int cl_addr_len;
    
    if (argc < 3 || argc > 5){
        fprintf(stderr,"\n Usage: %s <Chunk Size> <Window Size> <Loss Rate> <Timer>\n Loss Rate and Timer are optional, if not specified are set to 0 and 1.\n You gave %d Argument/s.\n\n", argv[0], argc);
        exit(1);
    }
    
    srand48(2345);
    
    memset((void *)&srv_addr, 0, sizeof(srv_addr));
    socklen_t addrlen = sizeof(srv_addr);
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(PORT_NO);
    srv_addr.sin_addr.s_addr = INADDR_ANY;
    
    int chunkSize = atoi(argv[1]);
    if(chunkSize > DATALIMIT){
        fprintf(stderr, "Error: Chunk Size is too large. Must be < 512 bytes\n");
        exit(1);
    }
    
    int windowSize = atoi(argv[2]);
    if(windowSize < 1){
        fprintf(stderr, "Error: Window Size must be > 1\n");
        exit(1);
    }
    
    float loss_rate = 0;
    
    if (argc == 4){
        loss_rate = atof(argv[3]);
        if(loss_rate > 1){
            fprintf(stderr, "Error: Loss Rate must be < 1\n");
            exit(1);
        }
    }
    
    int userTimer = 1;
    
    if (argc == 5){
        userTimer = atoi(argv[4]);
        if(userTimer < 1){
            fprintf(stderr, "Error: Timer must be > 1\n");
            exit(1);
        }
    }
    
    if (signal(SIGCHLD, sig_chld_handler) == SIG_ERR) {
        fprintf(stderr, "errore in signal");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0){
        perror("errore in socket");
        exit(1);
    }

    if (bind(sockfd, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0){
          perror("errore in bind");
          exit(1);
    }
    
    printf("\n ---------------- Server started successfully! ---------------\n");
    fflush(stdout);
    
    pid_t pid;
    
    struct segmentPacket requestPck;
    
    while (1) {
        
        memset(requestPck.data, 0, sizeof(requestPck.data));
        
        cl_addr_len = sizeof(cli_addr);
        
        if (recvfrom(sockfd,
                     &requestPck,
                     sizeof(requestPck),
                     MSG_PEEK,
                     (struct sockaddr*)&cli_addr,
                     &cl_addr_len) < 0) {
            if (errno != EINTR){
                DieWithError("recvfrom() failed");
            }
        }
        
        if (requestPck.srv_pid == 0){
            
            if (recvfrom(sockfd,
                         &requestPck,
                         sizeof(requestPck),
                         0,
                         (struct sockaddr*)&cli_addr,
                         &cl_addr_len) < 0) {
                if (errno != EINTR){
                    DieWithError("recvfrom() failed");
                }
            }
            
            if ((pid = fork()) == 0) {
                childFunc(requestPck,
                          sockfd,
                          cli_addr,
                          cl_addr_len,
                          chunkSize,
                          windowSize,
                          loss_rate,
                          userTimer);
            }
        }
    }
    return 0;
}
