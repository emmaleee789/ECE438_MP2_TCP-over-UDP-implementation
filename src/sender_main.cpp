#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

#include <iostream>
#include <netdb.h>
#include <string>
#include <cmath>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>
// #include "param.h"

using namespace std;

//---------------

#define MSS 5000
#define TIMEOUT 100000

struct TCPheader{
    int seqn = 0;
    int ackn = 0;
    int DATA = 0;
    int ACK = 0;
    int FIN = 0;
    int FINACK = 0;
    int size = 0;
    // clock_t start_time;
    // clock_t end_time;
    int resend_flag = 0;
    char data[MSS];
};


void _client_send_pkt_err();
void _send_packet();
void _congestion_control();
void _choose_mode();
void _send_fin();

void user_data_handler(char* filename, unsigned long long int bytesToTransfer);
void ack_handler(TCPheader &packet);
void timeout_handler(TCPheader &packet);



// struct addrinfo *p; // 将指向struct addrinfos 的链表中各个可用addrinfo
struct sockaddr_in si_other;
int sockfd, slen;
long SRTT, DevRTT, RTO, R2;

queue<TCPheader> file_buf;
queue<TCPheader> trans_buf;
queue<TCPheader> wait_for_ack;
unordered_map<int, int> ackn_n_map;
// unordered_map<int, vector<TCPheader>> ackn_pkt_map;
unordered_map<int, clock_t> seqn_startT_map;
unordered_map<int, clock_t> seqn_endT_map;


int isFirstPacket = 1; // flag
int isTimeout = 0; /* flags for congestion control */
int isNewAck = 0;
int isDupAck = 0; 
int isDupAck3 = 0; 
int seqnCnt = 1; //  维护Client的序列号的 counter
float cwnd, ssthresh; //only cwnd should be float??? bug
int mode;


void diep(char *s) {
    perror(s);
    exit(1);
}


void _client_send_pkt_err(){
    printf("Client: Fail to send file to the server.");
    exit(1);
    return;
}

void _client_recv_pkt_err(){
    printf("Client: Fail to receive file from the server.");
    exit(1);
    return;
}

void _send_to_wrapper(TCPheader packet){
    if (!sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr*)&si_other, sizeof(si_other))){
        _client_send_pkt_err();
    }
    return;
}

void _send_packet(){
    cout<<endl;

    if (file_buf.empty()){
        return;
    }

    cout<<"file_buf size: "<<file_buf.size()<<endl;

    // int pks_to_send = (cwnd - wait_for_ack.size() <= trans_buf.size())? cwnd - wait_for_ack.size() : trans_buf.size();
    int cnt = file_buf.size();
    while(cwnd >= wait_for_ack.size() && cnt > 0){
        TCPheader tmp_pkt = file_buf.front();
        cout<<"file content: "<<tmp_pkt.data<<endl;
        trans_buf.push(tmp_pkt);
        wait_for_ack.push(tmp_pkt); /* for re-send and check ACKs purpose */
        file_buf.pop();
        cnt--;
    }

    while (!trans_buf.empty()) { //bug: fix 
        TCPheader packet = trans_buf.front();
        cout<<"trans_buf content: "<<packet.data<<endl;

        clock_t startTime = clock(); /* clock的精度为ms级 */
        seqn_startT_map[packet.seqn] = startTime;

        _send_to_wrapper(packet);

        /* 成功发送的 packet 即为等待 ack 的 packet */
        trans_buf.pop();

        cout<<"send packet #"<<packet.seqn<<endl;
        cout<<"wait_for_ack.size(): "<<wait_for_ack.size()<<endl;
        // cout<<"trans_buf.size(): "<<trans_buf.size()<<endl;
        cout<<"cwnd: "<<cwnd<<endl;

    }
    return;
}

void _send_fin(){
    cout<<"begin to send FIN packet"<<endl;
    TCPheader FINpacket;
    FINpacket.FIN = 1;
    _send_to_wrapper(FINpacket);
    TCPheader FINACKpacket;
    while(1){
        if (!recvfrom(sockfd, &FINACKpacket, sizeof(FINACKpacket), 0, (struct sockaddr *)&si_other, (socklen_t*)&slen)){
            if (errno != EAGAIN || errno != EWOULDBLOCK)
                diep("failed to send FIN to receiver");
            else{
                trans_buf.push(FINpacket);
                _send_to_wrapper(FINpacket);
            }
        }
        else{
            if (FINACKpacket.FINACK == 1){
                cout<<"sender: received FINACK"<<endl;
                break;
            }
        }
    }


    return;
}

