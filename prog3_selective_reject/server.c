#include "srej.h"
#include "networks.h"
#include "cpe464.h"

typedef enum state STATE;
enum state {
   START, DONE, FILENAME, SEND_DATA, WAIT_ON_DATA, FILE_END, RECV_ACKS, END_ACK, OUT
};

void run_server(int port_number);
void process_client(uint8_t* buf, int32_t recv_len, Connection* client, uint8_t flag);
STATE filename(Connection* client, uint8_t* buf, int32_t recv_len, int32_t* data_file, Window *window, int32_t* buf_size, uint8_t flag, int32_t* seq_num);
STATE send_data(Connection* client, uint8_t* packet, int32_t data_file, int32_t* seq_num, Window *window, int32_t buf_size, int32_t* eof_seq);
STATE recv_acks(Connection *client, Window *window, int32_t eof_seq);
STATE wait_for_data(Connection* client, Window* window);
STATE file_end(Connection* client, Window *window);

int main(int argc, char** argv) {
	int port_number = 0;
	int32_t server_sk = 0;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <error-percent> <optional-port-number>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	sendtoErr_init(atof(argv[1]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_OFF);

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
               process_client(buf, recv_len, &client, flag);
               exit(0);
            }
            while (waitpid(-1, &status, WNOHANG) > 0) {
            }
         }
      }
   }
}

void process_client(uint8_t* buf, int32_t recv_len, Connection* client, uint8_t flag) {
   STATE state = START;
   int32_t data_file = 0;
   Window window;
   int32_t buf_size = 0;
   int32_t seq_num = START_SEQ_NUM;
   uint8_t packet[MAX_LEN];
   uint8_t buf1[MAX_LEN];
   uint32_t recv_len1;
   int32_t eof_seq;
   
   while(state != DONE) {
      switch (state) {
         case START:
            state = FILENAME;
            break;
         case FILENAME:
            state = filename(client, buf, recv_len, &data_file, &window, &buf_size, flag, &seq_num);
            break;
         case SEND_DATA:
            state = send_data(client, packet, data_file, &seq_num, &window, buf_size, &eof_seq);
            break;
         case RECV_ACKS:
            state = recv_acks(client, &window, eof_seq);
            break;
         case WAIT_ON_DATA:
            state = wait_for_data(client, &window);
            break;
         case FILE_END:
            state = file_end(client, &window);
            break;
         default:
            printf("ERROR - default STATE\n");
            exit(-1);
            break;
      }
   }
   close(client->sk_num);
}

