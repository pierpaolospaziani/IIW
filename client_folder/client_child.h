#include "packets.h"
#include "errors.h"

#define IP_ADDRESS "127.0.0.1"
#define PORT_NO 5193
#define TIMEOUT_SECS 1
#define MAXTRIES 10

#include "client_list.h"
#include "client_get.h"
#include "client_put.h"

void CatchAlarm(int ignored){
    printf("In Alarm\n");
}

void childFunc(char* inputString, char* command, int fd, int chunkSize, int windowSize, float loss_rate){
    
    struct sigaction myAction;
    myAction.sa_handler = CatchAlarm;
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
        perror("errore in socket");
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
        fprintf(stderr, "errore in inet_pton per %s", IP_ADDRESS);
        exit(1);
    }
    
    /*                   ------------------------------    */
    /*   requestPck :   | 0 | 0 | cl_pid | 0 | request |   */
    /*                   ------------------------------    */
    struct segmentPacket requestPck;
    requestPck = createDataPacket(0, 0, cl_pid, srv_pid, inputString);
    printf("Sending your request\n");
    
    if (sendto(sockfd,
               &requestPck,
               sizeof(requestPck),
               0,
               (struct sockaddr*) &cl_addr,
               cl_addr_size) != sizeof(requestPck)) {
        perror("errore in sendto");
        exit(1);
    }
    
    alarm(TIMEOUT_SECS);
    
    // LIST
    if (strcmp(command,"list") == 0){
        if (listFiles(sockfd,
                      cl_addr,
                      cl_addr_size,
                      cl_pid,
                      chunkSize,
                      windowSize,
                      loss_rate)){
            printf("listFiles error\n");
            fflush(stdout);
        } else {
            printf("listFiles succeeded!\n");
            fflush(stdout);
            kill(getpid(), SIGKILL);
        }
    }
    
    // GET
    if (strcmp(command,"get") == 0){
        if (getFile(fd,
                    sockfd,
                    cl_addr,
                    cl_addr_size,
                    cl_pid,
                    chunkSize,
                    windowSize,
                    loss_rate)){
            printf("getFile error\n");
            fflush(stdout);
        } else {
            printf("file downloaded!\n");
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
                    printf("----------------------- Recieved ACK for requestPck\n");
                    break;
                } else if (requestACK.type == 0){
                    printf("----------------------- This file already exist!\n");
                    // controlla bene sta cosa che l'ho scritta al volo
                    alarm(0);
                    kill(getpid(), SIGKILL);
                }
            }
        }
        
        alarm(0);
        srv_pid = requestACK.srv_pid;
        
        if (putFile(fd,
                    sockfd,
                    cl_addr,
                    srv_addr,
                    srv_addr_size,
                    chunkSize,
                    windowSize,
                    cl_pid,
                    srv_pid) == 0){
            printf("File sent\n");
        }
        if (close(fd) < 0){
            printf("close error\n");
            exit(-1);
        }
        kill(getpid(), SIGKILL);
    }
}
