char* file;
void putAlarm(int signum){
    printf(" Client is not responding, probably it's disconnected!\n");
    remove(file);
    kill(getpid(), SIGKILL);
}

int putFile(int fd, int sockfd, struct sockaddr_in cli_addr, socklen_t cl_addr_len, int cl_pid, int srv_pid, int chunkSize, float loss_rate, int timeout, char* fileName){
    
    file = fileName;
    
    struct sockaddr_in fool_addr;
    unsigned int fool_addr_len;
    
    if (signal(SIGALRM, putAlarm) < 0){
        fprintf(stderr,"signal() failed for SIGALRM");
        exit(1);
    }
    
    //printf(" Sending requestACK\n");
    struct ACKPacket requestAck;
    requestAck = createACKPacket(1, 0, cl_pid, srv_pid);
    
    if(!is_lost(loss_rate)){
        if (sendto(sockfd,
                   &requestAck,
                   sizeof(requestAck),
                   0,
                   (struct sockaddr*)&cli_addr,
                   cl_addr_len) != sizeof(requestAck)){
            fprintf(stderr,"sendto() sent a different number of bytes than expected");
            exit(1);
        }
    } else {
        //printf(" Loss simulation requestACK");
    }
    
    alarm(timeout);
    
    int base = -2;
    int seqNumber = 0;
    
    while (1){
     
        struct segmentPacket dataPacket;

        struct ACKPacket ack;

        while ((recvfrom(sockfd,
                      &dataPacket,
                      sizeof(dataPacket),
                      MSG_PEEK,
                      (struct sockaddr *) &fool_addr,
                      &fool_addr_len)) < 0){
            if (errno != EINTR){
                fprintf(stderr,"recvfrom() failed");
                exit(1);
            } else if (errno == EINTR){
                continue;
            }
        }
        
        if (dataPacket.srv_pid == srv_pid){
            
            alarm(0);
            
            if ((recvfrom(sockfd,
                          &dataPacket,
                          sizeof(dataPacket),
                          0,
                          (struct sockaddr *) &cli_addr,
                          &cl_addr_len)) < 0){
                fprintf(stderr,"recvfrom() failed");
                exit(1);
            }

            seqNumber = dataPacket.seq_no;
            
            if (dataPacket.type == 2){
                
                //printf(" Recieved an Empty file\n");
                base = -1;
                ack = createACKPacket(4, base, cl_pid, srv_pid);
                
            } else if (dataPacket.seq_no == 0 && dataPacket.type == 1){
                
                //printf(" Recieved Initial Packet\n");
                
                write(fd, dataPacket.data, chunkSize);
                
                base = 0;
                ack = createACKPacket(2, base, cl_pid, srv_pid);
                
            } else if (dataPacket.seq_no == base + 1){
                
                //printf("Recieved Packet #%d\n", dataPacket.seq_no);
                
                write(fd, dataPacket.data, chunkSize);
                
                base = dataPacket.seq_no;
                ack = createACKPacket(2, base, cl_pid, srv_pid);
                
            } else if (dataPacket.type == 1 && dataPacket.seq_no != base + 1){
                
                //printf("Recieved out of sequence Packet #%d\n", dataPacket.seq_no);
                ack = createACKPacket(2, base, cl_pid, srv_pid);
            }

            if(dataPacket.type == 4 && seqNumber == base){
                base = -1;
                ack = createACKPacket(4, base, cl_pid, srv_pid);
            }

            if(!is_lost(loss_rate)){
                if(base >= 0){
                    //printf(" Sending ACK #%d\n", base);
                    if (sendto(sockfd,
                               &ack,
                               sizeof(ack), 0,
                               (struct sockaddr *) &cli_addr,
                               sizeof(cli_addr)) != sizeof(ack)){
                        fprintf(stderr,"sendto() sent a different number of bytes than expected");
                        exit(1);
                    }
                } else if (base == -1 && dataPacket.type != 5) {
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
            } else {
                //printf(" Loss simulation ACK #%d\n", base);
            }
            
            if(dataPacket.type == 5 && base == -1){
                return 0;
            } else if (dataPacket.type == 2){
                return 0;
            }
        }
        alarm(timeout);
    }
    return 0;
}