STATE filename(Connection* client, uint8_t* buf, int32_t recv_len, int32_t* data_file, Window *window, int32_t* buf_size, uint8_t flag, int32_t* seq) {
   char fname[MAX_LEN];
   int32_t window_size;
   uint8_t packet[MAX_LEN];
   int32_t seq_num = 0;
 
   if ((client->sk_num = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
      perror("filename, open client socket");
      exit(-1);
   }

   if (flag == CONNECT) {
      send_buf(0, 0, client, ACCEPTED, *seq, packet);
      (*seq)++;
   }
   
   recv_len = recv_buf(packet, MAX_LEN, client->sk_num, client, &flag, &seq_num);
   if (recv_len == CRC_ERROR) {
      return START;
   }
   
   if (flag == FNAME) { 
      memcpy(buf_size, packet, SIZE_OF_BUF_SIZE);
      *buf_size = ntohl(*buf_size);
      memcpy(&window_size, &packet[SIZE_OF_BUF_SIZE], SIZE_OF_BUF_SIZE);
      memcpy(fname, &packet[2*SIZE_OF_BUF_SIZE], recv_len - 2 * SIZE_OF_BUF_SIZE);
      init_window(window, window_size);
   }
   if (((*data_file) = open(fname, O_RDONLY)) < 0) {
      send_buf(0, 0, client, FNAME_BAD, *seq, buf);
      (*seq)++;
      return DONE;
   }

   send_buf(0, 0, client, FNAME_OK, *seq, buf);
   (*seq)++;
   
   return SEND_DATA;
}

STATE send_data(Connection* client, uint8_t* packet, int32_t data_file, int32_t* seq_num, Window *window, int32_t buf_size, int32_t* eof_seq) {
   uint8_t buf[MAX_LEN];
   int32_t len_read = 0;

   memset(buf, 0, MAX_LEN);

   if(full(window)) {
     // printf("full window\n");
      return WAIT_ON_DATA;
   }

   len_read = read(data_file, buf, buf_size);
   switch(len_read) {
      case -1:
         perror("send_data read error");
         return DONE;
      case 0:
         *eof_seq = window->current;
     //    printf("eof index %d\n", *eof_seq);
         //send_buf(buf, len_read, client, END_OF_FILE, window->current, packet);
         // printf("in eof\n");
         return WAIT_ON_DATA;
      default:
        // printf("index %d\n", window->current);
         send_buf(buf, len_read, client, DATA, window->current, packet);
         add_data_to_buffer(window, buf, len_read, window->current);
         window->current++;
         break;
   }

   if (select_call(client->sk_num, 0, 0, NOT_NULL) == 1)
      return RECV_ACKS;
   
   return SEND_DATA;
}

STATE recv_acks(Connection *client, Window *window, int32_t eof_seq) {
   int32_t recv_len;
   int32_t seq_num = 0;
   uint8_t flag;
   uint8_t packet[MAX_LEN];
   char* data;
   int len;
   uint32_t rr = 0;

   recv_len = recv_buf(packet, MAX_LEN, client->sk_num, client, &flag, &seq_num);
   memcpy(&rr, packet, SIZE_OF_BUF_SIZE);
  
   rr = ntohl(rr);
   //printf("rr: %d\n", rr);
   if (recv_len == CRC_ERROR) {
      return WAIT_ON_DATA; 
   }

   switch(flag) {
      case RR:
         if(rr > window->bottom) {
            //printf("seq: %d\n", seq_num);
            update_window(window, rr);
         }
         if (rr == eof_seq) {
          printf("in eof\n");
            return FILE_END;
         }
         break;
     case SREJ:
         update_window(window, rr);
         get_data_from_buffer(window, rr, &data, &len);
        // printf("data srej: %d\n", rr);
         send_buf(data, len, client, DATA, rr, packet);
         break;
      default:
         printf("ERROR - default flag in RECV\n");
         exit(-1);
         break;
   }
   return SEND_DATA;
}

STATE wait_for_data(Connection* client, Window* window) {
   static int retry_count = 0;
   int return_val;
   char *data;
   char packet[MAX_LEN];
   int len;

   retry_count++;
//   printf("here with count %d\n", retry_count);
   if (retry_count > MAX_TRIES) {
      printf("Sent data %d times, no ACK, client is probably gone - I'm dead", MAX_TRIES);
      return DONE;
   }

   if (select_call(client->sk_num, SHORT_TIME, 0, NOT_NULL) == 1) {
      retry_count = 0;
      return_val = RECV_ACKS;
   } else {
      get_data_from_buffer(window, window->bottom, &data, &len);
      send_buf(data, len, client, DATA, window->bottom, packet);
      return_val = WAIT_ON_DATA;
   }
   return return_val;
}

STATE file_end(Connection* client, Window *window) {
   static int retry_count = 0;
   int return_val;
   char *data;
   char packet[MAX_LEN];
   int32_t recv_len;
   uint8_t flag = 0;
   uint32_t seq_num = 0;
   STATE state = FILE_END;
   
   if (retry_count > MAX_TRIES) {
      return DONE;
   }
   send_buf(0, 0, client, END_OF_FILE, window->bottom, packet);
   if (select_call(client->sk_num, SHORT_TIME, 0, NOT_NULL) == 1) {
      recv_len = recv_buf(packet, MAX_LEN, client->sk_num, client, &flag, &seq_num);
      if (recv_len == CRC_ERROR) {
         return FILE_END;
      }
      if (flag == EOF_ACK) {
         return DONE; 
      }
   } else {
      retry_count++;
      return FILE_END;
   }
   return FILE_END;
}