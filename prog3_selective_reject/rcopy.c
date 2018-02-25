#include "srej.h"
#include "networks.h"
#include "cpe464.h"

typedef enum state STATE;

enum state {
	DONE, FILENAME, RECV_DATA, FILE_OK, START, END_FILE, FULL_WINDOW
};

void run_client(char* local_file, char* remote_file, int32_t window_size, int32_t buffer_size, char* host, int16_t port);
void check_args(int argc, char** argv);
STATE start_state(char* host, uint16_t port, Connection *server);
STATE filename(char* fname, int32_t buff_size, int32_t window_size, Connection* server);
STATE file_ok(int* out_file_fd, char* out_file_name);

int main(int argc, char** argv) {
	char* local_file;
	char* remote_file;

	check_args(argc, argv);

	remote_file = argv[2];

	sendtoErr_init(atof(argv[5]), DROP_ON, FLIP_ON, DEBUG_OFF, RSEED_OFF);

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
	if (atoi(argv[4]) < 400 || atoi(argv[4]) > 1400) {
		printf("Buffer size needs to be between 400 and 1400 and is: %d\n", atoi(argv[4]));
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

	while(state != DONE) {
		switch(state) {
			case START:
				state = start_state(host, port, &server);
				break;
			case FILENAME:
				state = filename(remote_file, buff_size, window_size, &server);
				break;
			case FILE_OK:
				state = file_ok(&out_fd, local_file);
				break;
			case RECV_DATA:
				printf("hello\n");
				break;
			default:
				printf("ERROR - default STATE\n");
				break;
		}
	}
}

STATE start_state(char* host, uint16_t port, Connection *server) {
	STATE state = FILENAME;

	if (server->sk_num > 0) {
		close(server->sk_num);
	}
	if (udp_client_setup(host, port, server) < 0) {
		state = DONE;
	} else {
		state = FILENAME;
	}

	return state;
}

STATE filename(char* fname, int32_t buff_size, int32_t window_size, Connection* server) {
	STATE state = START;
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

	send_buf(buf, fname_len + 2 * SIZE_OF_BUF_SIZE, server, FNAME, 0, packet);

	if ((state = process_select(server, &retry_count, START, FILE_OK, DONE)) == FILE_OK) {
		recv_check = recv_buf(packet, MAX_LEN, server->sk_num, server, &flag, &seq_num);

		if (recv_check == CRC_ERROR) {
			state = START;
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