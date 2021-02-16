int getFile(int fd, int sockfd, struct sockaddr_in cli_addr, socklen_t cl_addr_len, int cl_pid, int chunkSize, int windowSize, float loss_rate, int timeout){
    
    // base rappresenta il numero di sequenza dell'ultimo pacchetto in sequenza
    int base = -2;
    int seqNumber = 0;
    
    while (1){
        
        struct segmentPacket dataPacket;
        struct ACKPacket ack;
        
        // attendo la ricezione di Pacchetti
        while ((recvfrom(sockfd,
                      &dataPacket,
                      sizeof(dataPacket),
                      MSG_PEEK,
                      (struct sockaddr *) &cli_addr,
                      &cl_addr_len)) < 0){
            if (errno != EINTR){
                fprintf(stderr,"recvfrom() failed");
                exit(1);
            } else if (errno == EINTR){
                continue;
            }
        }
        
        // se il Pacchetto ricevuto "è per me"
        if (dataPacket.cl_pid == cl_pid){
            
            // azzero il timer significa che il server è ancora online
            alarm(0);
            
            // tolgo il Pacchetto dal buffer avendo usato precedentemente MSG_PEEK e lasciato disponibile
            if ((recvfrom(sockfd,
                          &dataPacket,
                          sizeof(dataPacket),
                          0,
                          (struct sockaddr *) &cli_addr,
                          &cl_addr_len)) < 0){
                fprintf(stderr,"recvfrom() failed");
                exit(1);
            }
            
            // prendo il pid del processo server che utilizzo per l'invio degli ACK
            int srv_pid = dataPacket.srv_pid;

            // prendo il numero di sequenza del Pacchetto ricevuto
            seqNumber = dataPacket.seq_no;
            
            // se non esiste il file richiesto (type = 3) ..
            if (dataPacket.type == 3){
                printf(" File not available!\n");
                fflush(stdout);
                return 1;
                
            // .. se ricevo file vuoto (type = 2) creo l'ACK terminatore ..
            } else if (dataPacket.type == 2){
                
                printf(" Recieved an Empty file\n\n");
                base = -1;
                ack = createACKPacket(8, base, cl_pid, srv_pid);
                
            // .. se ricevo il primo pacchetto del file scrivo sul file e creo l'ACK ..
            } else if (dataPacket.seq_no == 0 && dataPacket.type == 1){
                
                //printf(" Recieved Initial Packet\n");
                write(fd, dataPacket.data, chunkSize);
                base = 0;
                ack = createACKPacket(2, base, cl_pid, srv_pid);
                
            // .. se ricevo pacchetti in sequenza scrivo sul file e creo l'ACK ..
            } else if (dataPacket.seq_no == base + 1){
                
                //printf(" Recieved Packet #%d\n", dataPacket.seq_no);
                write(fd, dataPacket.data, chunkSize);
                base = dataPacket.seq_no;
                ack = createACKPacket(2, base, cl_pid, srv_pid);
               
            // .. se ricevo pacchetti ma non in sequenza ..
            } else if (dataPacket.type == 1 && dataPacket.seq_no != base + 1){
                
                //printf(" Recieved out of sequence Packet #%d\n", dataPacket.seq_no);
                ack = createACKPacket(2, base, cl_pid, srv_pid);
            }

            // .. se ricevo l'ultimo pacchetto creo ACK creo l'ACK terminatore con type 8
            if(dataPacket.type == 4 && seqNumber == base){
                base = -1;
                ack = createACKPacket(8, base, cl_pid, srv_pid);
            }

            // invio ACK, con probabilità di perdita
            if(!is_lost(loss_rate)){
                if(base >= 0){
                    //printf(" Sending ACK #%d\n", base);
                    if (sendto(sockfd,
                               &ack,
                               sizeof(ack),
                               0,
                               (struct sockaddr *) &cli_addr,
                               sizeof(cli_addr)) != sizeof(ack)){
                        fprintf(stderr,"sendto() sent a different number of bytes than expected");
                        exit(1);
                    }
                    
                // la differenza sta solo nei printf
                } else if (base == -1) {
                    //printf(" Recieved Last Packet\n");
                    //printf(" Sending Terminal ACK\n");
                    if (sendto(sockfd,
                               &ack,
                               sizeof(ack),
                               0,
                               (struct sockaddr *) &cli_addr,
                               sizeof(cli_addr)) != sizeof(ack)){
                        fprintf(stderr,"sendto() sent a different number of bytes than expected");
                        exit(1);
                    }
                }

                // se ho ricevuto l'ultimo pacchetto posso terminare
                // (anche se perdo l'ACK il server ha il timer e terminerà comunque)
                if(dataPacket.type == 4 && base == -1){
                    return 0;
                // se ho ricevuto il file vuoto posso terminare
                } else if (dataPacket.type == 2){
                    return 0;
                }
                
            } else {
                //printf(" Loss simulation ACK #%d\n", base);
            }
        }
        // riparte il timer per controllare che il server sia ancora online
        alarm(timeout);
    }
    return 0;
}