/*(Make Package, Check Window Size, Send Data via UDP, Calculate RTT)*/
/* 发送端的所有事情 */
//先把 file 里的内容全部填入 file_buf, 然后再一点一点从 file_buf中拿到 trans_buf
void user_data_handler(char* filename, unsigned long long int bytesToTransfer){
    FILE *fp;
    fp = fopen(filename, "rb");

    int packetnum = 0;
    int last_trans_bytes = 0;
    if (MSS >= bytesToTransfer){
        packetnum = 1;
        last_trans_bytes = bytesToTransfer;
    } else{
        packetnum = (bytesToTransfer + MSS - 1)/MSS;
        last_trans_bytes = bytesToTransfer%MSS;
        
    }
    cout<<"packetnum: "<< packetnum<<endl;
    cout<<"last_trans_bytes: "<<last_trans_bytes<<endl;
    for (int i=0; i<packetnum; i++){
        TCPheader packet;
        char tmp_data[MSS];
        int read_size = (i == packetnum - 1)? last_trans_bytes : MSS;
        if (!fread(tmp_data, sizeof(char), read_size, fp)){
            fprintf(stderr, "client: failed to write the file into buffer\n");
            exit(1);
        }
        /* 填入 packet 对应的信息 */
        memcpy(&packet.data, tmp_data, read_size);
        packet.DATA = 1;
        packet.seqn = seqnCnt;
        packet.size = read_size;
        seqnCnt++;
        file_buf.push(packet);
    }

    fclose(fp);

    return;
}

/* ack_handler 和 timeout_handler调用 */
void _congestion_control(){
    switch (mode){
        case 1: //慢启动状态
            if (isNewAck) {
                if (cwnd >= ssthresh){
                    mode = 2;
                    break;
                }
                cwnd = cwnd+1;
                isNewAck = 0;
            }
            else if (isDupAck) isDupAck = 0;
            else if (isDupAck3){
                ssthresh = (cwnd/2 > 1)? cwnd/2 : (float)1;
                cwnd = ssthresh+3;
                isDupAck3 = 0;//重置
                mode = 3;
            }
            else if (isTimeout){
                ssthresh = cwnd/2;
                cwnd = 1;
                isTimeout = 0;
            }
            break;
        case 2: //拥塞避免状态
            if (isNewAck) {
                cwnd = cwnd+1/cwnd;
                isNewAck = 0;
            }
            else if (isDupAck) isDupAck = 0;
            else if (isDupAck3){
                ssthresh = (cwnd/2 > 1)? cwnd/2 : (float)1;
                cwnd = ssthresh+3;
                isDupAck3 = 0;//重置
                mode = 3;
            }
            else if (isTimeout){
                ssthresh = cwnd/2;
                cwnd = 1;
                isTimeout = 0;
                mode = 1;
            }
            break;
        //拥塞发生状态
        case 3: //快速重传
            if (isNewAck) {
                cwnd = (ssthresh > 1)? ssthresh : (float)1;
                isNewAck = 0;
                mode = 2;
            }
            else if (isDupAck) {
                cwnd += 1;
                isDupAck = 0;
            }
            else if (isDupAck3){
                isDupAck3 = 0;
                break;//bug: ? not sure
            }
            else if (isTimeout){
                ssthresh = cwnd/2;
                cwnd = 1;
                isTimeout = 0;
                mode = 1;
            }
            break;
    }
    return;
}


