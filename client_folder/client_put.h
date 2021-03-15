void putAlarm(int signum){
    //printf(" Timeout");
    //fflush(stdout);
}

int putFile(int fd, int sockfd, struct sockaddr_in cl_addr, struct sockaddr_in srv_addr, unsigned int srv_addr_size, int chunkSize, int windowSize, int cl_pid, int srv_pid, float loss_rate, float timeout){
    
    struct timeval stop, start;
    gettimeofday(&start, NULL);
    
    // gestione timeout
    struct sigaction myAction;
    myAction.sa_handler = putAlarm;
    if (sigemptyset(&myAction.sa_mask) < 0){
        fprintf(stderr,"sigfillset() failed");
        exit(1);
    }
    myAction.sa_flags = 0;
    if (sigaction(SIGALRM, &myAction, 0) < 0){
        fprintf(stderr,"sigaction() failed for SIGALRM");
        exit(1);
    }
    
    // setup del timer, se adattivo è già stato calcolato in client_child
    struct itimerval it_val, stopTimer;
    it_val.it_value.tv_sec = (int) timeout;
    it_val.it_value.tv_usec = (int) (timeout * 1000000 - ((int) timeout) * 1000000);
    it_val.it_interval = it_val.it_value;
    
    stopTimer.it_value.tv_sec = 0;
    stopTimer.it_value.tv_usec = 0;
    stopTimer.it_interval = stopTimer.it_value;
    
    // numero di ritrasmissioni consecutive
    int tries = 0;
    
    // calcolo del numero dei segmenti di file
    int dataBufferSize = lseek(fd, 0L, SEEK_END);
    lseek(fd, 0L, SEEK_SET);
    int numOfSegments = dataBufferSize / chunkSize;
    if (dataBufferSize % chunkSize > 0){
        numOfSegments++;
    }
    
    // base rappresenta l'ultimo pacchetto di cui abbiamo ricevuto l'ACK
    int base = -1;
    int seqNumber = 0;
    
    // ciclo finchè non riceviamo l'ultimo ACK
    int lastACK = 1;

    // parte il timer per non rimanere bloccati se il server è offline
    if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
      perror("error calling setitimer()");
      exit(1);
    }
    
    while(lastACK){
        
        // FILE VUOTO
        if (dataBufferSize == 0){
            struct segmentPacket dataPacket;
            char seg_data[chunkSize];
            memset(seg_data, 0, sizeof(seg_data));
            dataPacket = createDataPacket(2, seqNumber, cl_pid, srv_pid, seg_data);
            //printf("\n Empty file\n\n Sending Terminal Packet\n");
            
            // invio del paccheto con type = 2, con probabilità di perdita
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
                //printf(" Loss simulation\n");
            }
            
        // FILE NON VUOTO
        } else {
            
            // trasmissione paccheti finchè c'è spazio nella finestra oppure fino a terminare
            while(seqNumber <= numOfSegments && (seqNumber - base) <= windowSize){
                
                // posizionamento sul file nel punto giusto
                lseek(fd, seqNumber * chunkSize, SEEK_SET);
                
                struct segmentPacket dataPacket;

                // se tutto il file è stato inviato, invia il terminatore ..
                if(seqNumber == numOfSegments){
                    dataPacket = createTerminalPacket(seqNumber, cl_pid, srv_pid);
                    //printf(" Sending Terminal Packet\n");
                    
                // .. altrimenti legge il file e crea il pacchetto
                } else {
                    
                    char seg_data[chunkSize];
                    memset(seg_data, 0, sizeof(seg_data));
                    read(fd,seg_data,chunkSize);
                    dataPacket = createDataPacket(1, seqNumber, cl_pid, srv_pid, seg_data);
                    //printf(" Sending Packet: %d\n", seqNumber);
                }

                // invio del paccheto con type = 1, con probabilità di perdita
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

        // attendo la ricezione di ACK
        struct ACKPacket ack;
        while (recvfrom(sockfd,
                        &ack,
                        sizeof(ack),
                        MSG_PEEK,
                        (struct sockaddr *) &srv_addr,
                        &srv_addr_size) < 0) {
            
            // se scade il timer:
            if (errno == EINTR) {
                
                // riprende dal pacchetto successivo all'ultimo di cui ha ricevuto ACK
                seqNumber = base + 1;

                //printf(": Resending for the %d time\n", tries+1);
                //fflush(stdout);
                if(tries >= MAXTRIES){
                    printf(" Server is not responding anymore: Upload failed!\n");
                    return 1;
                } else {
                    // RITRASMISSIONE
                    
                    // aumento il numero dei tentativi di ritrasmissione
                    tries++;
                    
                    if (dataBufferSize == 0){
                    
                        char seg_data[chunkSize];
                        memset(seg_data, 0, sizeof(seg_data));
                        struct segmentPacket dataPacket;
                        dataPacket = createDataPacket(2, seqNumber, cl_pid, srv_pid, seg_data);
                        //printf(" Empty file\n Sending Terminal Packet\n");
                        
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
                }
            } else {
                fprintf(stderr,"recvfrom() failed");
                exit(1);
            }
        }
        
        // se l'ACK ricevuto "è per me"
        if (ack.cl_pid == cl_pid){
            
            // tolgo l'ACK dal buffer avendo usato precedentemente MSG_PEEK e lasciato disponibile
            if (recvfrom(sockfd,
                         &ack,
                         sizeof(ack),
                         0,
                         (struct sockaddr *) &srv_addr,
                         &srv_addr_size) < 0) {
                if (errno != EINTR){
                    fprintf(stderr,"recvfrom() failed");
                    exit(1);
                }
            }
            
            // se NON è l'ultimo ..
            if(ack.type != 4){
                //printf(" Recieved ACK: %d\n", ack.ack_no);
                //fflush(stdout);
                
                /* .. controllo se è quello che aspettavo, nel caso azzero il timer e
                 aggiorno l'ultimo pacchetto di cui ho ricevuto ACK .. */
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
                
            // .. altrimenti se è l'ultimo (type 4), interrompo il ciclo, invio la conferma e termino
            } else {
                //printf(" Recieved Terminal ACK\n");
                lastACK = 0;
                struct segmentPacket dataPacket;
                dataPacket = createTerminalConfirmPacket(seqNumber, cl_pid, srv_pid);
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
            }

            /* quando ricevo un ACK, sia in sequenza che non, azzero i tentativi
             di ritrasmissione, significa che il server è ancora online */
            tries = 0;
        }
    }
    gettimeofday(&stop, NULL);
    //printf(" Took %f seconds\n", (float) ((stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec)/1000000);
    
    close(sockfd);
    return 0;
}
