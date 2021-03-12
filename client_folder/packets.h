struct segmentPacket {
    int type;
    int seq_no;
    int cl_pid;
    int srv_pid;
    char data[512];
};

struct ACKPacket {
    int type;
    int ack_no;
    int cl_pid;
    int srv_pid;
};

struct segmentPacket createDataPacket (int type, int seq_no, int cl_pid, int srv_pid, char* data){

    struct segmentPacket pkt;

    pkt.type = type;
    pkt.seq_no = seq_no;
    pkt.cl_pid = cl_pid;
    pkt.srv_pid = srv_pid;
    memset(pkt.data, 0, sizeof(pkt.data));
    memcpy(pkt.data, data, sizeof(pkt.data));
    return pkt;
}

struct segmentPacket createTerminalPacket (int seq_no, int cl_pid, int srv_pid){

    struct segmentPacket pkt;

    pkt.type = 4;
    pkt.seq_no = seq_no;
    pkt.cl_pid = cl_pid;
    pkt.srv_pid = srv_pid;
    memset(pkt.data, 0, sizeof(pkt.data));

    return pkt;
}

struct segmentPacket createTerminalConfirmPacket (int seq_no, int cl_pid, int srv_pid){

    struct segmentPacket pkt;

    pkt.type = 5;
    pkt.seq_no = seq_no;
    pkt.cl_pid = cl_pid;
    pkt.srv_pid = srv_pid;
    memset(pkt.data, 0, sizeof(pkt.data));

    return pkt;
}

struct ACKPacket createACKPacket (int ack_type, int base, int cl_pid, int srv_pid){
        struct ACKPacket ack;
        ack.type = ack_type;
        ack.ack_no = base;
        ack.cl_pid = cl_pid;
        ack.srv_pid = srv_pid;
        return ack;
}
