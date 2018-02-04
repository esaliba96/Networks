/******************************************************************************
* tcp_server.c
*
* CPE 464 - Program 1
*****************************************************************************/

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

#include "networks.h"

#define MAXBUF 1024
#define DEBUG_FLAG 0

void recvFromClient(int clientSocket);
int checkArgs(int argc, char *argv[]);

int main(int argc, char *argv[]) {
	int serverSocket = 0;   //socket descriptor for the server socket
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	serverSocket = tcpServerSetup(portNumber);
	clients->max_clients = 10;

	while(1) {
		fd_set fdvar;
		FD_ZERO(&fdvar);

		FD_SET(serverSocket, &fdvar);
		set_active(&fdvar);

		if (select(clients->max_clients +1, &fdvar, NULL, NULL, NULL) < 0){
			perror("select");
			exit(-1);
		} else {
			if (FD_ISSET(serverSocket, &fdvar)) {
				accept_client(serverSocket);
			}
			check_active(&fdvar);
		}
	}
	close(serverSocket);
	
	return 0;
}

int checkArgs(int argc, char *argv[]) {
	int portNumber = 0;

	if (argc > 2) {
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	if (argc == 2) {
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}

void accept_client(int server_socket) {
	struct sockaddr_in new_client;
	socklen_t sock_len = sizeof(new_client);
	struct client *c = add();

	if ((c->socket = accept(server_socket, (struct sockaddr *) &new_client, &sock_len)) < 0) {
		perror("new client");
		free(c);
		return;
	}
	if (clients->max_clients < c->socket) {
		clients->max_clients = c->socket;
	}
}

void set_active(fd_set *fdvar) {
	struct client *active;
	active = clients->head;
	while(active) {
		FD_SET(active->socket, fdvar);
		active = active->next;
	}
}

void check_active(fd_set *fdvar) {
	struct client *active = clients->head;
	while(active) {
		if(FD_ISSET(active->socket, fdvar)) {
			client_requests(active);
		}
		active = active->next;
	}
}

void client_requests(struct client* c) {
	ssize_t rec_buf;
	uint8_t* buf = malloc(2048);
	if ((rec_buf = recv(c->socket, buf, 2048-1, 0)) < 0) {
		perror("client_requests");
		free(buf);
		return;
	}

	if(rec_buf == 0) {
		remove_client(c);
	} else {
		c->packet_len = rec_buf;
		c->packet = (struct chat_header*) buf;

		switch(c->packet->flag) {
			case 1:
				add_user(c);
				break;
			case 5:
				send_it(c);
				break;
			case 10:
				send_client_list(c);
				break;
			case 8:
				exit_response(c);
				break;
		}
	}
}


void exit_response(struct client *c) {
	respond_to_client(c, 9, NULL, 0);
	remove_client(c);
}

void send_client_list(struct client *c) {
	uint32_t num_clients = client_nbr();
	respond_to_client(c, 11, (uint8_t*)&num_clients, sizeof(uint32_t));
	int i = 0;
	struct client *curr;
	curr = clients->head;
	uint8_t data[102];
	uint8_t len;
	ssize_t count = (ntohl(num_clients));

	for(i; i < count; i++) {
		printf("hello\n");
		len = strlen(curr->handle);
		memcpy(data, &len, sizeof(len));
		memcpy(data+sizeof(len), curr->handle, len);
		respond_to_client(c, 12, data, len + sizeof(len));
		curr = curr->next;
	}
	respond_to_client(c, 13, NULL, 0);
}

void add_user(struct client *c) {
	uint8_t* buf = (uint8_t*)c->packet;
	uint8_t handle_len = *(buf+ sizeof(struct chat_header));
	char *handle = malloc(handle_len);

	memcpy(handle, buf + sizeof(struct chat_header) + 1, handle_len);
	handle[buf[4]] = '\0';
	if(find(handle)) {
		respond_to_client(c, 3, NULL, 0);
		return;
	}

	c->handle = handle;
	respond_to_client(c, 2, NULL, 0);
}

void respond_to_client(struct client* c, int flag, uint8_t* data, ssize_t data_len) {
	struct chat_header response;

	response.flag = flag;
	response.size = sizeof(struct chat_header) + data_len;
	response.size = htons(response.size);

	uint8_t* packet = make_packet_server(response, data, data_len);
	ssize_t packet_len = sizeof(struct chat_header) + data_len;
	if (send(c->socket, packet, packet_len, 0) < 0) {
		perror("respond_to_client");
	}
	free(packet);
}

uint8_t* make_packet_server(struct chat_header response, uint8_t* data, ssize_t data_len) {
	int packet_len = sizeof(struct chat_header) + data_len;
	uint8_t* packet = malloc(packet_len);

	memcpy(packet, &response, sizeof(struct chat_header));
	memcpy(packet + sizeof(struct chat_header), data, data_len);

	return packet;
}

void send_it(struct client* c) {
	uint8_t* buf = (uint8_t*)c->packet;
	uint8_t sender_len = *(uint8_t*)(buf+ sizeof(struct chat_header));
	uint8_t dest_handle_len;
	uint8_t nbr = *(uint8_t*)(buf + sizeof(struct chat_header) + 1 + sender_len);
	int i = 0;
 	char dests[10][101];
 	memset(dests, 0, 10);

 	buf += sizeof(struct chat_header) + 1 + sender_len + 1;
	for(i; i < nbr; i++) {
		memcpy(&dest_handle_len, buf, 1);
		buf += 1;
		memcpy(dests[i], buf, dest_handle_len);
		dests[i][dest_handle_len] = '\0';
		buf += dest_handle_len; 
	}

	i =0;
	for (i; i < nbr; i++) {
		dest_handle_len =  strlen(dests[i]);
		struct client* dest_c = find(dests[i]);
		uint8_t fail[strlen(dests[i])];
		memcpy(fail, &dest_handle_len, sizeof(dest_handle_len));
		memcpy(fail + sizeof(dest_handle_len), dests[i], dest_handle_len);
		ssize_t fail_len = dest_handle_len + 1;
		if(dest_c == NULL) {
			respond_to_client(c, 7, fail, fail_len);
		} else {
			if (send(dest_c->socket, c->packet, c->packet_len, 0) < 0) {
		 		perror("send_it");
		 	}
		}
	}
}
