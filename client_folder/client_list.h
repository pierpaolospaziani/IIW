int listFiles(int sockfd, struct sockaddr_in srv_addr, socklen_t srv_addr_len, int cl_pid, int chunkSize, int windowSize, float loss_rate){
    
    int base = -2;
    int seqNumber = 0;
    
    while (1){
     
        struct segmentPacket dataPacket;

        struct ACKPacket ack;

        if ((recvfrom(sockfd,
                      &dataPacket,
                      sizeof(dataPacket),
                      MSG_PEEK,
                      (struct sockaddr *) &srv_addr,
                      &srv_addr_len)) < 0){
            if (errno != EINTR){
                DieWithError("recvfrom() failed 1");
            }
        }
        
        if (dataPacket.cl_pid == cl_pid){
            
            if ((recvfrom(sockfd,
                          &dataPacket,
                          sizeof(dataPacket),
                          0,
                          (struct sockaddr *) &srv_addr,
                          &srv_addr_len)) < 0){
                DieWithError("recvfrom() failed 2");
            }
            
            int srv_pid = dataPacket.srv_pid;

            seqNumber = dataPacket.seq_no;
            
            if(dataPacket.seq_no == 0 && dataPacket.type == 1){
                
                printf("\n%s", dataPacket.data);
                fflush(stdout);
                
                base = 0;
                ack = createACKPacket(2, base, cl_pid, srv_pid);
                
            } else if (dataPacket.seq_no == base + 1){
                
                printf("%s", dataPacket.data);
                fflush(stdout);
                
                base = dataPacket.seq_no;
                ack = createACKPacket(2, base, cl_pid, srv_pid);
                
            } else if (dataPacket.type == 1 && dataPacket.seq_no != base + 1){
                //printf("Recieved Out of Sync Packet #%d\n", dataPacket.seq_no);
                ack = createACKPacket(2, base, cl_pid, srv_pid);
            }

            if(dataPacket.type == 4 && seqNumber == base ){
                base = -1;
                ack = createACKPacket(8, base, cl_pid, srv_pid);
            }

            if(!is_lost(loss_rate)){

                /* Send ACK for Packet Recieved */
                if(base >= 0){
                    //printf("\n------------------------------------  Sending ACK #%d\n", base);
                    if (sendto(sockfd,
                               &ack,
                               sizeof(ack), 0,
                               (struct sockaddr *) &srv_addr,
                               sizeof(srv_addr)) != sizeof(ack)){
                        DieWithError("sendto() sent a different number of bytes than expected");
                    }
                } else if (base == -1) {
                    printf("\n");
                    //printf("\n------------------------------------ Sending Terminal ACK\n");
                    if (sendto(sockfd,
                               &ack,
                               sizeof(ack),
                               0,
                               (struct sockaddr *) &srv_addr,
                               sizeof(srv_addr)) != sizeof(ack)){
                        DieWithError("sendto() sent a different number of bytes than expected");
                    }
                }

                /* if data packet is terminal packet, display and clear the recieved message */
                if(dataPacket.type == 4 && base == -1){
                    return 0;
                }
            } else {
                //printf("SIMULATED LOSE ACK #%d\n", base);
            }
        }
    }
    return 0;
}
