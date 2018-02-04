/******************************************************************************
* tcp_client.c
*
*****************************************************************************/
#include "networks.h"

#define MAXBUF 1024
#define DEBUG_FLAG 0
#define xstr(a) str(a)
#define str(a) #a

void sendToServer(int socketNum);
void checkArgs(int argc, char * argv[]);

struct client_info {
	int socket;
	char handle[101];
	struct blocked *b;
};

struct client_info *g;

int main(int argc, char * argv[]) {
	int socketNum = 0;       
	g = malloc(sizeof(struct client_info));
	g->b = (struct blocked*)malloc(100 * sizeof(struct blocked));
	for (size_t i = 0 ; i < 100; i++) {
      memset(g->b[i].name, 0, 100);
   }
   checkArgs(argc, argv);
	g->socket = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);
	add_handle();
	server_run();
	free(g->b); 	
	close(socketNum);
	
	return 0;
}

void checkArgs(int argc, char * argv[]) {
	if(argc != 4) {
		fprintf(stderr, "Usage: ./myClient <handle_name> <server_name> <server_port>\n");
		exit(1);
	}

	if(strlen(argv[1]) > 100) {
		fprintf(stderr, "Invalid handle, handle longer than 100 characters: %s\n", argv[1]);
		exit(1);
	}

	if((int)argv[1][0] > 47 && (int)argv[1][0] < 58) {
		fprintf(stderr, "Invalid handle, handle starts with a number %s\n", argv[1]);
		exit(1);	
	}
	strncpy(g->handle, argv[1], 100);	
}

void add_handle() {
	uint8_t handle_len = strlen(g->handle);
	uint8_t* data = malloc(handle_len);
	data[0] = handle_len;
	memcpy(data+1, g->handle, handle_len);

	ssize_t sent_packet_len = handle_len + 1 + sizeof(struct chat_header);
	uint8_t* sent_packet = make_packet_client(1, data, handle_len + 1);

	ssize_t recv_len;
	struct chat_header *recv;

	send_wait_for_recv(sent_packet, sent_packet_len, (uint8_t**)&recv, &recv_len);

	if (recv) {
		switch(recv->flag) {
			case 2:
				break;
			case 3:
				fprintf(stderr, "Handle already in use: %s\n", g->handle);
				exit(1);
				break;
			default:
				fprintf(stderr, "ERROR: Unexpected header flag (%d)\n", recv->flag);
				exit(1);
				break;
		}
		free(recv);
	}
	free(sent_packet);
}

void send_wait_for_recv(uint8_t* sent_packet, ssize_t sent_packet_len, uint8_t** rec_packet, ssize_t* rec_packet_len) {
	ssize_t num_bytes;
	fd_set fdvar;
	int sent;
	if((sent = send(g->socket, sent_packet, sent_packet_len, 0)) < 0) {
		perror("sending packets failed");
		return;
	} 

	static struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	FD_ZERO(&fdvar);
	FD_SET(g->socket, &fdvar);

	if (select(g->socket + 1, &fdvar, NULL, NULL, &timeout) < 0) {
		perror("select in wait");
		exit(1);
	} else {
		if(FD_ISSET(g->socket, &fdvar)) {
			struct chat_header* recv_buf = malloc(2048);
			if((num_bytes = recv(g->socket, (uint8_t*)recv_buf, 2048, 0)) < 0) {
				perror("recv failed");
				return;
			}				
			*rec_packet = (uint8_t*)recv_buf;
			*rec_packet_len = num_bytes;
		}
	}
}

uint8_t* make_packet_client(uint8_t flag, uint8_t *data, ssize_t data_len) {
	int packet_len = sizeof(struct chat_header) + data_len;
	uint8_t* packet = malloc(packet_len);
	struct chat_header* chat_h = (struct chat_header*)packet;


	chat_h->size = packet_len;
	chat_h->size = htonl(chat_h->size);	
	chat_h->flag = flag;

	memcpy(packet + sizeof(struct chat_header), data, data_len);
	
	return packet;
}

