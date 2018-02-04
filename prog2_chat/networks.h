
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
void send_packet(uint8_t* sent_packet, ssize_t sent_packet_len);
uint8_t* make_packet_client(uint8_t flag, uint8_t *data, ssize_t data_len);
void client_requests(struct client* c);
void add_user(struct client *c);
void respond_to_client(struct client *c, int, uint8_t* , ssize_t);
uint8_t* make_packet_server(struct chat_header response, uint8_t* data, ssize_t data_len);
void server_run();
int read_socket(int);
void parse_input();
void parse_message(char* input);
void send_msg(uint8_t nbr_dest, char* dest, uint8_t dest_len, char* msg);
void send_grp_msg(uint8_t nbr_dest, char dest[10][101], char* msg);
void send_it(struct client *c);
void send_client_list(struct client *c);
void list();
void exit_user();
void exit_response(struct client*);
int block_user(char*);
void print_blocked();
int is_blocked(char* name);
int unblock_user(char* handle);
void get_msg(char *buf);

#endif
