#include "srej.h"
#include "networks.h"
#include "cpe464.h"

typedef enum state STATE;

enum state {
	DONE, FILENAME, RECV_DATA, FILE_OK, START, END_FILE, SREJ_BLOCK
};

void run_client(char* local_file, char* remote_file, int32_t window_size, int32_t buffer_size, char* host, int16_t port);
void check_args(int argc, char** argv);
STATE start_state(char* host, uint16_t port, Connection *server, int32_t* seq);
STATE filename(char* fname, int32_t buff_size, int32_t window_size, Connection* server, int32_t* seq);
STATE file_ok(int* out_file_fd, char* out_file_name);
STATE recv_data(int32_t out_fd, Connection *server, Window* window, int32_t* seq);
STATE srej_block(uint32_t out_fd, Connection *server, Window* window, int32_t* seq);
void write_to_out(Window *window, int bottom, int current, int out_fd);

int main(int argc, char** argv) {
	char* local_file;
	char* remote_file;

	check_args(argc, argv);

	remote_file = argv[2];

	sendtoErr_init(atof(argv[5]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_OFF);

	run_client(argv[1], argv[2], atoi(argv[3]), atoi(argv[4]), argv[6], atoi(argv[7]));

}

void check_args(int argc, char** argv) {
	if (argc != 8) {
		printf("usage : %s <local-file> <remote-file> <window-size> <buffer-size> <error-percent> <remote-machine> <remote-port>\n", argv[0]);
		exit(-1);
	}
	if (strlen(argv[1]) > 100) {
		printf("Local filename needs to be less than 100 and is: %d\n", strlen(argv[1]));
		exit(-1);
	}
	if (strlen(argv[2]) > 100) {
		printf("Remote filename needs to be less than 100 and is: %d\n", strlen(argv[2]));
		exit(-1);
	}
	if (atoi(argv[4]) < 4 || atoi(argv[4]) > 1400) {
		printf("Buffer size needs to be between 4 and 1400 and is: %d\n", atoi(argv[4]));
		exit(-1);
	}
	if(atoi(argv[5]) < 0 || atoi(argv[5]) >= 1) {
		printf("Errore rate needs to be between 0 and less than 1 and is: %d\n", atoi(argv[5]));
		exit(-1);
	}	
}

void run_client(char* local_file, char* remote_file, int32_t window_size, int32_t buff_size, char* host, int16_t port) {
	Connection server;
	int32_t out_fd;
	STATE state = START;
	Window window;
	init_window(&window, window_size);
	static int32_t seq_num = 0;

	while(state != DONE) {
		switch(state) {
			case START:
				state = start_state(host, port, &server, &seq_num);
				break;
			case FILENAME:
				//printf("sequence: %d\n", seq_num);
				state = filename(remote_file, buff_size, window_size, &server, &seq_num);
				break;
			case FILE_OK:
				state = file_ok(&out_fd, local_file);
				break;
			case RECV_DATA:
				state = recv_data(out_fd, &server, &window, &seq_num);
				break;
			case SREJ_BLOCK:
				state = srej_block(out_fd, &server, &window, &seq_num);
				break;
			default:
				printf("ERROR - default STATE\n");
				break;
		}
	}
	close(out_fd);
	close(server.sk_num);
}

STATE start_state(char* host, uint16_t port, Connection *server, int32_t* seq) {
	STATE state = START;
	uint8_t packet[MAX_LEN];
	uint8_t flag = 0;
	int32_t seq_num = 0;
	int32_t recv_check = 0;
	static int retry_count = 0;

	if (server->sk_num > 0) {
		close(server->sk_num);
	}
	if (udp_client_setup(host, port, server) < 0) {
		state = DONE;
	} else {
		(*seq)++;
		send_buf(0, 0, server, CONNECT, *seq, packet);
		//printf("seq here %d\n", *seq);
		if ((state = process_select(server, &retry_count, START, FILENAME, DONE)) == FILENAME) {
			recv_check = recv_buf(packet, MAX_LEN, server->sk_num, server, &flag, &seq_num);

			if (recv_check == CRC_ERROR) {
				state = START;
			} else if (flag == ACCEPTED) {
				state = FILENAME;	
			}
		}
	}
	return state;
}

STATE filename(char* fname, int32_t buff_size, int32_t window_size, Connection* server, int32_t* seq) {
	STATE state = FILENAME;
	uint8_t packet[MAX_LEN];
	uint8_t buf[MAX_LEN];
	uint8_t flag = 0;
	int32_t seq_num = 0;
	int32_t fname_len = strlen(fname) + 1;
	int32_t recv_check = 0;
	static int retry_count = 0;

	buff_size = htonl(buff_size);
	memcpy(buf, &buff_size, SIZE_OF_BUF_SIZE);
	memcpy(buf + SIZE_OF_BUF_SIZE, &window_size, SIZE_OF_BUF_SIZE);
	memcpy(buf + 2* SIZE_OF_BUF_SIZE, fname, fname_len);

	(*seq)++;
	send_buf(buf, fname_len + 2 * SIZE_OF_BUF_SIZE, server, FNAME, *seq, packet);
	if ((state = process_select(server, &retry_count, FILENAME, FILE_OK, DONE)) == FILE_OK) {
		recv_check = recv_buf(packet, MAX_LEN, server->sk_num, server, &flag, &seq_num);
		if (recv_check == CRC_ERROR) {
			state = FILENAME;
		} else if (flag == FNAME_BAD) {
			printf("File %s not found\n", fname);
			state = DONE;
		}
	}

	return state;
}

STATE file_ok(int* out_file_fd, char* out_file_name) {
	if ((*out_file_fd = open(out_file_name, O_CREAT|O_TRUNC|O_WRONLY, 0600)) < 0) {
		perror("File open error");
		return DONE;
	}
	return RECV_DATA;
}

STATE recv_data(int32_t out_fd, Connection *server, Window* window, int32_t* seq) {
	int32_t seq_num = 0;
	uint8_t flag;
	int32_t data_len;
	uint8_t data_buf[MAX_LEN];
	uint8_t packet[MAX_LEN];
	uint32_t rr[1];

	if(select_call(server->sk_num, LONG_TIME, 0, NOT_NULL)  == 0) {
		printf("Timeout after 10 seconds, server is dead!\n");
		return DONE;
	}

	data_len = recv_buf(data_buf, MAX_LEN, server->sk_num, server, &flag, &seq_num);
	//printf("seq num: %d\n", seq_num);
	if (data_len == CRC_ERROR) {
		//rr[0] = htonl(window->bottom);
		//(*seq)++;
		window->current = window->bottom;
		//send_buf((uint8_t*)rr, 4, server, SREJ, *seq, packet);
		return SREJ_BLOCK;
	}

	if (flag == END_OF_FILE) {
		(*seq)++;
		send_buf(0, 0, server, EOF_ACK, *seq, packet);
		return DONE;
	}

	if (seq_num < window->bottom) {
		rr[0] = htonl(window->bottom);
		(*seq)++;
		send_buf((uint8_t*)rr, 4, server, RR, window->bottom, packet);
	} else if (seq_num == window->bottom) {
		update_window(window, window->bottom + 1);
		rr[0] = htonl(window->bottom);
		(*seq)++;
		//printf("rr here in send %d\n", ntohl(rr[0]));
		send_buf((uint8_t*)rr, 4, server, RR, *seq, packet);
		write(out_fd, data_buf, data_len);
		//printf("data intital: %s\n", data_buf);
	} else {
		add_data_to_buffer(window, data_buf, data_len, seq_num);
		window->current = window->bottom;
		//send_buf(0, 0, server, RR, window->bottom, packet);
		return SREJ_BLOCK;
	}

	return RECV_DATA;
}

STATE srej_block(uint32_t out_fd, Connection *server, Window* window, int32_t* seq) {
	int return_val;
   int seq_num;
	uint8_t flag;
	int32_t data_len;
	uint8_t data_buf[MAX_LEN];
	char* data;
	uint8_t packet[MAX_LEN];
	int32_t rejected;
	STATE state = SREJ_BLOCK;
	int len;
	int i;
	static last_recv = 0;
	uint32_t rr[1];

	if(select_call(server->sk_num, LONG_TIME, 0, NOT_NULL)  == 0) {
		printf("Timeout after 10 seconds, server is dead!\n");
		return DONE;
	}
   
	data_len = recv_buf(data_buf, MAX_LEN, server->sk_num, server, &flag, &seq_num);

	if (data_len == CRC_ERROR) {
		return SREJ_BLOCK;
	}

	if (flag == END_OF_FILE) {
		send_buf(0, 0, server, EOF_ACK, *seq, packet);
		return DONE;
	}

	add_data_to_buffer(window, data_buf, data_len, seq_num);
	for (i = window->bottom; i<= window->top; i++) {
		if(check_if_valid(window, i) == 0) {
			rejected = i;
			rr[0] = htonl(rejected);
			(*seq)++;
			send_buf((uint8_t*)rr, 4, server, SREJ, *seq, packet);
		}
		break;
	}

	while(rejected != seq_num) {
		data_len = recv_buf(data_buf, MAX_LEN, server->sk_num, server, &flag, &seq_num);
		if (data_len != CRC_ERROR && seq_num <= window->top) {
			if (seq_num >= last_recv) {
				last_recv = seq_num;			
			}
			add_data_to_buffer(window, data_buf, data_len, seq_num);
		}
	}

	for (i = rejected; i <= last_recv; i++) {
		if(check_if_valid(window, i) == 0) {
			window->bottom = i;
			write_to_out(window, rejected, i, out_fd);
			return SREJ_BLOCK;
		}
	}

	update_window(window, last_recv + 1);
	write_to_out(window, rejected, last_recv + 1, out_fd);
	rr[0] = htonl(last_recv+1);
	(*seq)++;
	send_buf((uint8_t*)rr, 4, server, RR, *seq, packet);
	return RECV_DATA;
}

void write_to_out(Window *window, int bottom, int current, int out_fd) {
	int i;
	char* data;
	int32_t len;

	for (i = bottom; i < current; i++) {
		printf("%d\n", i);
		get_data_from_buffer(window, i, &data, &len);
		remove_from_buffer(window, i);
		write(out_fd, data, len);
	}
}