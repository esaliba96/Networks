#ifndef TRACE_H
#define TRACE_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pcap.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/ether.h>

struct eth_header {
   uint8_t dest_MAC[6];
   uint8_t src_MAC[6];
   uint16_t type;
} __attribute__((packed));

struct arp_header {
   uint16_t hardware_type;
   uint16_t protocol_type;
   uint8_t  hardware_size;
   uint8_t  protocol_size;
   uint16_t op;
} __attribute__((packed));

struct ip_header {
   uint8_t version_length;
   uint8_t service_type;
   uint16_t total_length;
   uint16_t identifier;
   uint16_t flags_offset;
   uint8_t time_live;
   uint8_t protocol;
   uint16_t checksum;
   in_addr_t src;
   in_addr_t dest;
} __attribute__((packed));

struct tcp_header {
   uint16_t src_port;
   uint16_t dest_port;
   uint32_t seq_nbr;
   uint32_t ack_nbr;
   uint8_t data_offset;
   uint8_t flags;
   uint16_t window;
   uint16_t checksum;
   uint16_t urgent_pointer;
} __attribute__((packed));

struct icmp_header
{
   uint8_t type;
   uint8_t code;
   uint16_t checksum;

} __attribute__((packed));

struct udp_header
{
   uint16_t src_port;
   uint16_t dest_port;
   uint16_t len;
   uint16_t checksum;

} __attribute__((packed));

struct tcp_pseudo_header
{
   uint32_t src_address;
   uint32_t dest_address;
   uint8_t reserved;
   uint8_t protocol;
   uint16_t seg_length;
} __attribute__((packed));

void print_arp(uint8_t *packet);
void print_IP(uint8_t *packet);
void print_ether(uint8_t *packet);
void print_TCP(uint8_t *packet, struct ip_header *ip);
void print_ICMP(uint8_t *packet);
void print_UDP(uint8_t *packet);
void TCP_UDP_port_print(int port);
int tcp_checksum(struct tcp_header *tcp, struct ip_header *ip);

#endif