void server_run() {
	fd_set fdvar;

	while(1) {
		printf("\r$: ");
		fflush(stdout);

		while(1) {
			FD_ZERO(&fdvar);
			FD_SET(g->socket, &fdvar);
			FD_SET(STDIN_FILENO, &fdvar);

			if (select(g->socket + 1, &fdvar, NULL, NULL, NULL) < 0) {
				perror("server_run");
				exit(1);
			}

			if (FD_ISSET(g->socket, &fdvar) || FD_ISSET(STDIN_FILENO, &fdvar)) {
				if (FD_ISSET(g->socket, &fdvar)) {
					if (read_socket(g->socket)) {
						break;
					} else {
						continue;
					}
				}
				if (FD_ISSET(STDIN_FILENO, &fdvar)) {
					parse_input();
				}
				break;
			}
		}
	}
}

int read_socket(int sk) {
	int out = 0;
	struct chat_header rec;
	ssize_t numBytes = 0;
	uint8_t data[1200];
	if ((numBytes = recv(sk, &rec, sizeof(struct chat_header), 0)) < 0) {
		perror("read_socket");
		out = 1;
	}
	else if (numBytes == 0) {
		fprintf(stderr, "ERROR: Server has closed the socket\n");
		exit(1);
	}
	int len = ntohs(rec.size) - 2;
	if ((numBytes = recv(sk, (uint8_t *)data, len, 0)) < 0) {
		perror("read_socket");
		out = 1;
	}

	uint8_t* buf = (uint8_t*)data;
	uint8_t handle_len;
	char name[101] = {0};
	
	if(rec.flag == 13) {
		out = 0;
	}

	if(rec.flag == 12) {
		handle_len = *(buf);
		memcpy(name, buf + sizeof(handle_len), handle_len);
		name[handle_len] = '\0';
		printf("\r%s\n", name);
		out = 1;
	}

	if(rec.flag == 7) {
		char fail[100] = {0};
		uint8_t fail_len;

		fail_len = *(uint8_t *)buf;		
		memcpy(fail, ((uint8_t *)buf) + 1, fail_len);
		fprintf(stderr, "\rClient with handle %s does not exist.\n", fail);
		out = 1;
	}

	if(rec.flag == 5) {
		get_msg(data);
		out = 1;
	}

	return out;
}

void parse_input() {
	char input[1400];

	if(!fgets(input, 1400, stdin)) {
		return;
	}
	if(strncmp(input, "%m", 2) == 0 || strncmp(input, "%M", 2) == 0) {
		parse_message(input + 3);
	}
	if(strncmp(input, "%L", 2) == 0 || strncmp(input, "%l", 2) == 0) {
		list();
	}
	if(strncmp(input, "%E", 2) == 0 || strncmp(input, "%e", 2) == 0) {
		exit_user();
	}
	if(strncmp(input, "%B", 2) == 0 || strncmp(input, "%b", 2) == 0) {
		block_user(input + 3);
	}
	if(strncmp(input, "%u", 2) == 0 || strncmp(input, "%U", 2) == 0) {
		unblock_user(input + 3);
	}
}

int block_user(char* handle) {
	int i = 0;
	char *name = strdup(handle);
	int len = strlen(handle);
	name[len-1] = '\0';
	if(len > 100) {
		fprintf(stderr, "Invalid handle, handle longer than 100 characters: %s\n", name);
		exit(1);
	}
	if(strlen(name) == 0) {
		print_blocked(g);
	} else {
		while(strlen(g->b[i].name) != 0) {
			if(strcmp(name, g->b[i].name) == 0) {
				printf("Block failed, handle %s is already blocked.\n", name);
				return 0;	
			}
			i++;
		}
		strcpy(g->b[i].name, name);
		print_blocked();
	}
	return 1;
}

void print_blocked() {
	int i = 1;
	printf("Blocked:");
	printf(" %s", g->b[i-1].name);
	while(strlen(g->b[i].name) > 0) {
		printf(", %s", g->b[i].name);
		i++;	
	}	
	printf("\n");
}

int is_blocked(char* name) {
	int i = 0;
	while(strlen(g->b[i].name) > 0) {
		if(strcmp(name, g->b[i].name) == 0) {
			return 1;
		}
		i++;
	}
	return 0;
}

int unblock_user(char* handle) {
	int i = 0;
	char *name = strdup(handle);
	int len = strlen(handle);
	name[len-1] = '\0';
	if(len > 100) {
		fprintf(stderr, "Invalid handle, handle longer than 100 characters: %s\n", name);
		exit(1);
	}
	if(!is_blocked(name)) {
		printf("Unblock failed, handle %s is not blocked.\n", name);
		return 0;	
	} else {
		while(strcmp(name, g->b[i].name) != 0) {
			i++;
		}
		while(strlen(g->b[i+1].name) > 0) {
			strcpy(g->b[i].name, g->b[i+1].name);
			i++;
		}
		g->b[i].name[0] = '\0';
		printf("Handle %s unblocked.\n", name);
	}
	return 1;
}

