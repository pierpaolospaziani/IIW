#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>

#define DATALIMIT 511

#define commandsString "\n  -      list       : returns a list of available files\n\n  - get 'file_name' : download a file from the server\n\n  - put 'file_name' : upload a file to the server\n\n"
#define directory "./client_file/"
#define tempDirectory "./client_file/temp/"

#include "./client_folder/client_child.h"

int commandCheck(char* inputString, char* command){
    
    if (inputString[0] == 'l' &&
        inputString[1] == 'i' &&
        inputString[2] == 's' &&
        inputString[3] == 't'){
        
        strcpy(command,"list");
        return 1;
        
    } else if (inputString[0] == 'g' &&
                inputString[1] == 'e' &&
                inputString[2] == 't' &&
                inputString[3] == ' '){
        
        strcpy(command,"get");
        return 2;
        
    } else if (inputString[0] == 'p' &&
                inputString[1] == 'u' &&
                inputString[2] == 't' &&
                inputString[3] == ' '){
        
        strcpy(command,"put");
        return 3;
    }
    return 0;
}

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
        printf("\n\n Type one of the 3 commands:\n\n> ");
        fflush(stdout);
    }
    return;
}

int main(int argc, char *argv[]) {
    
    if (argc < 3 || argc > 5){
        fprintf(stderr,"\n Usage: %s <Chunk Size> <Window Size> <Loss Rate> <Timer>\n Loss Rate and Timer are optional, if not specified are set to 0 and 1.\n You gave %d Argument/s.\n\n", argv[0], argc);
        exit(1);
    }
    
    srand48(2345);
    
    if (signal(SIGCHLD, sig_chld_handler) == SIG_ERR) {
        fprintf(stderr, "error in signal");
        exit(1);
    }
    
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
    
    if (argc >= 4){
        loss_rate = atof(argv[3]);
        if(loss_rate > 1){
            fprintf(stderr, "Error: Loss Rate must be < 1\n");
            exit(1);
        }
    }
    
    int userTimer = 0;
    
    if (argc == 5){
        userTimer = atoi(argv[4]) * 1000;
        if(userTimer < 1){
            fprintf(stderr, "Error: Timer must be > 1\n");
            exit(1);
        }
    }
    
    printf("\n ----------------------- Welcome! -----------------------\n\n Available commands:\n");
    fflush(stdout);
    printf(commandsString);
    fflush(stdout);
    printf(" --------------------------------------------------------\n");
    fflush(stdout);
    
    char command[1024];
    int fd;
    
redo:
    
    printf("\n Type one of the 3 commands:\n\n> ");
    fflush(stdout);
    
    while (1) {
        
        char inputString[1024];
        fgets(inputString, 1024, stdin);
        fflush(stdin);
        
        int res = commandCheck(inputString,command);
        
        char fileName[strlen(inputString) - 4];
        memset(fileName, 0, sizeof(fileName));
        
        strncpy(fileName, inputString + 4, strlen(inputString) - 5);
        
        char directoryFile[strlen(directory) + strlen(fileName) + 1];
        
        snprintf(directoryFile, sizeof directoryFile, "%s%s", directory, fileName);
        
        char tempDirectoryFile[strlen(tempDirectory) + strlen(fileName) + 1];
        memset(tempDirectoryFile, 0, sizeof(tempDirectoryFile));
        snprintf(tempDirectoryFile, sizeof tempDirectoryFile, "%s%s", tempDirectory, fileName);
        
        // LIST
        if (res == 1){
        
        // GET
        } else if (res == 2){
            
            // se il file esiste già
            if(access(directoryFile, F_OK) == 0) {
redo1:
                printf("\n This file already exist! Do you want to overwrite it? [Y or N]\n> ");
                fflush(stdout);
                
                char s[sizeof(char)];
                
                scanf("%s", s);
                fflush(stdin);
                
                // se si vuole sovrascrivere il file esistente
                if (strcmp(s,"Y") == 0 || strcmp(s,"y") == 0){
                    printf("\n Ok, the file will be overwritten!\n");
                    fflush(stdout);
                    
                    remove(directoryFile);
                    
                    fd = open(tempDirectoryFile, O_WRONLY | O_CREAT, 0666);
                    
                    if (fd == -1){
                        printf(" File open error, try again..\n");
                        fflush(stdout);
                        
                        goto redo;
                    }
                
                // se NON si vuole sovrascrivere il file esistente
                } else if (strcmp(s,"N") == 0 || strcmp(s,"n") == 0){
                    printf("\n Ok, the file will not be overwritten!\n Try changing the file name on your computer and try again.\n");
                    fflush(stdout);
                    
                    goto redo;
                    
                // se si scrive altro
                } else {
                    printf("\n Sorry, uninterpreted input, try again..\n");
                    fflush(stdout);
                    
                    goto redo1;
                }
                
            // se il file NON esiste ancora
            } else if (access(tempDirectoryFile, F_OK) == 0){
                
                printf("\n This file already exist!\n Unluckily you cannot overwrite it now!\n It is already in download or it is being uploaded to the server.\n If you want overwrite it try later.\n");
                fflush(stdout);
                
                goto redo;
                
            } else {
                
                fd = open(tempDirectoryFile, O_WRONLY | O_CREAT, 0666);
                
                if (fd == -1){
                    printf(" File open error, try again..\n");
                    fflush(stdout);
                    
                    goto redo;
                }
            }
            
        // PUT
        } else if (res == 3) {
            
            // se il file esiste
            if(access(directoryFile, F_OK) == 0) {
                
                rename(directoryFile, tempDirectoryFile);
                
                fd = open(tempDirectoryFile, O_RDONLY);
                
                if (fd == -1){
                    printf(" File open error, try again..\n");
                    fflush(stdout);
                    
                    goto redo;
                }
                
            // se il file NON esiste
            } else {
                printf("\n Sorry, file not found, try again..\n");
                fflush(stdout);
                
                goto redo;
            }
            
        // comando sbagliato
        } else {
            printf("\n Sorry, the only commands allowed are:\n");
            fflush(stdout);
            printf(commandsString);
            fflush(stdout);
            
            goto redo;
        }
        
        
        pid_t pid;
        
        if ((pid = fork()) == 0) {
            childFunc(inputString,
                      command,
                      directoryFile,
                      tempDirectoryFile,
                      fd,
                      chunkSize,
                      windowSize,
                      loss_rate,
                      userTimer);
        }
    }
    return 0;
}

