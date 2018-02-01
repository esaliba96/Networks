#ifndef __CLIENT_H__
#define __CLEINT_H__

#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>


struct blocked {
	uint8_t *name;
};

struct client {
	int socket;
	char *handle;
	struct client* next;
	struct chat_header *packet;
	ssize_t packet_len;	
};


struct client_list {
	struct client* head;
	struct client* tail;
	int max_clients;
};

extern struct ClientList *client_list;

uint32_t client_nbr();
struct client* find(char* handle);
void remove_client(struct client* client);
struct client* add();
extern struct client_list *clients;


#endif
