#include "packets.h"
#include "loss.h"

#define IP_ADDRESS "127.0.0.1"
#define PORT_NO 5193

#define MAXTRIES 5

#include "client_list.h"
#include "client_get.h"
#include "client_put.h"

char* tempFile;
char* file;

void alarmNoServer(int signum){
    printf(" Server is not responding, probably it's offline!\n");
    rename(tempFile, file);
    kill(getpid(), SIGKILL);
}
void alarmNoServerGet(int signum){
    printf(" Server is not responding, probably it's offline!\n");
    remove(tempFile);
    kill(getpid(), SIGKILL);
}

void childFunc(char* inputString, char* command, char* directoryFile, char* tempDirectoryFile, int fd, int chunkSize, int windowSize, float loss_rate, int timeout){
    
    file = directoryFile;
    tempFile = tempDirectoryFile;
    
    // gestione timeout
    struct sigaction myAction;
    myAction.sa_handler = alarmNoServer;
    if (sigemptyset(&myAction.sa_mask) < 0){
        fprintf(stderr,"sigfillset() failed");
        exit(1);
    }
    myAction.sa_flags = 0;
    if (sigaction(SIGALRM, &myAction, 0) < 0){
        fprintf(stderr,"sigaction() failed for SIGALRM");
        exit(1);
    }
    
    /* inizzializzo le varibili con pid del processo client corrente
        e il pid del processo padre con cui comunica */
    int cl_pid = getpid();
    int srv_pid = 0;
    
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0){
        perror("error in socket");
        kill(getpid(), SIGKILL);
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
        kill(getpid(), SIGKILL);
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
            kill(getpid(), SIGKILL);
        }
    } else {
        //printf(" Loss simulation\n");
    }
    
    /* dopo aver mandato la richiesta parte il timer,
        se scade il server non ha risposto in tempo oppure è offline */
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
            fflush(stdout);
            kill(getpid(), SIGKILL);
        }
    }
    
    // GET
    if (strcmp(command,"get") == 0){
        printf("\n Your file has been requested ...\n\n");
        fflush(stdout);
        
        /* cambio la gestione del sengale perchè è stato creato il file,
            se non verrà ricevuto correttamente viene eliminato */
        myAction.sa_handler = alarmNoServerGet;
        if (sigaction(SIGALRM, &myAction, 0) < 0){
            fprintf(stderr,"sigaction() failed for SIGALRM");
            exit(1);
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
            remove(tempDirectoryFile);
        } else {
            printf(" File downloaded successfully!\n");
            fflush(stdout);
            rename(tempDirectoryFile, directoryFile);
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
        
        /* chiede di fare l'upload del file, il server può rispondere:
            OK (type = 1) oppure il file esiste già (type = 0) */
        while (1){
            
            // attendo la risposta del server ..
            if (recvfrom(sockfd,
                         &requestACK,
                         sizeof(requestACK),
                         MSG_PEEK,
                         (struct sockaddr *) &srv_addr,
                         &srv_addr_size) < 0) {
                if (errno != EINTR){
                    perror("errore in recvfrom");
                    rename(tempDirectoryFile, directoryFile);
                    kill(getpid(), SIGKILL);
                }
            }
            
            // .. se l'ACK che arriva "è per me"
            if (requestACK.cl_pid == cl_pid){
                
                // azzero il timer
                alarm(0);
                
                // tolgo l'ACK dal buffer avendo usato precedentemente MSG_PEEK e lasciato disponibile
                if (recvfrom(sockfd,
                             &requestACK,
                             sizeof(requestACK),
                             0,
                             (struct sockaddr *) &srv_addr,
                             &srv_addr_size) < 0) {
                    if (errno != EINTR){
                        perror("errore in recvfrom");
                        rename(tempDirectoryFile, directoryFile);
                        kill(getpid(), SIGKILL);
                    }
                }
                
                // controllo la risposta del server
                if (requestACK.type == 1){
                    //printf(" Recieved ACK for requestPck\n");
                    break;
                } else if (requestACK.type == 0){
                    printf(" This file already exist!\n");
                    rename(tempDirectoryFile, directoryFile);
                    kill(getpid(), SIGKILL);
                }
            }
        }
        
        // prendo il pid del processo server che utilizzo per l'invio dei pacchetti
        srv_pid = requestACK.srv_pid;
        
        // se ho ricevuto l'OK procedo con l'upload
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
            rename(tempDirectoryFile, directoryFile);
        }
        if (close(fd) < 0){
            printf("close error\n");
            exit(-1);
        }
        rename(tempDirectoryFile, directoryFile);
        kill(getpid(), SIGKILL);
    }
}
