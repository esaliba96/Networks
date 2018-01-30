#include "client.h"

struct client_list _clients;
struct client_list *clients =  &_clients;

struct client* add() {
	struct client* new_client = malloc(sizeof(struct client));

	if(!clients->head && !clients->tail) {
		clients->head = new_client;
		clients->tail = new_client;
	} else {
		clients->tail->next = new_client;
		clients->tail = new_client;
	}

	return new_client;
}

void remove_client(struct client* client) {
	struct client* c = clients->head;
	if(c == client) {
		clients->head = c->next;
		c = NULL;
	} else {
		while(c) {
			if(c->next == client) {
				c->next = client->next;
				break;
			}
			c = c->next;
		}
	}

	if(clients->tail == client) {
		clients->tail = c;
	}

	if(client->socket == clients->max_clients) {
		if(clients->head == NULL) {
			clients->max_clients = 8;
		}
	}

	close(client->socket);

	// free(client->handle);
	// free(client);
}

struct client* find(char* handle) {
	struct client* c = clients->head;

	while (c) {
		if (c->handle && (strcmp(handle, c->handle) == 0)) {
			break;
		}
		c = c->next;
	}
	return c;
}

uint32_t client_nbr() {
	struct client* c = clients->head;
	uint32_t i = 0;

	while(c) {
		i++;
		c = c->next;
	}
 	i = htonl(i);

	return i;
}
