
// 	Writen - HMS April 2017
//  Supports TCP and UDP - both client and server

#ifndef __NETWORKS_H__
#define __NETWORKS_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <fcntl.h>
#include <strings.h>

#define MAX_LEN 1500

enum SELECT {
	SET_NULL, NOT_NULL
};

typedef struct connection Connection;

struct connection {
  int32_t sk_num;
  struct sockaddr_in remote;
  uint32_t len;
};

int32_t udp_server(int port_nbr);
int32_t udp_client_setup(char * hostname, uint16_t port_num, Connection * connection);
int32_t select_call(int32_t socket_num, int32_t seconds, int32_t microseconds, int32_t set_null);
int32_t safe_send(uint8_t* packet, uint32_t len, Connection* connection);
int32_t safe_recv(int recv_sk_num, char* data_buf, int len, Connection *connection);

#endif