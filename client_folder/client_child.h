#include "packets.h"
#include "errors.h"

#define IP_ADDRESS "127.0.0.1"
#define PORT_NO 5193

#define MAXTRIES 5

#include "client_list.h"
#include "client_get.h"
#include "client_put.h"

char* file;

void alarmNoServer(int ignored){
    printf(" Server is not responding, probably it's offline!\n");
    exit(1);
}
void alarmNoServerGet(int ignored){
    printf(" Server is not responding, probably it's offline!\n");
    remove(file);
    exit(1);
}

void childFunc(char* inputString, char* command, char* directoryFile, int fd, int chunkSize, int windowSize, float loss_rate, int timeout){
    
    file = directoryFile;
    
    struct sigaction myAction;
    myAction.sa_handler = alarmNoServer;
    if (sigemptyset(&myAction.sa_mask) < 0){
        DieWithError("sigfillset() failed");
    }
    myAction.sa_flags = 0;
    
    if (sigaction(SIGALRM, &myAction, 0) < 0){
        DieWithError("sigaction() failed for SIGALRM");
    }
    
    int cl_pid = getpid();
    int srv_pid = 0;
    
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0){
        perror("error in socket");
        exit(1);
    }
    
    struct sockaddr_in cl_addr;
    
    struct sockaddr_in srv_addr;
    unsigned int srv_addr_size;
    
    memset((void *)&cl_addr, 0, sizeof(cl_addr));
    socklen_t cl_addr_size = sizeof(cl_addr);
    cl_addr.sin_family = AF_INET;
    cl_addr.sin_port = htons(PORT_NO);
    if (inet_pton(AF_INET, IP_ADDRESS, &cl_addr.sin_addr) <= 0) {
        fprintf(stderr, "error in inet_pton per %s", IP_ADDRESS);
        exit(1);
    }
    
    /*                   ------------------------------    */
    /*   requestPck :   | 0 | 0 | cl_pid | 0 | request |   */
    /*                   ------------------------------    */
    struct segmentPacket requestPck;
    requestPck = createDataPacket(0, 0, cl_pid, srv_pid, inputString);
    
    if(!is_lost(loss_rate)){
        if (sendto(sockfd,
                   &requestPck,
                   sizeof(requestPck),
                   0,
                   (struct sockaddr*) &cl_addr,
                   cl_addr_size) != sizeof(requestPck)) {
            perror("errore in sendto");
            exit(1);
        }
    } else {
        printf("SIMULATED LOSE\n");
    }
    
    alarm(timeout);
    
    // LIST
    if (strcmp(command,"list") == 0){
        printf("\n The file list has been requested ...\n\n");
        fflush(stdout);
        if (listFiles(sockfd,
                      cl_addr,
                      cl_addr_size,
                      cl_pid,
                      chunkSize,
                      windowSize,
                      loss_rate,
                      timeout)){
            printf("listFiles error\n");
            fflush(stdout);
        } else {
            //printf(" The required list is above!\n");
            fflush(stdout);
            kill(getpid(), SIGKILL);
        }
    }
    
    // GET
    if (strcmp(command,"get") == 0){
        printf("\n Your file has been requested ...\n\n");
        fflush(stdout);
        
        myAction.sa_handler = alarmNoServerGet;
        if (sigaction(SIGALRM, &myAction, 0) < 0){
            DieWithError("sigaction() failed for SIGALRM");
        }
        
        if (getFile(fd,
                    sockfd,
                    cl_addr,
                    cl_addr_size,
                    cl_pid,
                    chunkSize,
                    windowSize,
                    loss_rate,
                    timeout)){
            remove(directoryFile);
        } else {
            printf(" File downloaded successfully!\n");
            fflush(stdout);
        }
        if (close(fd) < 0){
            printf("close error\n");
            exit(-1);
        }
        kill(getpid(), SIGKILL);
    }
    
    // PUT
    if (strcmp(command,"put") == 0){
        
        printf("\n Trying to upload your file ...\n\n");
        fflush(stdout);
        
        struct ACKPacket requestACK;
        
        while (1){
            
            if (recvfrom(sockfd,
                         &requestACK,
                         sizeof(requestACK),
                         MSG_PEEK,
                         (struct sockaddr *) &srv_addr,
                         &srv_addr_size) < 0) {
                if (errno != EINTR){
                    perror("errore in recvfrom");
                    exit(1);
                }
            }
            
            if (requestACK.cl_pid == cl_pid){
                
                alarm(0);
                
                if (recvfrom(sockfd,
                             &requestACK,
                             sizeof(requestACK),
                             0,
                             (struct sockaddr *) &srv_addr,
                             &srv_addr_size) < 0) {
                    if (errno != EINTR){
                        perror("errore in recvfrom");
                        exit(1);
                    }
                }
                
                if (requestACK.type == 1){
                    //printf("----------------------- Recieved ACK for requestPck\n");
                    break;
                } else if (requestACK.type == 0){
                    printf(" This file already exist!\n");
                    kill(getpid(), SIGKILL);
                }
            }
        }
        
        srv_pid = requestACK.srv_pid;
        
        if (putFile(fd,
                    sockfd,
                    cl_addr,
                    srv_addr,
                    srv_addr_size,
                    chunkSize,
                    windowSize,
                    cl_pid,
                    srv_pid,
                    loss_rate,
                    timeout) == 0){
            printf(" File uploaded successfully!\n");
        }
        if (close(fd) < 0){
            printf("close error\n");
            exit(-1);
        }
        
        kill(getpid(), SIGKILL);
    }
}
