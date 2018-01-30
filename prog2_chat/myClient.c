/******************************************************************************
* tcp_client.c
*
*****************************************************************************/
#include "networks.h"

#define MAXBUF 1024
#define DEBUG_FLAG 1
#define xstr(a) str(a)
#define str(a) #a

void sendToServer(int socketNum);
void checkArgs(int argc, char * argv[]);

struct client_info {
	int socket;
	char handle[100];
};

struct client_info *g;

int main(int argc, char * argv[]) {
	int socketNum = 0;         //socket descriptor
	g = malloc(sizeof(struct client_info));

	if(argc != 4) {
		fprintf(stderr, "Usage: ./myClient <handle_name> <server_name> <server_port>\n");
		exit(1);
	}

	if(strlen(argv[1]) > 100) {
		fprintf(stderr, "Invalid handle, handle longer than 100 characters: %s\n", argv[1]);
	}

	strncpy(g->handle, argv[1], 100);

	/* set up the TCP Client socket  */
	g->socket = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);
	//fprintf(stdout, "%s\n", g->handle);
	add_handle();
	server_run();
	//sendToServer(socketNum);
	
	close(socketNum);
	
	return 0;
}

void checkArgs(int argc, char * argv[]) {
	/* check command line arguments  */
	if (argc != 3)
	{
		printf("usage: %s host-name port-number \n", argv[0]);
		exit(1);
	}
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
	//printf("packet_len: %d\n", sent_packet_len);

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
	printf("Amount of data sent is: %d\n", sent_packet_len);
	
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
	printf("packet_len_1: %d\n", packet_len);
	printf("chat_len: %d\n", sizeof(struct chat_header));
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
		printf("$: ");
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
	return 1;
}

void parse_input() {
	char input[1400];

	if(!fgets(input, 1400, stdin)) {
		return;
	}
	if(strncmp(input, "%m", 2) == 0 || strncmp(input, "%M", 2) == 0) {
		//printf("we out her\n");
		parse_message(input + 3);
	}
}

void parse_message(char* input) {
	int input_len = strlen(input);
	char *index = input;
 	char *handle;

	while(!isspace(*index))
	 	index++;

	ssize_t handle_len = index - input;
	printf("%d\n", handle_len);
	handle = malloc(handle_len);
	memcpy(handle, input, handle_len);

	while(isspace(*index))
		index++;

	char *msg;
	ssize_t msg_len;
	if ((index - input) >= input_len) {
		msg = "\n";
		msg_len = 1;
	} else {
		msg = index;
		msg_len = strlen(msg);
	}

	if (msg_len > 1400) {
		fprintf(stderr, "ERROR: Message exceeds maximum allowed length\n");
		return;
	}
	//printf("%s\n", handle);
	//printf("%s\n", msg);
	send_msg(1, handle, handle_len, msg);
}

void send_msg(uint8_t nbr_dest, char* dest, uint8_t dest_len, char* msg) {
	uint8_t from_c_len = strlen(g->handle);	
	ssize_t data_len = 3 + dest_len + from_c_len + strlen(msg);
	uint8_t* data = malloc(data_len);
	ssize_t index = 0;

	if (strcmp(dest, g->handle) == 0) {
		printf("%s: %s", dest, msg);
	}

	memcpy(data, &from_c_len, sizeof(from_c_len));
	index += sizeof(from_c_len);
	memcpy(data + index, g->handle, from_c_len);
	index += from_c_len;
	memcpy(data +index, &dest_len, sizeof(dest_len));
	index += sizeof(dest_len);
	memcpy(data + index, dest, dest_len);
	index += dest_len;
	memcpy(data + index, msg, strlen(msg) + 1);

	uint8_t* sent_packet = make_packet_client(5, data, data_len);
	ssize_t sent_packet_len = sizeof(struct chat_header) + data_len;

	ssize_t recv_len;
	struct chat_header *recv;
	printf("packet_len: %d\n", sent_packet_len);

	send_wait_for_recv(sent_packet, sent_packet_len, (uint8_t**)&recv, &recv_len);

	char *fail;
	uint8_t fail_len;

	if(recv) {
		switch(recv->flag) {
			case 7:
				fail_len = *((uint8_t *)recv + sizeof(struct chat_header));
				memcpy(fail, ((uint8_t *)recv + sizeof(struct chat_header)), fail_len);
				//badHandle[badHandleLen] = '\0';
				fprintf(stderr, "Client with handle %s does not exist.\n", fail);
				break;
			default:
				fprintf(stderr, "ERROR: Unexpected header flag (%d)\n", recv->flag);
				exit(1);
				break;
		}
		free(recv);
	}
	free(sent_packet);
	free(data);
}

