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

   (*retry_count++);
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