
/* 	Code originally give to Prof. Smith by his TA in 1994.
	No idea who wrote it.  Copy and use at your own Risk
*/

#ifndef __NETWORKS_H__
#define __NETWORKS_H__

#define BACKLOG 10

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>


#include "client.h"

struct chat_header { 
	uint16_t size;
	uint8_t flag;
};

// for the server side
int tcpServerSetup(int portNumber);
int tcpAccept(int server_socket, int debugFlag);

// for the client side
int tcpClientSetup(char * serverName, char * port, int debugFlag);
void accept_client(int server_socket);
void set_active(fd_set *fdvar);
void add_handle();
void check_active(fd_set *fdvar);
void send_wait_for_recv(uint8_t* sent_packet, ssize_t sent_packet_len, uint8_t** rec_packet, ssize_t* rec_packet_len);
uint8_t* make_packet_client(uint8_t flag, uint8_t *data, ssize_t data_len);
void client_requests(struct client* c);
void register_handle(struct client *c);
void respond_to_client(struct client *c, int, uint8_t* , ssize_t);
uint8_t* make_packet_server(struct chat_header response, uint8_t* data, ssize_t data_len);
void server_run();
int read_socket(int);
void parse_input();
void parse_message(char* input);
void send_msg(char* handle, char* msg);

#endif
