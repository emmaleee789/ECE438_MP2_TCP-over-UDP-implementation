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
// #include "param.h"

using namespace std;

#define MSS 5000

struct sockaddr_in si_me, si_other;
int sockfd, slen;
int expectAckn;// prevAckn, countAckn;

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

void _receiver_data_handler(TCPheader &packet, TCPheader &ACKpacket, FILE* fp){
    // cout<<"Receiver: expect ACK "<<expectAckn<<endl;
    ACKpacket.ACK = 1;
    ACKpacket.DATA = 0;
    ACKpacket.seqn = packet.seqn;
    if (packet.seqn == expectAckn){ /* in-order packet */
        expectAckn++;

        fwrite(packet.data, sizeof(char), packet.size, fp);

        while(!pqueue.empty() && pqueue.top().seqn == expectAckn - 1){
            TCPheader pkt = pqueue.top();
            fwrite(pkt.data, sizeof(char), pkt.size, fp);
            expectAckn++;
            pqueue.pop();
        }

        ACKpacket.ackn = expectAckn;

        cout<<"Receiver: expect ackn: "<<expectAckn<<endl;
        cout<<"Receiver: receive right seqn: "<<packet.seqn<<endl;
        cout<<"Receiver: send ackn: "<<ACKpacket.ackn<<endl<<endl;

    } else if (packet.seqn > expectAckn){ /* out-of-order packet */
        cout<<"Receiver: expect ackn: "<<expectAckn<<endl;
        cout<<"Receiver: receive higher seqn: "<<packet.seqn<<endl<<endl;
        ACKpacket.ackn = expectAckn;
        
        pqueue.push(packet);

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

    // prevAckn = 1; // Ackn should start from 2 (Seqn starts from 1)
    expectAckn = 1;
    // countAckn = 2;
    // pks_RecvfromSender = 0;

    TCPheader packet;
    while(1){
        // cout<<"Receiver: receiveing packets~~"<<endl;
        if (!recvfrom(sockfd, &packet, sizeof(packet), 0, (sockaddr*)&si_other, (socklen_t*)&slen)){
            diep("Receiver: failed to receive from the sender");
        }
        // pks_RecvfromSender++;
        if (packet.FIN == 1 && packet.FINACK == 0 && packet.ACK == 0 && packet.DATA == 0){
            TCPheader FINACKpacket;
            FINACKpacket.FINACK = 1;
            if (!sendto(sockfd, &FINACKpacket, sizeof(FINACKpacket), 0, (sockaddr*)&si_other, (socklen_t)slen)){
                diep("Receiver: failed to send ACK to the sender");
            }
            break;
        }
        else if (packet.FIN == 0 && packet.FINACK == 0 && packet.ACK == 0 && packet.DATA == 1){
            // outfile.write(packet.data, packet.size);//确定的 bug：不能直接是 MSS！要不 diff 会不一致
            TCPheader ACKpacket;
            _receiver_data_handler(packet, ACKpacket, fp);
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

