// Non ho commentato il codice essendo molto simile a 'client_get' che è stato commentato.

int listFiles(int sockfd, struct sockaddr_in srv_addr, socklen_t srv_addr_len, int cl_pid, int chunkSize, int windowSize, float loss_rate, int timeout){
    
    int base = -2;
    int seqNumber = 0;
    int lastACKed = -1;
    
    while (1){
     
        struct segmentPacket dataPacket;
        struct ACKPacket ack;
        
        while ((recvfrom(sockfd,
                      &dataPacket,
                      sizeof(dataPacket),
                      MSG_PEEK,
                      (struct sockaddr *) &srv_addr,
                      &srv_addr_len)) < 0){
            if (errno != EINTR){
                fprintf(stderr,"recvfrom() failed");
                exit(1);
            } else if (errno == EINTR){
                continue;
            }
        }
        
        if (dataPacket.cl_pid == cl_pid){
            
            alarm(0);
            
            if ((recvfrom(sockfd,
                          &dataPacket,
                          sizeof(dataPacket),
                          0,
                          (struct sockaddr *) &srv_addr,
                          &srv_addr_len)) < 0){
                fprintf(stderr,"recvfrom() failed");
                exit(1);
            }
            
            int srv_pid = dataPacket.srv_pid;

            seqNumber = dataPacket.seq_no;
            
            if(dataPacket.seq_no == 0 && dataPacket.type == 1){
                
                if (dataPacket.seq_no > lastACKed){
                    printf("%s", dataPacket.data);
                    fflush(stdout);
                    lastACKed = dataPacket.seq_no;
                }
                
                base = 0;
                ack = createACKPacket(2, base, cl_pid, srv_pid);
                
            } else if (dataPacket.seq_no == base + 1){
                
                if (dataPacket.seq_no > lastACKed){
                    printf("%s", dataPacket.data);
                    fflush(stdout);
                    lastACKed = dataPacket.seq_no;
                }
                
                base = dataPacket.seq_no;
                ack = createACKPacket(2, base, cl_pid, srv_pid);
                
            } else if (dataPacket.type == 1 && dataPacket.seq_no != base + 1){
                //printf(" Recieved out of sequence Packet #%d\n", dataPacket.seq_no);
                ack = createACKPacket(2, base, cl_pid, srv_pid);
            }

            if(dataPacket.type == 4 && seqNumber == base ){
                
                base = -1;
                ack = createACKPacket(8, base, cl_pid, srv_pid);
                
            } else if (dataPacket.type == 4 && dataPacket.seq_no == 0){
                
                printf(" Server hasn't available files!\n");
                base = -1;
                ack = createACKPacket(8, base, cl_pid, srv_pid);
                
            }

            if(!is_lost(loss_rate)){
                if(base >= 0){
                    //printf("\n Sending ACK #%d\n", base);
                    if (sendto(sockfd,
                               &ack,
                               sizeof(ack), 0,
                               (struct sockaddr *) &srv_addr,
                               sizeof(srv_addr)) != sizeof(ack)){
                        fprintf(stderr,"sendto() sent a different number of bytes than expected");
                        exit(1);
                    }
                } else if (base == -1) {
                    //printf("\n Sending Terminal ACK\n");
                    if (sendto(sockfd,
                               &ack,
                               sizeof(ack),
                               0,
                               (struct sockaddr *) &srv_addr,
                               sizeof(srv_addr)) != sizeof(ack)){
                        fprintf(stderr,"sendto() sent a different number of bytes than expected");
                        exit(1);
                    }
                }

                if(dataPacket.type == 4 && base == -1){
                    return 0;
                }
            } else {
                //printf(" Loss simulation ACK #%d\n", base);
            }
        }
        alarm(timeout);
    }
    return 0;
}
