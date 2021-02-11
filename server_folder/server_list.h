int listFiles(int sockfd, struct sockaddr_in cli_addr, socklen_t cl_addr_len, char* directory, int chunkSize, int windowSize, float loss_rate, int cl_pid, int srv_pid){
    
    // accesso alla cartella server_file
    DIR *d;
    struct dirent *dir;
    d = opendir("./server_file/");
    
    int num = 0;
    
    char* list;
    list = (char *) malloc(sizeof(char));
    if(list == NULL){
        printf("Memoria insufficiente\n");
        exit(1);
    }
    
    char tempBuff[1024];
    int len = 0;
    
    if (d != NULL){
        
        // mette tutti i file in 'list' e mando solo 1 pacchetto (dimensione permettendo)
        while ((dir = readdir(d)) != NULL){
            
            if (strcmp(dir->d_name, ".") &&
                strcmp(dir->d_name, "..") &&
                strcmp(dir->d_name, ".DS_Store")){
                
                num++;
                
                memset(tempBuff, 0, sizeof(tempBuff));
                
                snprintf(tempBuff, 1024, " - %s\n", dir->d_name);
                len += strlen(tempBuff);
                
                list = (char *) realloc(list, len);
                if(list == NULL){
                    printf("Memoria insufficiente\n");
                    exit(1);
                }
                
                strcat(list,tempBuff);
            }
        }
        closedir(d);
        
        // se non è presente alcun file
        if (num == 0){
            
            struct segmentPacket dataPacket;
            dataPacket = createTerminalPacket(num, cl_pid, srv_pid);
            
            if(!is_lost(loss_rate)){
                if (sendto(sockfd,
                           &dataPacket,
                           sizeof(dataPacket),
                           0,
                           (struct sockaddr *) &cli_addr,
                           sizeof(cli_addr)) != sizeof(dataPacket)){
                    DieWithError("sendto() sent a different number of bytes than expected");
                }
            } else {
                printf("SIMULATED LOSE\n");
            }
            
            alarm(TIMEOUT_SECS);
            
            struct ACKPacket ack;
            while (recvfrom(sockfd,
                            &ack,
                            sizeof(ack),
                            MSG_PEEK,
                            (struct sockaddr *) &cli_addr,
                            &cl_addr_len) < 0) {
                DieWithError("recvfrom() failed");
            }
            
            if (ack.cl_pid == cl_pid){
                
                if (recvfrom(sockfd,
                             &ack,
                             sizeof(ack),
                             0,
                             (struct sockaddr *) &cli_addr,
                             &cl_addr_len) < 0) {
                    if (errno != EINTR){
                        DieWithError("recvfrom() failed");
                    }
                }
                
                if(ack.type == 8){
                    printf("----------------------- Recieved ACK for Empty folder\n");
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
            int noTearDownACK = 1;
            
            while(noTearDownACK){

                /* Send chunks from base up to window size */
                while(seqNumber <= numOfSegments && (seqNumber - base) <= windowSize){
                    
                    struct segmentPacket dataPacket;

                    if(seqNumber == numOfSegments){
                        dataPacket = createTerminalPacket(seqNumber, cl_pid, srv_pid);
                        printf("Sending Terminal Packet\n");
                        
                    } else {
                        
                        char seg_data[chunkSize];
                        
                        memset(seg_data, 0, sizeof(seg_data));
                        
                        strncpy(seg_data, (list + seqNumber*chunkSize), chunkSize);

                        dataPacket = createDataPacket(1, seqNumber, cl_pid, srv_pid, seg_data);
                        printf("Sending Packet: %d\n", seqNumber);
                    }
                    
                    if(!is_lost(loss_rate)){
                        if (sendto(sockfd,
                                   &dataPacket,
                                   sizeof(dataPacket),
                                   0,
                                   (struct sockaddr *) &cli_addr,
                                   sizeof(cli_addr)) != sizeof(dataPacket)){
                            DieWithError("sendto() sent a different number of bytes than expected");
                        }
                    } else {
                        printf("SIMULATED LOSE Packet: %d\n", seqNumber);
                    }
                    seqNumber++;
                }
                
                alarm(TIMEOUT_SECS);
                printf("Window full: waiting for ACKs\n");
                
                struct ACKPacket ack;
                while (recvfrom(sockfd,
                                &ack,
                                sizeof(ack),
                                MSG_PEEK,
                                (struct sockaddr *) &cli_addr,
                                &cl_addr_len) < 0) {
                    if (errno == EINTR) {
                        seqNumber = base + 1;

                        printf("Timeout: Resending\n");
                        
                        if(tries >= MAXTRIES){
                            printf("Tries exceeded: Closing\n");
                            exit(1);
                        } else {
                            alarm(0);
                            
                            // RITRASMISSIONE
                            
                            while(seqNumber <= numOfSegments && (seqNumber - base) <= windowSize){
                                struct segmentPacket dataPacket;

                                if(seqNumber == numOfSegments){
                                    dataPacket = createTerminalPacket(seqNumber, cl_pid, srv_pid);
                                    printf("Sending Terminal Packet\n");
                                } else {
                                    char seg_data[chunkSize];
                                    
                                    memset(seg_data, 0, sizeof(seg_data));
                                    
                                    strncpy(seg_data, (list + seqNumber*chunkSize), chunkSize);

                                    dataPacket = createDataPacket(1, seqNumber, cl_pid, srv_pid, seg_data);
                                    printf("Sending Packet: %d\n", seqNumber);
                                }
                                
                                if(!is_lost(loss_rate)){
                                    if (sendto(sockfd,
                                               &dataPacket,
                                               sizeof(dataPacket),
                                               0,
                                               (struct sockaddr *) &cli_addr,
                                               sizeof(cli_addr)) != sizeof(dataPacket)){
                                        DieWithError("sendto() sent a different number of bytes than expected");
                                    }
                                } else {
                                    printf("SIMULATED LOSE Packet: %d\n", seqNumber);
                                }
                                seqNumber++;
                            }
                            alarm(TIMEOUT_SECS);
                        }
                        tries++;
                    } else {
                        DieWithError("recvfrom() failed");
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
                            DieWithError("recvfrom() failed");
                        }
                    }
                    
                    if(ack.type != 8){
                        printf("----------------------- Recieved ACK: %d\n", ack.ack_no);
                        if(ack.ack_no > base){
                            base = ack.ack_no;
                        }
                    } else {
                        printf("----------------------- Recieved Terminal ACK\n");
                        noTearDownACK = 0;
                    }
                    alarm(0);
                    tries = 0;
                }
            }
        }
    }
    close(sockfd);
    return 0;
}
