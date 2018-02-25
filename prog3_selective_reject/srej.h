#ifndef __SREJ_H__
#define __SREJ_H__

#include "networks.h"
#include "cpe464.h"

#define MAX_LEN 1500
#define SIZE_OF_BUF_SIZE 4
#define START_SEQ_NUM 1
#define MAX_TRIES 10
#define LONG_TIME 10
#define SHORT_TIME 1

#pragma pack(1)

typedef struct header Header;
typedef struct window Window;

struct header {
   uint32_t seq_num;
   uint16_t checksum;
   uint8_t flag;
};

struct window {
   int32_t size;
   int32_t buf_size; 
   int32_t seq_num;  
};

enum FLAG {
   FNAME, DATA, FNAME_OK, FNAME_BAD, RR, SREJ, END_OF_FILE, EOF_ACK, CRC_ERROR = -1 
};

int32_t send_buf(uint8_t *buf, uint32_t len, Connection *connection, uint8_t flag, 
	uint32_t seq_num, uint8_t* packet);
int32_t recv_buf(uint8_t *buf, uint32_t len, int32_t recv_sk_num, 
	Connection *connection, uint8_t *flag, uint32_t *seq_num);
int process_select(Connection* client, int* retry_count, int select_timeout_state,
	int data_ready_state, int done_state);
int create_header(uint32_t len, uint8_t flag, uint32_t seq_num, uint8_t* packet);
int retrieve_header(char* data_buf, int recv_len, uint8_t *flag, int32_t* seq_num);

#endif