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
typedef struct data Data;

struct data {
   char payload[MAX_LEN];
   int len;
   int valid;
};

struct header {
   uint32_t seq_num;
   uint16_t checksum;
   uint8_t flag;
};

struct window {
   int32_t size;
   int32_t bottom;
   int32_t current;
   int32_t top;
   struct data *buff;
};

enum FLAG {
   FNAME = 7, DATA = 3, FNAME_OK, FNAME_BAD, RR = 5, SREJ = 6, END_OF_FILE = 8, EOF_ACK, CONNECT = 1, ACCEPTED = 2, CRC_ERROR = -1 
};

int32_t send_buf(uint8_t *buf, uint32_t len, Connection *connection, uint8_t flag, 
	uint32_t seq_num, uint8_t* packet);
int32_t recv_buf(uint8_t *buf, uint32_t len, int32_t recv_sk_num, 
	Connection *connection, uint8_t *flag, uint32_t *seq_num);
int process_select(Connection* client, int* retry_count, int select_timeout_state,
	int data_ready_state, int done_state);
int create_header(uint32_t len, uint8_t flag, uint32_t seq_num, uint8_t* packet);
int retrieve_header(char* data_buf, int recv_len, uint8_t *flag, int32_t* seq_num);
void init_window(Window *window, int size);
int full(Window* window);
void add_data_to_buffer(Window* window, uint8_t* buf, int32_t data_len, int32_t seq_num);
void update_window(Window*, int);
void get_data_from_buffer(Window* window, int seq, char** data, int *len);
void remove_from_buffer(Window *window, int seq);
int check_if_valid(Window *window, int seq);
void all_invalid(Window *window);

#endif