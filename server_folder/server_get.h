void getAlarm(int signum){
    //printf(" Timeout");
    //fflush(stdout);
}

int getFile(int fd, int sockfd, struct sockaddr_in cl_addr, unsigned int cl_addr_len, int chunkSize, int windowSize, int cl_pid, int srv_pid, float loss_rate, float timeout){
    
    struct sockaddr_in fool_addr;
    unsigned int fool_addr_len;
    
    struct sigaction myAction;
    myAction.sa_handler = getAlarm;
    if (sigemptyset(&myAction.sa_mask) < 0){
        fprintf(stderr,"sigfillset() failed");
        exit(1);
    }
    myAction.sa_flags = 0;
    
    if (sigaction(SIGALRM, &myAction, 0) < 0){
        fprintf(stderr,"sigaction() failed for SIGALRM");
        exit(1);
    }
    
    int tries = 0;
    
    int dataBufferSize = lseek(fd, 0L, SEEK_END);
    lseek(fd, 0L, SEEK_SET);
    int numOfSegments = dataBufferSize / chunkSize;
    if (dataBufferSize % chunkSize > 0){
        numOfSegments++;
    }
    
    int base = -1;
    int seqNumber = 0;
    
    int lastACK = 1;
    
    struct timeval stop, start;
    struct itimerval it_val, stopTimer;
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
    
    while(lastACK){
        
        if (dataBufferSize == 0){
            struct segmentPacket dataPacket;
            char seg_data[chunkSize];
            memset(seg_data, 0, sizeof(seg_data));
            dataPacket = createDataPacket(2, seqNumber, cl_pid, srv_pid, seg_data);
            //printf(" Empty file\nSending Terminal Packet\n");
            
            if(!is_lost(loss_rate)){
                if (sendto(sockfd,
                           &dataPacket,
                           sizeof(dataPacket),
                           0,
                           (struct sockaddr *) &cl_addr,
                           sizeof(cl_addr)) != sizeof(dataPacket)){
                    fprintf(stderr,"sendto() sent a different number of bytes than expected");
                    exit(1);
                }
            } else {
                //printf(" Loss simulation Packet: %d\n", seqNumber);
            }
        } else {
            
            while(seqNumber <= numOfSegments && (seqNumber - base) <= windowSize){
                
                lseek(fd, seqNumber * chunkSize, SEEK_SET);
                
                struct segmentPacket dataPacket;

                if(seqNumber == numOfSegments){
                    dataPacket = createTerminalPacket(seqNumber, cl_pid, srv_pid);
                    //printf(" Sending Terminal Packet\n");
                    
                } else {
                    char seg_data[chunkSize];
                    
                    memset(seg_data, 0, sizeof(seg_data));
                    
                    read(fd,seg_data,chunkSize);

                    dataPacket = createDataPacket(1, seqNumber, cl_pid, srv_pid, seg_data);
                    //printf(" Sending Packet: %d\n", seqNumber);
                }

                if(!is_lost(loss_rate)){
                    if (sendto(sockfd,
                               &dataPacket,
                               sizeof(dataPacket),
                               0,
                               (struct sockaddr *) &cl_addr,
                               sizeof(cl_addr)) != sizeof(dataPacket)){
                        fprintf(stderr,"sendto() sent a different number of bytes than expected");
                        exit(1);
                    }
                } else {
                    //printf(" Loss simulation Packet: %d\n", seqNumber);
                }
                seqNumber++;
            }
        }

        //printf(" Window full\n");

        struct ACKPacket ack;
        while (recvfrom(sockfd,
                        &ack,
                        sizeof(ack),
                        MSG_PEEK,
                        (struct sockaddr *) &fool_addr,
                        &fool_addr_len) < 0) {
            if (errno == EINTR) {
                
                seqNumber = base + 1;
                
                //printf(": Resending for the %d time\n", tries);
                
                if(tries >= MAXTRIES){
                    printf(" Client is not responding, probably it's disconnected!\n");
                    kill(getpid(), SIGKILL);
                } else {
                    
                    tries++;
                    
                    if (dataBufferSize == 0){
                    
                        char seg_data[chunkSize];
                        memset(seg_data, 0, sizeof(seg_data));
                        struct segmentPacket dataPacket;
                        dataPacket = createDataPacket(2, seqNumber, cl_pid, srv_pid, seg_data);
                        //printf(" Empty file\nSending Terminal Packet\n");
                        if(!is_lost(loss_rate)){
                            if (sendto(sockfd,
                                       &dataPacket,
                                       sizeof(dataPacket),
                                       0,
                                       (struct sockaddr *) &cl_addr,
                                       sizeof(cl_addr)) != sizeof(dataPacket)){
                                fprintf(stderr,"sendto() sent a different number of bytes than expected");
                                exit(1);
                            }
                        } else {
                            //printf(" Loss simulation Packet: %d\n", seqNumber);
                        }
                    } else {
                        
                        lseek(fd, seqNumber * chunkSize, SEEK_SET);
                        
                        while(seqNumber <= numOfSegments && (seqNumber - base) <= windowSize){
                            struct segmentPacket dataPacket;

                            if(seqNumber == numOfSegments){
                                dataPacket = createTerminalPacket(seqNumber, cl_pid, srv_pid);
                                //printf(" Sending Terminal Packet\n");
                            } else {
                                char seg_data[chunkSize];
                                
                                memset(seg_data, 0, sizeof(seg_data));
                                
                                read(fd,seg_data,chunkSize);

                                dataPacket = createDataPacket(1, seqNumber, cl_pid, srv_pid, seg_data);
                                //printf(" Sending Packet: %d\n", seqNumber);
                            }

                            if(!is_lost(loss_rate)){
                                if (sendto(sockfd,
                                           &dataPacket,
                                           sizeof(dataPacket),
                                           0,
                                           (struct sockaddr *) &cl_addr,
                                           sizeof(cl_addr)) != sizeof(dataPacket)){
                                    fprintf(stderr,"sendto() sent a different number of bytes than expected");
                                    exit(1);
                                }
                            } else {
                                //printf(" Loss simulation Packet: %d\n", seqNumber);
                            }
                            seqNumber++;
                        }
                        it_val.it_value.tv_sec = (int) timeout * tries;
                        it_val.it_value.tv_usec = (int) (timeout * 1000000 - ((int) timeout) * 1000000) * tries;
                        it_val.it_interval = it_val.it_value;
                        if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
                          perror("error calling setitimer()");
                          exit(1);
                        }
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
                         (struct sockaddr *) &cl_addr,
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
    
    close(sockfd);
    return 0;
}
