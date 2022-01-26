/*************************************************************************
	> File Name: common.h
	> Author:liu 
	> Mail: 
	> Created Time: Thu 09 Sep 2021 09:04:05 AM CST
 ************************************************************************/

#ifndef _COMMON_H
#define _COMMON_H

#include "head.h"

#define UDP_SYN 0x01
#define UDP_SYN_ACK 0x02
#define UDP_ACK 0x04
#define UDP_FIN 0x08
#define UDP_MSG 0x16

struct udp_data_ds {
	int flag;
	char msg[512];
};

int make_nonblock(int fd);

int make_block(int fd);

int socket_create_udp(int port);

int socket_udp();

int recv_file_from_socket(int sockfd, char *name, char *dir);

int send_file_to_socket(int confd, char *name);

int get_line(int cfd, char *buf, int size);

int accept_udp(int listener);

int socket_connect_udp(const char *ip, int port);

void add_to_reactor(int fd, int epollfd);

char *get_conf_value(const char *filename, const char *key);

int socket_create(int port);

int socket_connect(const char* ip, int port);

#endif
