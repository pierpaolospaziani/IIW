#include "packets.h"
#include "loss.h"

#define MAXTRIES 5

#include "server_list.h"
#include "server_get.h"
#include "server_put.h"

#define directory "./server_file/"
#define tempDirectory "./server_file/temp/"

void alarmServer(int signum){
    //printf(" Timeout");
    //fflush(stdout);
}

void childFunc(struct segmentPacket requestPck, int sockfd, struct sockaddr_in cl_addr, socklen_t cl_addr_len, int chunkSize, int windowSize, float loss_rate, int timeout){
    
    int cl_pid = requestPck.cl_pid;
    int srv_pid = getpid();
    
    struct sockaddr_in fool_addr;
    unsigned int fool_addr_len;
    
    struct sigaction myAction;
    myAction.sa_handler = alarmServer;
    if (sigemptyset(&myAction.sa_mask) < 0){
        fprintf(stderr,"sigfillset() failed");
        exit(1);
    }
    myAction.sa_flags = 0;
    
    if (sigaction(SIGALRM, &myAction, 0) < 0){
        fprintf(stderr,"sigaction() failed for SIGALRM");
        exit(1);
    }
    
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
                      timeout,
                      cl_pid,
                      srv_pid)){
            printf("list error\n");
            fflush(stdout);
        } else {
            printf(" List sent!\n");
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
                        loss_rate,
                        timeout) == 0){
                printf(" File sent!\n");
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
            
            int tries = 0;
            alarm(timeout);
            
            struct segmentPacket dataPacket;
            char seg_data[chunkSize];
            memset(seg_data, 0, sizeof(seg_data));
            dataPacket = createDataPacket(3, 0, cl_pid, srv_pid, seg_data);
            
            while (1) {
                // invio pacchetto con type 3
                if(!is_lost(loss_rate)){
                    if (sendto(sockfd,
                               &dataPacket,
                               sizeof(dataPacket),
                               0,
                               (struct sockaddr*)&cl_addr,
                               cl_addr_len) < 0) {
                        perror("errore in sendto");
                        kill(getpid(), SIGKILL);
                    }
                } else {
                    //printf("Loss simulation\n");
                }
                
                struct ACKPacket ack;
                while (recvfrom(sockfd,
                                &ack,
                                sizeof(ack),
                                MSG_PEEK,
                                (struct sockaddr *) &fool_addr,
                                &fool_addr_len) < 0) {
                    if (errno != EINTR){
                        fprintf(stderr,"recvfrom() failed");
                        exit(1);
                    } else if (errno == EINTR){
                        //printf(": Resending for the %d time\n", tries+1);
                        tries++;
                        if (tries >= MAXTRIES){
                            printf(" Client is not responding, probably it's disconnected!\n");
                            kill(getpid(), SIGKILL);
                        }
                    }
                    alarm(timeout);
                }
                
                if (ack.srv_pid == srv_pid){
                    
                    if (recvfrom(sockfd,
                                 &ack,
                                 sizeof(ack),
                                 0,
                                 (struct sockaddr *) &cl_addr,
                                 &cl_addr_len) < 0) {
                        if (errno != EINTR){
                            fprintf(stderr,"recvfrom() failed");
                            exit(1);
                        }
                    }
                    
                    if(ack.type == 5){
                        //printf(" Recieved Confirm ACK\n");
                        kill(getpid(), SIGKILL);
                    }
                }
            }
        }
        
    // PUT
    } else if (requestPck.data[0] == 'p'){
        printf("\n Operation:   put\n");
        fflush(stdout);
        
        timeout = 3;
        
        char fileName[strlen(requestPck.data) - 4];
        memset(fileName, 0, sizeof(fileName));
        
        strncpy(fileName, requestPck.data + 4, strlen(requestPck.data) - 5);
        
        char directoryFile[strlen(tempDirectory) + strlen(fileName) + 1];
        memset(directoryFile, 0, sizeof(directoryFile));
        
        snprintf(directoryFile, sizeof directoryFile, "%s%s", tempDirectory, fileName);
        
        char finalDirectoryFile[strlen(directory) + strlen(fileName) + 1];
        memset(finalDirectoryFile, 0, sizeof(finalDirectoryFile));
        snprintf(finalDirectoryFile, sizeof finalDirectoryFile, "%s%s", directory, fileName);
        
        // se il file esiste già
        if (access(directoryFile, F_OK) == 0 || access(finalDirectoryFile, F_OK) == 0) {
            printf("\n This file already exist!\n");
            fflush(stdout);
            
            struct ACKPacket ack;
            
            ack = createACKPacket(0, 0, cl_pid, srv_pid);
            
            // invio ACK con type 0
            if(!is_lost(loss_rate)){
                if (sendto(sockfd,
                           &ack,
                           sizeof(ack),
                           0,
                           (struct sockaddr*)&cl_addr,
                           cl_addr_len) < 0) {
                    perror("errore in sendto");
                    kill(getpid(), SIGKILL);
                }
            } else {
                //printf("Loss simulation\n");
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
            
            // invio ACK con type 1 e procedo
            if (putFile(fd,
                        sockfd,
                        cl_addr,
                        cl_addr_len,
                        cl_pid,
                        srv_pid,
                        chunkSize,
                        loss_rate,
                        timeout,
                        directoryFile)){
                remove(directoryFile);
            } else {
                printf(" File received!\n");
                fflush(stdout);
                rename(directoryFile,finalDirectoryFile);
            }
            if (close(fd) < 0){
                printf("close error\n");
                exit(-1);
            }
            kill(getpid(), SIGKILL);
        }
    }
}
