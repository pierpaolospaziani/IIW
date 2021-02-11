int putFile(int fd, int sockfd, struct sockaddr_in cli_addr, socklen_t cl_addr_len, int cl_pid, int srv_pid, int chunkSize, int windowSize, float loss_rate){
    
    printf("------------------------------------  Sending requestACK\n");
    struct ACKPacket requestAck;
    requestAck = createACKPacket(1, 0, cl_pid, srv_pid);
    // invio ACK con type 1
    if (sendto(sockfd,
               &requestAck,
               sizeof(requestAck),
               0,
               (struct sockaddr*)&cli_addr,
               cl_addr_len) < 0) {
        perror("errore in sendto");
        exit(1);
    }
    
    int base = -2;
    int seqNumber = 0;
    
    while (1){
     
        struct segmentPacket dataPacket;

        struct ACKPacket ack;

        if ((recvfrom(sockfd,
                      &dataPacket,
                      sizeof(dataPacket),
                      MSG_PEEK,
                      (struct sockaddr *) &cli_addr,
                      &cl_addr_len)) < 0){
            DieWithError("recvfrom() failed");
        }
        
        if (dataPacket.srv_pid == srv_pid){
            
            if ((recvfrom(sockfd,
                          &dataPacket,
                          sizeof(dataPacket),
                          0,
                          (struct sockaddr *) &cli_addr,
                          &cl_addr_len)) < 0){
                DieWithError("recvfrom() failed");
            }

            seqNumber = dataPacket.seq_no;
            
            if (dataPacket.type == 2){
                
                printf("Recieved an Empty file\n");
                base = -1;
                ack = createACKPacket(8, base, cl_pid, srv_pid);
                
            } else if (dataPacket.seq_no == 0 && dataPacket.type == 1){
                
                printf("Recieved Initial Packet\n");
                
                write(fd, dataPacket.data, chunkSize);
                
                base = 0;
                ack = createACKPacket(2, base, cl_pid, srv_pid);
                
            } else if (dataPacket.seq_no == base + 1){
                /* if base+1 then its a subsequent in order packet */
            
                /* then concatinate the data sent to the recieving buffer */
                printf("Recieved Subseqent Packet #%d\n", dataPacket.seq_no);
                fflush(stdout);
                
                write(fd, dataPacket.data, chunkSize);
                
                base = dataPacket.seq_no;
                ack = createACKPacket(2, base, cl_pid, srv_pid);
                
            } else if (dataPacket.type == 1 && dataPacket.seq_no != base + 1){
                
                /* if recieved out of sync packet, send ACK with old base */
                printf("Recieved Out of Sync Packet #%d\n", dataPacket.seq_no);
                /* Resend ACK with old base */
                ack = createACKPacket(2, base, cl_pid, srv_pid);
            }

            /* type 4 means that the packet recieved is a termination packet */
            if(dataPacket.type == 4 && seqNumber == base ){
                base = -1;
                /* create an ACK packet with terminal type 8 */
                ack = createACKPacket(8, base, cl_pid, srv_pid);
            }

            /* Add random packet lose, if lost dont process */
            if(!is_lost(loss_rate)){

                /* Send ACK for Packet Recieved */
                if(base >= 0){
                    printf("------------------------------------  Sending ACK #%d\n", base);
                    if (sendto(sockfd,
                               &ack,
                               sizeof(ack), 0,
                               (struct sockaddr *) &cli_addr,
                               sizeof(cli_addr)) != sizeof(ack)){
                        DieWithError("sendto() sent a different number of bytes than expected");
                    }
                } else if (base == -1) {
                    printf("Recieved Teardown Packet\n");
                    printf("------------------------------------  Sending Terminal ACK\n");
                    if (sendto(sockfd,
                               &ack,
                               sizeof(ack),
                               0,
                               (struct sockaddr *) &cli_addr,
                               sizeof(cli_addr)) != sizeof(ack)){
                        DieWithError("sendto() sent a different number of bytes than expected");
                    }
                }

                /* if data packet is terminal packet, display and clear the recieved message */
                if(dataPacket.type == 4 && base == -1){
                    return 0;
                }
            } else {
                printf("SIMULATED LOSE ACK #%d\n", base);
            }
        }
    }
    return 0;
}
