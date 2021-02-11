#include "packets.h"
#include "errors.h"

#define TIMEOUT_SECS 1
#define MAXTRIES 10

#include "server_list.h"
#include "server_get.h"
#include "server_put.h"

#define directory "./server_file/"

void CatchAlarm(int ignored){
    printf("In Alarm\n");
}

void childFunc(struct segmentPacket requestPck, int sockfd, struct sockaddr_in cl_addr, socklen_t cl_addr_len, int chunkSize, int windowSize, float loss_rate){
    
    struct sigaction myAction;
    myAction.sa_handler = CatchAlarm;
    if (sigemptyset(&myAction.sa_mask) < 0){
        DieWithError("sigfillset() failed");
    }
    myAction.sa_flags = 0;
    
    if (sigaction(SIGALRM, &myAction, 0) < 0){
        DieWithError("sigaction() failed for SIGALRM");
    }
    
    int cl_pid = requestPck.cl_pid;
    int srv_pid = getpid();
    
    // LIST
    if (requestPck.data[0] == 'l'){
        printf("\n Operation:   list\n");
        fflush(stdout);
        
        if (listFiles(sockfd,
                      cl_addr,
                      cl_addr_len,
                      directory,
                      chunkSize,
                      windowSize,
                      loss_rate,
                      cl_pid,
                      srv_pid)){
            printf("list error\n");
            fflush(stdout);
        } else {
            printf("list andato a buon fine!\n");
            fflush(stdout);
            kill(getpid(), SIGKILL);
        }
        
    // GET
    } else if (requestPck.data[0] == 'g'){
        printf("\n Operation:   get\n");
        fflush(stdout);
        
        char fileName[strlen(requestPck.data) - 4];
        memset(fileName, 0, sizeof(fileName));
        
        strncpy(fileName, requestPck.data + 4, strlen(requestPck.data) - 5);
        
        char directoryFile[strlen(directory) + strlen(fileName) + 1];
        memset(directoryFile, 0, sizeof(directoryFile));
        
        snprintf(directoryFile, sizeof directoryFile, "%s%s", directory, fileName);
        
        // se il file esiste
        if (access(directoryFile, F_OK) == 0) {
            
            int fd = open(directoryFile, O_RDONLY);
            if (fd == -1){
                printf(" open error\n");
                fflush(stdout);
            }
            
            if (getFile(fd,
                        sockfd,
                        cl_addr,
                        cl_addr_len,
                        chunkSize,
                        windowSize,
                        cl_pid,
                        srv_pid,
                        loss_rate) == 0){
                printf("File sent\n");
            }
            if (close(fd) < 0){
                printf("close error\n");
                exit(-1);
            }
            kill(getpid(), SIGKILL);
            
        // se il file NON esiste
        } else {
            
            printf("\n The required file doesn't exist!\n");
            fflush(stdout);
            
            struct ACKPacket ack;
            
            ack = createACKPacket(3, 0, cl_pid, srv_pid);
            
            // invio ACK con type 3
            if (sendto(sockfd,
                       &ack,
                       sizeof(ack),
                       0,
                       (struct sockaddr*)&cl_addr,
                       cl_addr_len) < 0) {
                perror("errore in sendto");
                exit(1);
            }
            kill(getpid(), SIGKILL);
        }
        
    // PUT
    } else if (requestPck.data[0] == 'p'){
        printf("\n Operation:   put\n");
        fflush(stdout);
        
        char fileName[strlen(requestPck.data) - 4];
        memset(fileName, 0, sizeof(fileName));
        
        strncpy(fileName, requestPck.data + 4, strlen(requestPck.data) - 5);
        
        char directoryFile[strlen(directory) + strlen(fileName) + 1];
        memset(directoryFile, 0, sizeof(directoryFile));
        
        snprintf(directoryFile, sizeof directoryFile, "%s%s", directory, fileName);
        
        // se il file esiste già
        if (access(directoryFile, F_OK) == 0) {
            printf("\n This file already exist!\n");
            fflush(stdout);
            
            struct ACKPacket ack;
            
            ack = createACKPacket(0, 0, cl_pid, srv_pid);
            
            // invio ACK con type 0
            if (sendto(sockfd,
                       &ack,
                       sizeof(ack),
                       0,
                       (struct sockaddr*)&cl_addr,
                       cl_addr_len) < 0) {
                perror("errore in sendto");
                exit(1);
            }
            
            kill(getpid(), SIGKILL);
            
        // se il file NON esiste
        } else {
            printf("\n Uploading %s\n", fileName);
            fflush(stdout);
            
            int fd = open(directoryFile, O_WRONLY | O_CREAT, 0666);
            
            if (fd == -1){
                printf(" open error\n");
                fflush(stdout);
            }
            
            if (putFile(fd,
                        sockfd,
                        cl_addr,
                        cl_addr_len,
                        cl_pid,
                        srv_pid,
                        chunkSize,
                        windowSize,
                        loss_rate)){
                printf("putFile error\n");
                fflush(stdout);
                remove(directoryFile);
            } else {
                printf("putFile andato a buon fine!\n");
                fflush(stdout);
            }
            if (close(fd) < 0){
                printf("close error\n");
                exit(-1);
            }
            kill(getpid(), SIGKILL);
        }
    }
}