void exit_user() {
	uint8_t* packet = make_packet_client(8, NULL, 0);
	ssize_t packet_len = sizeof(struct chat_header);
	ssize_t rec_len;
	struct chat_header* rec;

	send_wait_for_recv(packet, packet_len, (uint8_t**)&rec, &rec_len);

	if(rec->flag == 9) {
		exit(1);
	}
}

void list() {
	uint8_t* packet = make_packet_client(10, NULL, 0);
	ssize_t packet_len = sizeof(struct chat_header);
	ssize_t rec_len;
	struct chat_header* rec;
	uint32_t count;

	send_wait_for_recv(packet, packet_len, (uint8_t**)&rec, &rec_len);
	if(rec->flag == 11) {
	 	uint8_t *data = (uint8_t *)rec;
	 	count = *(uint32_t *)(data + sizeof(struct chat_header));
	 	count = ntohl(count);
	 	printf("Number of clients: %d\n", count);
	}
}

void parse_message(char* input) {
	uint8_t from_c_len = strlen(g->handle);
	uint8_t dest_len;		
	ssize_t data_len;
 	uint8_t i = 0, nbr = 1;
 	char* in;
 	char handles[10][101];
 	char msg[1400];
 	char data[1400];
 	ssize_t index = 0;

 	memset(handles, 0, 10);
 	in = strtok(input, " ");
 	if(atoi(in) > 1 && atoi(in) < 9) {
 		nbr = atoi(in);
  		while (i < nbr) {
  			in = strtok(NULL, " ");
  			memcpy(handles[i], in, strlen(in));	
  			handles[i][strlen(in)] = '\0';
  			i++;
	  	}
 	} else {
 		memcpy(handles[0], in, strlen(in));
 		handles[0][strlen(in)] = '\0';
 	}
  	in = strtok(NULL, "");
  	memcpy(msg, in, strlen(in));
  	msg[strlen(in)] = '\0';
  	i = 0;
	data_len = sizeof(from_c_len) + from_c_len + sizeof(nbr) + strlen(msg) + 1;

  	memcpy(data, &from_c_len, sizeof(from_c_len));
	index += sizeof(from_c_len);
	memcpy(data + index, g->handle, from_c_len);
	index += from_c_len;
  	memcpy(data + index, &nbr, sizeof(nbr));
	index += sizeof(nbr);
  	for(; i < nbr; i++) {
  		dest_len = strlen(handles[i]);
  		data_len += dest_len + sizeof(dest_len);
  		memcpy(data + index, &dest_len, sizeof(dest_len));
  		index += sizeof(dest_len);
		memcpy(data + index, handles[i], dest_len);
		index += dest_len;
  	}
  	memcpy(data + index, msg, strlen(msg) + 1);

  	uint8_t* sent_packet = make_packet_client(5, (uint8_t*)data, data_len);
	ssize_t sent_packet_len = sizeof(struct chat_header) + data_len;
	
	send_packet(sent_packet, sent_packet_len);
}

void send_packet(uint8_t* sent_packet, ssize_t sent_packet_len) {
	if(send(g->socket, sent_packet, sent_packet_len, 0) < 0) {
		perror("sending packets failed");
		return;
	} 
}

void get_msg(uint8_t *buf) {
	uint8_t sender_len = *(buf);
	uint8_t dest_handle_len;	
	char msg[1400];
	int i = 0;
	uint8_t nbr = *(uint8_t*)(buf + 1 + sender_len);
	
	char sender[sender_len];
	memcpy(sender, buf+1, (ssize_t)sender_len);
	sender[sender_len] = '\0';
	buf += 1 + sender_len;
	dest_handle_len = *buf;

	buf += 1;
	for(; i < nbr; i++) {
		memcpy(&dest_handle_len, buf, 1);
		buf += 1;
		buf += dest_handle_len; 
	}
	strcpy(msg, (char*)buf);

	if(!is_blocked(sender)) {
		printf("\n%s: %s", sender, msg);
	}
}