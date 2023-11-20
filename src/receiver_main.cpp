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

#include <fstream>
#include <iostream>
#include <string>
#include <queue>
// #include “sender_main.cpp”
// #include "param.h"

using namespace std;

#define MSS 5000

struct sockaddr_in si_me, si_other;
int sockfd, slen;
int expectAckn, prevAckn;
// extern int pks_RecvfromSender;
// extern int pks_SendtoReceiver;

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

void diep(char *s) {
    perror(s);
    exit(1);
}

struct compare {
    bool operator()(TCPheader a, TCPheader b) {
        return  a.seqn > b.seqn; 
    }
};

priority_queue<TCPheader, vector<TCPheader>, compare> pqueue;

void _receiver_data_handler(TCPheader &packet, TCPheader &ACKpacket){
    ACKpacket.ACK = 1;
    ACKpacket.DATA = 0;
    ACKpacket.seqn = packet.seqn;
    if (packet.seqn == expectAckn - 1){ /* in-order packet */
        ACKpacket.ackn = expectAckn;

        prevAckn = expectAckn;
        expectAckn++;

    } else if (packet.seqn > expectAckn - 1){ /* out-of-order packet */
        ACKpacket.ackn = prevAckn;

        expectAckn++;

    }
    return;
}


void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    slen = sizeof (si_other);


    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");
    cout<<"receiver's sockfd: "<<sockfd<<endl;
    memset((char *) &si_me, 0, sizeof (si_me));//每个字节都用0填充
    si_me.sin_family = AF_INET;//使用IPv4地址
    si_me.sin_port = htons(myUDPport);//端口
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);//任意本地址
    printf("Now binding\n");
    if (bind(sockfd, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");


	/* Now receive data and send acknowledgements */   
    // ofstream outfile;
    string s(destinationFile);
    FILE* fp = fopen(destinationFile, "wb");
    // outfile.open(s, ios::binary);//, ios::binary

    prevAckn = 1; // Ackn should start from 2 (Seqn starts from 1)
    expectAckn = 2;
    // pks_RecvfromSender = 0;

    TCPheader packet;
    while(1){
        // cout<<"Receiver: receiveing packets~~"<<endl;
        if (!recvfrom(sockfd, &packet, sizeof(packet), 0, (sockaddr*)&si_other, (socklen_t*)&slen)){
            diep("Receiver: failed to receive from the sender");
        }
        // pks_RecvfromSender++;
        if (packet.FIN == 1 && packet.FINACK == 0 && packet.ACK == 0 && packet.DATA == 0){
            break;
        }
        else if (packet.FIN == 0 && packet.FINACK == 0 && packet.ACK == 0 && packet.DATA == 1){
            // outfile.write(packet.data, packet.size);//确定的 bug：不能直接是 MSS！要不 diff 会不一致
            fwrite(packet.data, sizeof(char), packet.size, fp);
            TCPheader ACKpacket;
            _receiver_data_handler(packet, ACKpacket);
            if (!sendto(sockfd, &ACKpacket, sizeof(ACKpacket), 0, (sockaddr*)&si_other, (socklen_t)slen)){
                diep("Receiver: failed to send ACK to the sender");
            }
        }
    }
    // outfile.close();
    fclose(fp);
    close(sockfd);
	printf("%s received.", destinationFile);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}

