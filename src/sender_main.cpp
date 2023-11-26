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

/* only used in receiver side */
// int prevAckn; //share with receiver side
// int expectAckn; //share with receiver side

inline void _prepare_socket(char* hostname, unsigned short int hostUDPport);
inline void _set_timeout();
void _client_send_pkt_err();
void _send_packet();
void _congestion_control();
void _choose_mode();
void _send_fin();

void user_data_handler(char* filename, unsigned long long int bytesToTransfer);
void ack_handler(TCPheader &packet);
void timeout_handler(TCPheader &packet);

//---------------


// struct addrinfo *p; // 将指向struct addrinfos 的链表中各个可用addrinfo
struct sockaddr_in si_other;
int sockfd, slen;
// struct timeval tv;
long SRTT, DevRTT, RTO, R2;

queue<TCPheader> trans_buf;
queue<TCPheader> wait_for_ack;
unordered_map<int, int> ackn_n_map;
unordered_map<int, vector<TCPheader>> ackn_pkt_map;
unordered_map<int, clock_t> seqn_startT_map;
unordered_map<int, clock_t> seqn_endT_map;


int isFirstPacket = 1; // flag
int isTimeout = 0; /* flags for congestion control */
int isNewAck = 0;
int isDupAck = 0; 
int isDupAck3 = 0; 
int seqnCnt = 1; //  维护Client的序列号的 counter
float cwnd, ssthresh; //only cwnd should be float??? bug
// int receiveAckNum = 0;
int mode;
// int pks_SendtoReceiver;
// int pks_RecvfromSender;

void diep(char *s) {
    perror(s);
    exit(1);
}

// long long int currentTime_ms(void){
//     gettimeofday(&tv, NULL);
//     long long int rst;
//     rst = (long long int)tv.tv_sec * 1000 + ((long long int)tv.tv_usec) / 1000;
//     return rst;
// }

inline void _prepare_socket(char* hostname, unsigned short int hostUDPport){
    slen = sizeof (si_other);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");
    cout<<"sender's sockfd: "<<sockfd<<endl;
    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    return;
    
    // int status;
    // int fd;

    // struct addrinfo hints;
    // struct addrinfo *servinfo; // 将指向结果
    // memset(&hints, 0, sizeof hints); // 确保 struct 为空
    // hints.ai_family = AF_UNSPEC;  // 不用管是 IPv4 或 IPv6
    // hints.ai_socktype = SOCK_DGRAM; // UDP sockets

    // if ((status = getaddrinfo(hostname, (to_string(hostUDPport)).c_str(), &hints, &servinfo)) != 0) {
    //     fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    //     exit(1);
    // }

    // // loop through all the results and connect to the first we can
	// for(p = servinfo; p != NULL; p = p->ai_next) {
	// 	if ((fd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) {
	// 		perror("client: socket");
	// 		continue;
	// 	}

	// 	break;
	// }

	// if (p == NULL) {
	// 	fprintf(stderr, "client: failed to connect\n");
	// 	exit(1);
	// }
    

}

inline void _set_timeout(){
    struct timeval RTOv;
    RTOv.tv_sec = 0;
    RTOv.tv_usec = 10000;
    if (!setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &RTOv, sizeof(RTOv))){
        fprintf(stderr, "client: failed to set timeout\n");
        // WSAGetLastError();
		exit(1);
    }
    
    return;
    
    // TCPheader packet;
    // string tmp = "Just for RTT~~";
    // memcpy(packet.data, tmp.data(), sizeof(tmp)/sizeof(char));
    // clock_t startTime = clock(); /* clock的精度为ms级 */
    // if (!sendto(sockfd, &packet, sizeof(packet), 0, p->ai_addr, p->ai_addrlen)){
    //     _client_send_pkt_err();
    // }
    // recvfrom(sockfd, )
    // clock_t endTime = clock();
    // isFirstPacket = 0;
    // setsockopt(sockfd, SOL_SOCKET, )
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

void _send_packet(){
    cout<<"Sender: sending packets~~"<<endl;
    while(!trans_buf.empty() && cwnd >= wait_for_ack.size()){
        TCPheader packet = trans_buf.front();

        clock_t startTime = clock(); /* clock的精度为ms级 */
        seqn_startT_map[packet.seqn] = startTime;

        if (!sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr*)&si_other, sizeof(si_other))){
            _client_send_pkt_err();
        }

        /* 成功发送的 packet 即为等待 ack 的 packet */
        wait_for_ack.push(packet); /* for re-send and check ACKs purpose */
        trans_buf.pop();

        // pks_SendtoReceiver++;

    }
    return;
}

void _send_fin(){
    cout<<"Sender: begin to send FIN packet"<<endl;
    TCPheader FINpacket;
    FINpacket.FIN = 1;
    trans_buf.push(FINpacket);
    _send_packet();
    TCPheader FINACKpacket;
    while(1){
        if (!recvfrom(sockfd, &FINACKpacket, sizeof(FINACKpacket), 0, (struct sockaddr *)&si_other, (socklen_t*)&slen)){
            if (errno != EAGAIN || errno != EWOULDBLOCK)
                diep("Sender: failed to send FIN to receiver");
            else{
                trans_buf.push(FINpacket);
                _send_packet();
            }
        }
        else{
            break;
        }
    }


    return;
}

