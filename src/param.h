/*
 * @Author: your name
 * @Date: 2023-11-15 11:25:13
 * @LastEditTime: 2023-11-17 13:43:02
 * @LastEditors: liziyingdeMacBook-Pro.local
 * @Description: In User Settings Edit
 * @FilePath: /mp2/src/param.h
 */

#ifndef PARAM_H
#define PARAM_H

#include <iostream>

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

int prevAckn = 0; //share with receiver side
int expectAckn = 0; //share with receiver side

void _prepare_socket(char* hostname, unsigned short int hostUDPport);
void _set_timeout();
void _client_send_pkt_err();
void _send_packet();
void _congestion_control();
void _choose_mode();
void _send_fin();

void user_data_handler(char* filename, unsigned long long int bytesToTransfer);
void ack_handler(TCPheader &packet);
void timeout_handler(TCPheader &packet);

#endif
