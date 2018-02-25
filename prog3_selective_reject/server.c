#include "srej.h"
#include "networks.h"
#include "cpe464.h"

typedef enum state STATE;
enum state {
   START, DONE, FILENAME, SEND_DATA, WINDOW_FULL, WINDOW_CHECK, FILE_END, RECV_ACKS, END_ACK
};

void run_server(int port_number);
void process_client(uint8_t* buf, int32_t recv_len, Connection* client);
STATE filename(Connection* client, uint8_t* buf, int32_t recv_len, int32_t* data_file, Window **window);

int main(int argc, char** argv) {
	int port_number = 0;
	int32_t server_sk = 0;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <error-percent> <optional-port-number>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	sendtoErr_init(atof(argv[1]), DROP_ON, FLIP_ON, DEBUG_OFF, RSEED_OFF);

	if (argc == 3) {
		port_number = atoi(argv[2]);
	}

   server_sk = udp_server(port_number);
   run_server(server_sk);

   return 0;
}

void run_server(int server_sk) {
   pid_t pid = 0;
   int status;
   uint8_t buf[MAX_LEN];
   Connection client;
   uint8_t flag = 0;
   uint32_t seq_num = 0;
   uint32_t recv_len = 0;

   while (1) {
      if (select_call(server_sk, LONG_TIME, 0, NOT_NULL) == 1) {
         recv_len = recv_buf(buf, MAX_LEN, server_sk, &client, &flag, &seq_num);
         if (recv_len != CRC_ERROR) {
            if ((pid = fork()) < 0) {
               perror("fork");
               exit(-1);
            }
            if (pid == 0) {
               process_client(buf, recv_len, &client);
               exit(0);
            }
            while (waitpid(-1, &status, WNOHANG) > 0) {
            }
         }
      }
   }
}

void process_client(uint8_t* buf, int32_t recv_len, Connection* client) {
   STATE state = START;
   int32_t data_file = 0;
   Window *window = NULL;
   
   while(state != DONE) {
      switch (state) {
         case START:
            state = FILENAME;
            break;
         case FILENAME:
            state = filename(client, buf, recv_len, &data_file, &window);
            break;
         case WINDOW_CHECK:
            printf("chilling\n");
            break;
         default:
            printf("ERROR - default STATE\n");
            break;
      }
   }
}

STATE filename(Connection* client, uint8_t* buf, int32_t recv_len, int32_t* data_file, Window **window) {
   char fname[MAX_LEN];
   STATE state = DONE;
   int32_t buf_size;
   int32_t window_size;
   uint8_t response[1];

   memcpy(&buf_size, buf, SIZE_OF_BUF_SIZE);
   buf_size = ntohl(buf_size);
   memcpy(&window_size, &buf[SIZE_OF_BUF_SIZE], SIZE_OF_BUF_SIZE);
   memcpy(fname, &buf[2*SIZE_OF_BUF_SIZE], recv_len - 2 * SIZE_OF_BUF_SIZE);
  
   if ((client->sk_num = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
      perror("filename, open client socket");
      exit(-1);
   }

   if (((*data_file) = open(fname, O_RDONLY)) < 0) {
      send_buf(response, 0, client, FNAME_BAD, 0, buf);
      return DONE;
   }

   send_buf(response, 0, client, FNAME_OK, 0, buf);

   return WINDOW_CHECK;
}