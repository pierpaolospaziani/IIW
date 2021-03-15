void listAlarm(int signum){
    //printf(" Timeout");
    //fflush(stdout);
}

int listFiles(int sockfd, struct sockaddr_in cli_addr, socklen_t cl_addr_len, char* directory, int chunkSize, int windowSize, float loss_rate, float timeout, int cl_pid, int srv_pid){
    
    struct sockaddr_in fool_addr;
    unsigned int fool_addr_len;
    
    struct sigaction myAction;
    myAction.sa_handler = listAlarm;
    if (sigemptyset(&myAction.sa_mask) < 0){
        fprintf(stderr,"sigfillset() failed");
        exit(1);
    }
    myAction.sa_flags = 0;
    
    if (sigaction(SIGALRM, &myAction, 0) < 0){
        fprintf(stderr,"sigaction() failed for SIGALRM");
        exit(1);
    }
    
    // accesso alla cartella server_file
    DIR *d;
    struct dirent *dir;
    d = opendir("./server_file/");
    
    int num = 0;
    
    char* list;
    list = (char *) malloc(sizeof(char));
    if(list == NULL){
        printf("Malloc error\n");
        kill(getpid(), SIGKILL);
    }
    
    char tempBuff[1024];
    int len = 0;
    
    if (d != NULL){
        
        /* mette tutta la lista di file in 'list' direttamente con il separatore
            in modo da dover mandare direttamente la stringa */
        while ((dir = readdir(d)) != NULL){
            
            if (strcmp(dir->d_name, ".") &&
                strcmp(dir->d_name, "..") &&
                strcmp(dir->d_name, ".DS_Store") &&
                strcmp(dir->d_name, "temp")){
                
                num++;
                
                memset(tempBuff, 0, sizeof(tempBuff));
                
                snprintf(tempBuff, 1024, " - %s\n", dir->d_name);
                len += strlen(tempBuff);
                
                list = (char *) realloc(list, len);
                if(list == NULL){
                    printf("Malloc error\n");
                    kill(getpid(), SIGKILL);
                }
                
                strcat(list,tempBuff);
            }
        }
        closedir(d);
        
        /* utilizzato per calcolare l'RTT */
        struct timeval stop, start;
        
        // parte il timer per non rimanere bloccati se il client è offline
        struct itimerval it_val, stopTimer;
        
        /* se non inserito viene impostato un timeout di 1 secondo per il primo pacchetto
            per poi essere calcolato e impostato con la risposta al paccehtto di richiesta */
        if (timeout == 0.0){
            gettimeofday(&start, NULL);
            it_val.it_value.tv_sec = 1;
            it_val.it_value.tv_usec = 0;
            it_val.it_interval = it_val.it_value;
        } else {
            it_val.it_value.tv_sec = (int) timeout;
            it_val.it_value.tv_usec = (int) (timeout * 1000000 - ((int) timeout) * 1000000);
            it_val.it_interval = it_val.it_value;
        }
        
        stopTimer.it_value.tv_sec = 0;
        stopTimer.it_value.tv_usec = 0;
        stopTimer.it_interval = stopTimer.it_value;
        
        if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
          perror("error calling setitimer()");
          exit(1);
        }
        
        // se non è presente alcun file
        if (num == 0){
            
            int tries = 0;
            
            struct segmentPacket dataPacket;
            dataPacket = createTerminalPacket(num, cl_pid, srv_pid);
            
            if(!is_lost(loss_rate)){
                if (sendto(sockfd,
                           &dataPacket,
                           sizeof(dataPacket),
                           0,
                           (struct sockaddr *) &cli_addr,
                           sizeof(cli_addr)) != sizeof(dataPacket)){
                    fprintf(stderr,"sendto() sent a different number of bytes than expected");
                    exit(1);
                }
            } else {
                //printf(" Loss simulation\n");
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
                    if (tries >= MAXTRIES){
                        printf(" Client is not responding, probably it's disconnected!\n");
                        kill(getpid(), SIGKILL);
                    } else {
                        tries++;
                        if(!is_lost(loss_rate)){
                            if (sendto(sockfd,
                                       &dataPacket,
                                       sizeof(dataPacket),
                                       0,
                                       (struct sockaddr *) &cli_addr,
                                       sizeof(cli_addr)) != sizeof(dataPacket)){
                                fprintf(stderr,"sendto() sent a different number of bytes than expected");
                                exit(1);
                            }
                        } else {
                            //printf(" Loss simulation\n");
                        }
                    }
                }
                // parte l'allarme dopo aver ritrasmesso
                it_val.it_value.tv_sec = (int) timeout * tries;
                it_val.it_value.tv_usec = (int) (timeout * 1000000 - ((int) timeout) * 1000000) * tries;
                it_val.it_interval = it_val.it_value;
                if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
                  perror("error calling setitimer()");
                  exit(1);
                }
            }
            
            if (ack.srv_pid == srv_pid){
                
                if (recvfrom(sockfd,
                             &ack,
                             sizeof(ack),
                             0,
                             (struct sockaddr *) &cli_addr,
                             &cl_addr_len) < 0) {
                    if (errno != EINTR){
                        fprintf(stderr,"recvfrom() failed");
                        exit(1);
                    }
                }
                
                if(ack.type == 4){
                    //printf(" Recieved ACK for Empty folder\n");
                }
            }
            
        // invio la lista
        } else {
            
            int tries = 0;
            
            int numOfSegments = strlen(list) / chunkSize;
            if (strlen(list) % chunkSize > 0){
                numOfSegments++;
            }
            
            int base = -1;
            int seqNumber = 0;
            int lastACK = 1;
            
            while(lastACK){

                while(seqNumber <= numOfSegments && (seqNumber - base) <= windowSize){
                    
                    struct segmentPacket dataPacket;

                    if(seqNumber == numOfSegments){
                        dataPacket = createTerminalPacket(seqNumber, cl_pid, srv_pid);
                        //printf(" Sending Terminal Packet\n");
                        
                    } else {
                        
                        char seg_data[chunkSize];
                        
                        memset(seg_data, 0, sizeof(seg_data));
                        
                        strncpy(seg_data, (list + seqNumber*chunkSize), chunkSize);

                        dataPacket = createDataPacket(1, seqNumber, cl_pid, srv_pid, seg_data);
                        //printf(" Sending Packet: %d\n", seqNumber);
                    }
                    
                    if(!is_lost(loss_rate)){
                        if (sendto(sockfd,
                                   &dataPacket,
                                   sizeof(dataPacket),
                                   0,
                                   (struct sockaddr *) &cli_addr,
                                   sizeof(cli_addr)) != sizeof(dataPacket)){
                            fprintf(stderr,"sendto() sent a different number of bytes than expected");
                            exit(1);
                        }
                    } else {
                        //printf(" Loss simulation Packet: %d\n", seqNumber);
                    }
                    seqNumber++;
                }
                
                //printf(" Window full: waiting for ACKs\n");
                
                struct ACKPacket ack;
                while (recvfrom(sockfd,
                                &ack,
                                sizeof(ack),
                                MSG_PEEK,
                                (struct sockaddr *) &fool_addr,
                                &fool_addr_len) < 0) {
                    if (errno == EINTR) {
                        seqNumber = base + 1;
                        
                        printf(": Resending for the %d time\n", tries+1);
                        
                        if(tries >= MAXTRIES){
                            printf(" Client is not responding, probably it's disconnected!\n");
                            kill(getpid(), SIGKILL);
                        } else {
                            
                            tries++;
                            
                            while(seqNumber <= numOfSegments && (seqNumber - base) <= windowSize){
                                struct segmentPacket dataPacket;

                                if(seqNumber == numOfSegments){
                                    dataPacket = createTerminalPacket(seqNumber, cl_pid, srv_pid);
                                    //printf(" Sending Terminal Packet\n");
                                } else {
                                    char seg_data[chunkSize];
                                    
                                    memset(seg_data, 0, sizeof(seg_data));
                                    
                                    strncpy(seg_data, (list + seqNumber*chunkSize), chunkSize);

                                    dataPacket = createDataPacket(1, seqNumber, cl_pid, srv_pid, seg_data);
                                    //printf(" Sending Packet: %d\n", seqNumber);
                                }
                                
                                if(!is_lost(loss_rate)){
                                    if (sendto(sockfd,
                                               &dataPacket,
                                               sizeof(dataPacket),
                                               0,
                                               (struct sockaddr *) &cli_addr,
                                               sizeof(cli_addr)) != sizeof(dataPacket)){
                                        fprintf(stderr,"sendto() sent a different number of bytes than expected");
                                        exit(1);
                                    }
                                } else {
                                    //printf(" Loss simulation Packet: %d\n", seqNumber);
                                }
                                seqNumber++;
                            }
                            // parte l'allarme dopo aver ritrasmesso
                            if (setitimer(ITIMER_REAL, &stopTimer, NULL) == -1) {
                              perror("error calling setitimer()");
                              exit(1);
                            }
                            it_val.it_value.tv_sec = (int) timeout * tries;
                            it_val.it_value.tv_usec = (int) (timeout * 1000000 - ((int) timeout) * 1000000) * tries;
                            it_val.it_interval = it_val.it_value;
                            if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
                              perror("error calling setitimer()");
                              exit(1);
                            }
                        }
                    } else {
                        fprintf(stderr,"recvfrom() failed");
                        exit(1);
                    }
                }
                
                if (ack.srv_pid == srv_pid){
                    
                    if (recvfrom(sockfd,
                                 &ack,
                                 sizeof(ack),
                                 0,
                                 (struct sockaddr *) &cli_addr,
                                 &cl_addr_len) < 0) {
                        if (errno != EINTR){
                            fprintf(stderr,"recvfrom() failed");
                            exit(1);
                        }
                    }
                
                    if (timeout == 0 && ack.ack_no == 0){
                        gettimeofday(&stop, NULL);
                        timeout = (float) ((stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec)/1000000;
                        timeout = 3 * timeout;
                    }
                    
                    if(ack.type != 4){
                        //printf(" Recieved ACK: %d\n", ack.ack_no);
                        if(ack.ack_no > base){
                            base = ack.ack_no;
                            if (setitimer(ITIMER_REAL, &stopTimer, NULL) == -1) {
                              perror("error calling setitimer()");
                              exit(1);
                            }
                            it_val.it_value.tv_sec = (int) timeout;
                            it_val.it_value.tv_usec = (int) (timeout * 1000000 - ((int) timeout) * 1000000);
                            it_val.it_interval = it_val.it_value;
                            if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
                              perror("error calling setitimer()");
                              exit(1);
                            }
                        }
                    } else {
                        //printf(" Recieved Terminal ACK\n");
                        lastACK = 0;
                    }
                    tries = 0;
                }
            }
        }
    }
    close(sockfd);
    return 0;
}