/*(Make Package, Check Window Size, Send Data via UDP, Calculate RTT)*/
/* 发送端的所有事情 */
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
    cout<<"Sender: packetnum: "<< packetnum<<endl;
    cout<<"Sender: last_trans_bytes: "<<last_trans_bytes<<endl;
    for (int i=0; i<packetnum; i++){
        // if (i > 4193){
        //     cout<<"reach over 4195 packets!!"<<endl;
        // }
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
        trans_buf.push(packet);
    }

    fclose(fp);
    
    _send_packet();

    return;
}

/* ack_handler 和 timeout_handler调用 */
void _congestion_control(){
    switch (mode){
        case 1: //慢启动状态
            if (isNewAck) {
                cwnd = cwnd+1;
                isNewAck = 0;
            }
            if (isDupAck) isDupAck = 0;
            if (isDupAck3){
                ssthresh = cwnd/2;
                cwnd = ssthresh+3;
                isDupAck3 = 0;//重置
                mode = 3;
            }
            if (isTimeout){
                ssthresh = cwnd/2;
                cwnd = 1;
                isTimeout = 0;
            }
            if (cwnd >= ssthresh){
                mode = 2;
            }
            break;
        case 2: //拥塞避免状态
            if (isNewAck) {
                cwnd = cwnd+1/cwnd;
                isNewAck = 0;
            }
            if (isDupAck) isDupAck = 0;
            if (isDupAck3){
                ssthresh = cwnd/2;
                cwnd = ssthresh+3;
                isDupAck3 = 0;//重置
                mode = 3;
            }
            if (isTimeout){
                ssthresh = cwnd/2;
                cwnd = 1;
                isTimeout = 0;
                mode = 1;
            }
            break;
        //拥塞发生状态
        case 3: //快速重传
            if (isNewAck) {
                cwnd = ssthresh;
                isNewAck = 0;
                mode = 2;
            }
            if (isDupAck) {
                cwnd += 1;
                isDupAck = 0;
            }
            if (isDupAck3){
                isDupAck3 = 0;
                break;//bug: ? not sure
            }
            if (isTimeout){
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
    // /* update waiting queue */
    // wait_for_ack.pop();//bug：wait_for_ack和收到的 packet 顺序可以（不）一致，所以（未必）对应

    /* update 2 maps */
    ackn_n_map[packet.ackn]++;
    //Stale ACK
    if (ackn_n_map[packet.ackn] == 1 && wait_for_ack.front().seqn >= packet.ackn){ //bug log: not sure
        return;
    }

    //isNewAck
    else if (ackn_n_map[packet.ackn] == 1 && wait_for_ack.front().seqn < packet.ackn){
        vector<TCPheader> vec;
        ackn_pkt_map[packet.ackn] = vec; //第一个 ackn 对应的包是正确的，后来三个才是错误的
        isNewAck = 1;
        _congestion_control();

        /* update wait_for_ack_queue */
        int i=0;
        while(!wait_for_ack.empty() && i < packet.ackn - wait_for_ack.front().seqn){
            i++;
            wait_for_ack.pop();
        }
    }
    //isDupAck
    else if (ackn_n_map[packet.ackn] > 1 && wait_for_ack.front().seqn == packet.ackn){ 
        ackn_pkt_map[packet.ackn].push_back(packet);
    }

    /* Handle Duplicated ACK */
    //isDupAck3
    if (ackn_n_map[packet.ackn] - 1 == 3){ //除去第一个
        isDupAck3 = 1;
        _congestion_control();
        /* re-transmit missing segment */
        int i=0;
        while (ackn_pkt_map[packet.ackn].size() != 0){
            TCPheader resend_pkt = ackn_pkt_map[packet.ackn][i];
            resend_pkt.resend_flag = 1;
            resend_pkt.DATA = 1;
            resend_pkt.ACK = 0;
            resend_pkt.ackn = 0;
            trans_buf.push(resend_pkt);
            i++;
        }
        _send_packet();
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
    cout<<"Sender: timeout!"<<endl;
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
    RTOv.tv_usec =  10000;//bug: 用 RTO 就停不下来了
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

    TCPheader packet;
    while(!wait_for_ack.empty()){
        if (recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&si_other, (socklen_t*)&slen)){
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

    // while(1){
    //     if (!recvfrom(sockfd, &packet, sizeof(packet), 0, p->ai_addr, &(p->ai_addrlen))){
    //         if (errno != EAGAIN || errno != EWOULDBLOCK)
    //             _client_recv_pkt_err();
    //     }
    //     if (packet.ACK == 1){
    //         ack_handler(packet);
    //     }
    // }
    // if (!wait_for_ack.empty()){
    //     //重发
    //     //所以就是重复 while(1)
    //     //所以两个逻辑应该合并
    // }

    /* hold until the receiver receive all packets */
    // while(pks_RecvfromSender < pks_SendtoReceiver){}
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
