void getAlarm(int ignored){
    printf(" Timeout");
}

int getFile(int fd, int sockfd, struct sockaddr_in cl_addr, unsigned int cl_addr_len, int chunkSize, int windowSize, int cl_pid, int srv_pid, float loss_rate, int timeout){
    
    struct sigaction myAction;
    myAction.sa_handler = getAlarm;
    if (sigemptyset(&myAction.sa_mask) < 0){
        DieWithError("sigfillset() failed");
    }
    myAction.sa_flags = 0;
    
    if (sigaction(SIGALRM, &myAction, 0) < 0){
        DieWithError("sigaction() failed for SIGALRM");
    }
    
    int tries = 0;
    
    /* Calculate number of Segments */
    int dataBufferSize = lseek(fd, 0L, SEEK_END);
    lseek(fd, 0L, SEEK_SET);
    int numOfSegments = dataBufferSize / chunkSize;
    
    /* Might have left overs */
    if (dataBufferSize % chunkSize > 0){
        numOfSegments++;
    }
    
    /* Set seqNumber, base and ACK to 0 */
    int base = -1;
    int seqNumber = 0;
    
    int lastACK = 1;
    
    while(lastACK){
        
        if (dataBufferSize == 0){
            struct segmentPacket dataPacket;
            char seg_data[chunkSize];
            memset(seg_data, 0, sizeof(seg_data));
            dataPacket = createDataPacket(2, seqNumber, cl_pid, srv_pid, seg_data);
            printf("Empty file\nSending Terminal Packet\n");
            
            if(!is_lost(loss_rate)){
                if (sendto(sockfd,
                           &dataPacket,
                           sizeof(dataPacket),
                           0,
                           (struct sockaddr *) &cl_addr,
                           sizeof(cl_addr)) != sizeof(dataPacket)){
                    DieWithError("sendto() sent a different number of bytes than expected");
                }
            } else {
                printf("SIMULATED LOSE Packet: %d\n", seqNumber);
            }
        } else {
            
            /* Send chunks from base up to window size */
            while(seqNumber <= numOfSegments && (seqNumber - base) <= windowSize){
                
                lseek(fd, seqNumber * chunkSize, SEEK_SET);
                
                struct segmentPacket dataPacket;

                if(seqNumber == numOfSegments){
                    /* Reached end, create terminal packet */
                    dataPacket = createTerminalPacket(seqNumber, cl_pid, srv_pid);
                    printf("Sending Terminal Packet\n");
                    
                } else {
                    /* Create FIRST Data Packet Struct */
                    char seg_data[chunkSize];
                    
                    memset(seg_data, 0, sizeof(seg_data));
                    
                    read(fd,seg_data,chunkSize);

                    dataPacket = createDataPacket(1, seqNumber, cl_pid, srv_pid, seg_data);
                    printf("Sending Packet: %d\n", seqNumber);
                }

                /* Send the constructed data packet to the receiver */
                if(!is_lost(loss_rate)){
                    if (sendto(sockfd,
                               &dataPacket,
                               sizeof(dataPacket),
                               0,
                               (struct sockaddr *) &cl_addr,
                               sizeof(cl_addr)) != sizeof(dataPacket)){
                        DieWithError("sendto() sent a different number of bytes than expected");
                    }
                } else {
                    printf("SIMULATED LOSE Packet: %d\n", seqNumber);
                }
                seqNumber++;
            }
        }

        /* Set Timer */
        alarm(timeout);        /* Set the timeout */

        /* IF window is full alert that it is waiting */
        printf("Window full: waiting for ACKs\n");

        /* Listen for ACKs, get highest ACK, reset base */
        struct ACKPacket ack;
        while (recvfrom(sockfd,
                        &ack,
                        sizeof(ack),
                        MSG_PEEK,
                        (struct sockaddr *) &cl_addr,
                        &cl_addr_len) < 0) {
            if (errno == EINTR) {
            /* Alarm went off  */
                
                /* reset the seqNumber back to one ahead of the last recieved ACK */
                seqNumber = base + 1;
                
                printf(": Resending for the %d time\n", tries);
                
                if(tries >= MAXTRIES){
                    printf(" Client is not responding, probably it's disconnected!\n");
                    exit(1);
                } else {
                    alarm(0);
                    
                    // RITRASMISSIONE
                    
                    if (dataBufferSize == 0){
                    
                        char seg_data[chunkSize];
                        memset(seg_data, 0, sizeof(seg_data));
                        struct segmentPacket dataPacket;
                        dataPacket = createDataPacket(2, seqNumber, cl_pid, srv_pid, seg_data);
                        printf("Empty file\nSending Terminal Packet\n");
                        if(!is_lost(loss_rate)){
                            if (sendto(sockfd,
                                       &dataPacket,
                                       sizeof(dataPacket),
                                       0,
                                       (struct sockaddr *) &cl_addr,
                                       sizeof(cl_addr)) != sizeof(dataPacket)){
                                DieWithError("sendto() sent a different number of bytes than expected");
                            }
                        } else {
                            printf("SIMULATED LOSE Packet: %d\n", seqNumber);
                        }
                    } else {
                        
                        lseek(fd, seqNumber * chunkSize, SEEK_SET);
                        
                        while(seqNumber <= numOfSegments && (seqNumber - base) <= windowSize){
                            struct segmentPacket dataPacket;

                            if(seqNumber == numOfSegments){
                                /* Reached end, create terminal packet */
                                dataPacket = createTerminalPacket(seqNumber, cl_pid, srv_pid);
                                printf("Sending Terminal Packet\n");
                            } else {
                                /* Create Data Packet Struct */
                                char seg_data[chunkSize];
                                
                                memset(seg_data, 0, sizeof(seg_data));
                                
                                read(fd,seg_data,chunkSize);

                                dataPacket = createDataPacket(1, seqNumber, cl_pid, srv_pid, seg_data);
                                printf("Sending Packet: %d\n", seqNumber);
                            }

                            /* Send the constructed data packet to the receiver */
                            if(!is_lost(loss_rate)){
                                if (sendto(sockfd,
                                           &dataPacket,
                                           sizeof(dataPacket),
                                           0,
                                           (struct sockaddr *) &cl_addr,
                                           sizeof(cl_addr)) != sizeof(dataPacket)){
                                    DieWithError("sendto() sent a different number of bytes than expected");
                                }
                            } else {
                                printf("SIMULATED LOSE Packet: %d\n", seqNumber);
                            }
                            seqNumber++;
                        }
                        alarm(timeout);
                    }
                }
                tries++;
            } else {
                DieWithError("recvfrom() failed");
            }
        }
        
        if (ack.srv_pid == srv_pid){
            
            //cl_pid = ack.cl_pid;
            
            if (recvfrom(sockfd,
                         &ack,
                         sizeof(ack),
                         0,
                         (struct sockaddr *) &cl_addr,
                         &cl_addr_len) < 0) {
                if (errno != EINTR){
                    DieWithError("recvfrom() failed");
                }
            }
            
            /* 8 is teardown ack */
            if(ack.type != 8){
                printf("----------------------- Recieved ACK: %d\n", ack.ack_no);
                if(ack.ack_no > base){
                    /* Advances the sending, reset tries */
                    base = ack.ack_no;
                }
            } else {
                printf("----------------------- Recieved Terminal ACK\n");
                lastACK = 0;
            }

            /* recvfrom() got something --  cancel the timeout, reset tries */
            alarm(0);
            tries = 0;
        }
    }
    
    close(sockfd);
    return 0;
}