/* (Update RTT, Update CWND & RWND, Update FSM of Congestion Control, Handle Duplicated ACK) */
void ack_handler(TCPheader &packet){
    cout<<endl;
    cout<<"received packet #"<<packet.seqn<<endl;
    cout<<"received packet.ackn: "<<packet.ackn<<endl;
    cout<<"wait_for_ack.front().seqn: "<<wait_for_ack.front().seqn<<endl;
    /* update 2 maps */
    ackn_n_map[packet.ackn]++;
    //Stale ACK
    if (ackn_n_map[packet.ackn] == 1 && wait_for_ack.front().seqn >= packet.ackn){ //bug log: not sure
        return;
    }

    //isNewAck
    else if (ackn_n_map[packet.ackn] == 1 && wait_for_ack.front().seqn < packet.ackn){
        isNewAck = 1;
        _congestion_control();
        cout<<"mode: "<<mode<<endl;
        cout<<"cwnd: "<<cwnd<<endl;

        /* update wait_for_ack_queue */
        while(!wait_for_ack.empty() && packet.ackn > wait_for_ack.front().seqn){
            wait_for_ack.pop();
        }

        // if (mode == 1 || mode == 2) _send_packet(); //扩大窗口后继续发送
        _send_packet();
    }
    else if (ackn_n_map[packet.ackn] > 1 && wait_for_ack.front().seqn == packet.ackn){ 
        /* Handle Duplicated ACK */
        //isDupAck3
        if (ackn_n_map[packet.ackn] - 1 == 3){ //除去第一个
            isDupAck3 = 1;
            _congestion_control();
            cout<<"mode: "<<mode<<endl;
            cout<<"cwnd: "<<cwnd<<endl;
            /* re-transmit missing segment */
            TCPheader resend_pkt = wait_for_ack.front();
            cout<<"prepared to re-send packet with seqn: "<<resend_pkt.seqn<<endl;
            resend_pkt.resend_flag = 1;
            resend_pkt.DATA = 1;
            resend_pkt.ACK = 0;
            resend_pkt.ackn = 0;
            _send_to_wrapper(resend_pkt);
        }
        else{//isDupAck
            isDupAck = 1;
            _congestion_control();
            cout<<"mode: "<<mode<<endl;
            cout<<"cwnd: "<<cwnd<<endl;

            // if (mode == 3) _send_packet(); //扩大窗口后继续发送

        }
    }

    /* update RTT */
    if (packet.resend_flag == 0){
        if (isFirstPacket){
            SRTT = static_cast<long>(seqn_endT_map[packet.seqn] - seqn_startT_map[packet.seqn]);
            DevRTT = SRTT/2;
            RTO = SRTT + 4*DevRTT;
        }
        else{
            R2 = static_cast<long>(seqn_endT_map[packet.seqn] - seqn_startT_map[packet.seqn]);
            SRTT = SRTT + 0.125*(R2 - SRTT);
            DevRTT = (1-0.25)*DevRTT + 0.25*abs(R2 - SRTT);
            RTO = SRTT + 4*DevRTT;
        }
    }

    return;
}


/* Check Timeout, Resend Package, Update FSM of Congestion Control, Restart the clock */
void timeout_handler(TCPheader &packet){
    cout<<"timeout!"<<endl;
    isTimeout = 1;
    _congestion_control();
    

    /* Resend Package -- Push all timeout packets into trans_buf and re-send to the server */
    queue<TCPheader> temp_q = wait_for_ack;
    while(!temp_q.empty()) {
        TCPheader packet = temp_q.front();
        packet.resend_flag = 1;
        temp_q.pop();
        trans_buf.push(packet);
    }
    _send_packet();
    return;
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {

    // _prepare_socket(hostname, hostUDPport);

    // _set_timeout();

    /* Determine how many bytes to transfer */
    slen = sizeof (si_other);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    /* Set timeout for the socket */
    timeval RTOv;
    RTOv.tv_sec = 0;
    RTOv.tv_usec =  1000;//bug: 用 RTO 就停不下来了
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &RTOv, sizeof(RTOv)) == -1){
        diep("setsockopt failed");
    }


    /* Open the file */
    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Client: Could not open file to send.");
        exit(1);
    }
    fclose(fp);

    /* initialize args for congestion control */
    cwnd = 1;
    ssthresh = 256;//bug: which ssthresh?
    mode = 1; /* 从慢启动算法开始 */

    isFirstPacket = 1;
    // pks_SendtoReceiver = 0;

    cout<<"Sender's bytesToTransfer: "<<bytesToTransfer<<endl;

	/* Send data and receive acknowledgements on s*/
    user_data_handler(filename, bytesToTransfer);

    _send_packet();

    TCPheader packet;
    while(!wait_for_ack.empty()){//因为这里处理不对，所以才有时候可以退出来，有时候才能退出来，有时候退不出来
        if (recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&si_other, (socklen_t*)&slen)){
            // cout<<"receive packet #"<<packet.seqn<<endl;
            // cout<<"wait_for_ack queue size: "<<wait_for_ack.size()<<endl;
            clock_t endTime = clock(); /* 精度>= 10ms */
            seqn_endT_map[packet.seqn] = endTime;
            
            if (packet.ACK == 1){
                ack_handler(packet);
            }

        } else {
            if (errno != EAGAIN || errno != EWOULDBLOCK)
                _client_recv_pkt_err();
            else
                timeout_handler(packet);

        }

    }

    _send_fin();


    printf("Client: Closing the socket\n");
    close(sockfd);
    return;

}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);

    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);

    return (EXIT_SUCCESS);
}
