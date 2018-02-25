// Hugh Smith April 2017
// Network code to support UDP client and server connections

#include "networks.h"
#include "gethostbyname.h"
#include "cpe464.h"

int32_t udp_server(int port_nbr) {
   int sk = 0;     
   struct sockaddr_in  local;   
   uint32_t len = sizeof(local);

   if ((sk = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
      perror("socket");
      exit(-1);
   }

   local.sin_family = AF_INET;
   local.sin_addr.s_addr = INADDR_ANY;
   local.sin_port = htons(port_nbr);

   if (bind(sk, (struct sockaddr *)&local, sizeof(local)) < 0) {
      perror("udp_server, bind");
      exit(-1);
   }

   getsockname(sk,(struct sockaddr *)&local, &len) ;
   printf("Using Port #: %d\n", ntohs(local.sin_port));

   return(sk);
}

int32_t udp_client_setup(char * hostname, uint16_t port_num, Connection * connection) {
   struct hostent * hp = NULL;      

   connection->sk_num = 0;
   connection->len = sizeof(struct sockaddr_in);

   if ((connection->sk_num = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
      perror("udp_client_setup, socket");
      exit(-1);
   }

   connection->remote.sin_family = AF_INET;

   hp = gethostbyname(hostname);
 
   if (hp == NULL) {
      printf("Host not found: %s\n", hostname);
      return -1;
   }

   memcpy(&(connection->remote.sin_addr), hp->h_addr, hp->h_length) ;

   connection->remote.sin_port = htons(port_num);

   return 0;
}

int32_t select_call(int32_t socket_num, int32_t seconds, int32_t microseconds, int32_t set_null) {
   fd_set fdvar;
   struct timeval a_timeout;
   struct timeval *timeout = NULL;

   if (set_null == NOT_NULL) {
      a_timeout.tv_sec = seconds;  // set timeout to 1 second
      a_timeout.tv_usec = microseconds; // set timeout (in micro-second)
      timeout = &a_timeout;
   }

   FD_ZERO(&fdvar);     // reset variables
   FD_SET(socket_num, &fdvar);   //

   if (select(socket_num+1,(fd_set *) &fdvar, (fd_set *) 0, (fd_set *) 0, timeout) < 0) {
      perror("select");
      exit(-1);
   }

   if (FD_ISSET(socket_num, &fdvar)) {
      return 1;
   }  
   return 0;  
}

int32_t safe_send(uint8_t* packet, uint32_t len, Connection* connection) {
   int send_len = 0;


   if ((send_len = sendtoErr(connection->sk_num, packet, len, 0,
      (struct sockaddr *)&(connection->remote), connection->len)) < 0) {
      perror("in safe_send(), sendtoErr() call");
      exit(-1);
   }
   return send_len;
}

int32_t safe_recv(int recv_sk_num, char* data_buf, int len, Connection *connection) {
   int recv_len = 0;
   uint32_t remote_len = sizeof(struct sockaddr_in);

   if ((recv_len = recvfrom(recv_sk_num, data_buf, len, 0, (struct sockaddr*)&(connection->remote), &remote_len)) < 0) {
      perror("in safe_recv(), recvfrom() call");
      exit(-1);
   }

   connection->len = remote_len;

   return recv_len;
}