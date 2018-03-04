#include "srej.h"

int32_t send_buf(uint8_t *buf, uint32_t len, Connection *connection,
   uint8_t flag, uint32_t seq_num, uint8_t* packet) {
   int32_t sent_len = 0;
   uint16_t sending_len = 0;

   if (len > 0) {
      memcpy(&packet[sizeof(Header)], buf, len);
   }

   sending_len = create_header(len, flag, seq_num, packet);
   sent_len = safe_send(packet, sending_len, connection);

   return sent_len;
}

int32_t recv_buf(uint8_t *buf, uint32_t len, int32_t recv_sk_num, 
   Connection *connection, uint8_t *flag, uint32_t *seq_num) {
   char data_buf[MAX_LEN];
   int32_t recv_len = 0;
   int32_t data_len = 0;

   recv_len = safe_recv(recv_sk_num, data_buf, len, connection);
   data_len = retrieve_header(data_buf, recv_len, flag, seq_num);

   if (data_len > 0) {
      memcpy(buf, &data_buf[sizeof(Header)], data_len);
   }

   return data_len;
}

int create_header(uint32_t len, uint8_t flag, uint32_t seq_num, uint8_t* packet) {
   Header *head = (Header*)packet;
   uint16_t checksum = 0;

   seq_num = htonl(seq_num);
   memcpy(&(head->seq_num), &seq_num, sizeof(seq_num));
   head->flag = flag;

   memset(&(head->checksum), 0, sizeof(checksum));
   checksum = in_cksum((unsigned short*)packet, len + sizeof(Header));
   memcpy(&(head->checksum), &checksum, sizeof(checksum));

   return len + sizeof(Header);
}

int retrieve_header(char* data_buf, int recv_len, uint8_t* flag, int32_t* seq_num) {
   Header *head = (Header*)data_buf;
   int return_val = 0;

   if (in_cksum((unsigned short*)data_buf, recv_len) != 0) {
      return_val = CRC_ERROR;
   } else {
      *flag = head->flag;
      memcpy(seq_num, &(head->seq_num), sizeof(head->seq_num));
      *seq_num = ntohl(*seq_num);

      return_val = recv_len - sizeof(Header);
   }

   return return_val;
}

int process_select(Connection* client, int* retry_count, int select_timeout_state,
   int data_ready_state, int done_state) {
   int return_val = data_ready_state;

   (*retry_count)++;

   if (*retry_count > MAX_TRIES) {
      printf("Sent data %d times, no ACK, client is probably gone - I'm dead", MAX_TRIES);
      return_val = done_state;
   } else {
      if (select_call(client->sk_num, SHORT_TIME, 0, NOT_NULL) == 1) {
         *retry_count = 0;
         return_val = data_ready_state;
      } else {
         return_val = select_timeout_state;
      }
   }

   return return_val;
}

void init_window(Window *window, int size) {
   int i =0;

   window->bottom = 1;
   window->current = 1;
   window->top = size;
   window->size = size;
   window->buff = malloc((size + 1) * sizeof(Data));

   for (; i < size; i++) {
      memset(window->buff[i].payload, 0, MAX_LEN);
      window->buff[i].valid = 0;
      window->buff[i].len = 0;
   }
}

int full(Window* window) {
   return window->current > window->top;
}

void update_window(Window *window, int seq_num) {
   int i;

   for (i = window->bottom; i < seq_num; i++) {
      window->buff[(i-1)%window->size].valid = 0;
   }
   window->bottom = seq_num;
   window->top = seq_num + window->size - 1;
}

void add_data_to_buffer(Window* window, uint8_t* buf, int32_t data_len, int32_t seq_num) {
   int index = (seq_num-1) % window->size;
  // printf("window size: %d\n", window->size);
  // printf("index: %d\n", index);
   memcpy(window->buff[index].payload, buf, data_len);
   window->buff[index].len = data_len;
   window->buff[index].valid = 1;
   //printf("window buff: %s\n", window->buff[index].payload);
}

void get_data_from_buffer(Window* window, int seq, char** data, int *len) {
   int index = (seq-1) % window->size;
   //printf("index where lost %s\n", window->buff[index].payload);
   *data = window->buff[index].payload;
   *len = window->buff[index].len;
}

void remove_from_buffer(Window *window, int seq) {
   int index = (seq-1) % window->size;
   window->buff[index].valid = 0;
}

int check_if_valid(Window *window, int seq) {
   int index = (seq-1) % window->size;
   return window->buff[index].valid;
}

int all_invalid(Window *window) {
   int i;

   for (i = window->bottom; i <= window->top; i++) {
      if(window->buff[(i-1)%window->size].valid == 1) {
         return 0;
      }
   }

   return 1;
}