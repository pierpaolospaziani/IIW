void putAlarm(int signum){
    printf(" Timeout");
}

int putFile(int fd, int sockfd, struct sockaddr_in cl_addr, struct sockaddr_in srv_addr, unsigned int srv_addr_size, int chunkSize, int windowSize, int cl_pid, int srv_pid, float loss_rate, int timeout){
    
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

        // parte il timer riempita la finestra oppure inviato l'ultimo pacchetto
        alarm(timeout);
        
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

                printf(": Resending for the %d time\n", tries+1);
                if(tries >= MAXTRIES){
                    printf(" Server is not responding anymore: Upload failed!\n");
                    kill(getpid(), SIGKILL);
                } else {
                    alarm(0);
                    
                    // RITRASMISSIONE
                    
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
                        alarm(timeout);
                    }
                }
                // aumento il numero dei tentativi di ritrasmissione
                tries++;
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
            if(ack.type != 8){
                //printf(" Recieved ACK: %d\n", ack.ack_no);
                
                // .. controllo se è quello che aspettavo ..
                if(ack.ack_no > base){
                    base = ack.ack_no;
                }
                
            // .. altrimenti se è l'ultimo (type 8), interrompo il ciclo e termino
            } else {
                //printf(" Recieved Terminal ACK\n");
                lastACK = 0;
            }

            /* quando ricevo un ACK, sia in sequenza che non, azzero il timer e
             i tentativi di ritrasmissione, significa che il server è ancora online*/
            alarm(0);
            tries = 0;
        }
    }
    close(sockfd);
    return 0;
}
