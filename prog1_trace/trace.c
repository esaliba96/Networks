#include "checksum.h"
#include "trace.h"

int main(int argc, char **argv) {
   char err[1024];
   int count = 0, stat = 0;
   uint8_t *packet = NULL;
   struct pcap_pkthdr *pkth;
   pcap_t *ret = NULL;
  
   if (argc != 2) {
      printf("improper format\n");
      exit(1);
   }

   ret = pcap_open_offline(argv[1], err);
  
   while (1) {
      stat = pcap_next_ex(ret, &pkth, (const uint8_t **)&packet);
      if (stat == 1) {
         printf("\nPacket number: %d  Packet Len: %d\n\n", ++count, pkth->len);
         print_ether(packet);
      }
      else {
         break;
      }
   }
   pcap_close(ret);  
   return 0;
}

void print_ether(uint8_t *packet) {
   char *dest, *src;
   uint8_t *data;

   struct eth_header *eth = (struct eth_header*)packet;

   printf("\tEthernet Header\n");
      
   dest = ether_ntoa((struct ether_addr *)eth->dest_MAC  );
   printf("\t\tDest MAC: %s\n", dest);

   src = ether_ntoa((struct ether_addr *)eth->src_MAC  );
   printf("\t\tSource MAC: %s\n", src);

   eth->type = ntohs(eth->type);

   data = packet + sizeof(struct eth_header);

   printf("\t\tType: ");
   if (eth->type == 0x0806) {
      printf("ARP\n\n"); 
      print_arp(data);     
   } else if (eth->type == 0x0800) {
      printf("IP\n\n");
      print_IP(data);
   } else {
      printf("Unknown\n\n");
   }
}

void print_arp(uint8_t *packet) {
   uint8_t *sender_MAC, *target_MAC;
   in_addr_t *sender_IP, *target_IP;
   struct in_addr net;

   struct arp_header *arp = (struct arp_header*)(packet);

   printf("\tARP header\n");

   printf("\t\tOpcode: ");
   arp->op = ntohs(arp->op);
   if (arp->op == 1) {
      printf("Request\n");
   } else if (arp->op == 2) {
      printf("Reply\n");
   } else {
      printf("Unknown\n");
   }
   
   sender_MAC = (uint8_t *)arp + sizeof(struct arp_header);
   printf("\t\tSender MAC: %s\n", ether_ntoa((struct ether_addr *)sender_MAC));

   sender_IP = (in_addr_t *)(sender_MAC + arp->hardware_size);
   net.s_addr = (in_addr_t)*sender_IP;
   printf("\t\tSender IP: %s\n", inet_ntoa(net));

   target_MAC = (uint8_t*)sender_IP + arp->protocol_size;
   printf("\t\tTarget MAC: %s\n", ether_ntoa((struct ether_addr *)target_MAC));


   target_IP = (in_addr_t *)(target_MAC + arp->hardware_size);
   net.s_addr = (in_addr_t)*target_IP;
   printf("\t\tTarget IP: %s\n\n", inet_ntoa(net));
}

void print_IP(uint8_t *packet) {
   char *protocol_name = NULL;
   struct ip_header *ip = (struct ip_header*)(packet);
   int checksum;
   struct in_addr net;
   uint8_t* data;

   printf("\tIP Header\n");
   printf("\t\tIP Version: %d\n", ip->version_length>>4);
   printf("\t\tHeader Len (bytes): %d\n", 4*((int)ip->version_length&0x0F));
   printf("\t\tTOS subfields:\n");
   printf("\t\t   Diffserv bits: %d\n", (int)ip->service_type>>2);
   printf("\t\t   ECN bits: %x\n", (int)ip->service_type&0x03);
   printf("\t\tTTL: %d\n", ip->time_live);

   switch (ip->protocol) {
      case 0x06:
         protocol_name = "TCP";
         break;
      case 0x01:
         protocol_name = "ICMP";
         break;
      case 0x11:
         protocol_name = "UDP";
         break;
      default:
         protocol_name = "Unknown"; 
   }

   printf("\t\tProtocol: %s\n", protocol_name);
   printf("\t\tChecksum: ");
   checksum = in_cksum((uint16_t *)packet, sizeof(struct ip_header));
   if(checksum == 0) {
      printf("Correct ");
   } else {
      printf("Incorrect ");
   } 

   printf("(0x%04x)\n", ntohs(ip->checksum));
   net.s_addr = ip->src;
   printf("\t\tSender IP: %s\n", inet_ntoa(net));
   net.s_addr = ip->dest;
   printf("\t\tDest IP: %s\n", inet_ntoa(net));

   data = packet + ((ip->version_length & 0x0F) * 4);
   switch (ip->protocol) {
      case 0x06:
         print_TCP(data, ip);
         break;
      case 0x01:
         print_ICMP(data);
         break;
      case 0x11:
         print_UDP(data);
         break;
   }
}

void print_TCP (uint8_t *packet, struct ip_header *ip) {
   struct tcp_header *tcp = (struct tcp_header*)(packet);
   int src_port = (int)ntohs(tcp->src_port);
   int dest_port = (int)ntohs(tcp->dest_port);

   printf("\n\tTCP Header\n");
   printf("\t\tSource Port:  ");
   TCP_UDP_port_print(src_port);   
   printf("\t\tDest Port:  ");
   TCP_UDP_port_print(dest_port);
   printf("\t\tSequence Number: %u\n", ntohl(tcp->seq_nbr));
   printf("\t\tACK Number: %u\n", ntohl(tcp->ack_nbr));
   printf("\t\tData Offset (bytes): %d\n", (tcp->data_offset>>4)*4);
   printf("\t\tSYN Flag: %s\n", (tcp->flags & (1<<1)) ? "Yes" : "No");
   printf("\t\tRST Flag: %s\n", (tcp->flags & (1<<2)) ? "Yes" : "No");
   printf("\t\tFIN Flag: %s\n", (tcp->flags & 1) ? "Yes" : "No");
   printf("\t\tACK Flag: %s\n", (tcp->flags & (1<<4)) ? "Yes" : "No");
   printf("\t\tWindow Size: %d\n", ntohs(tcp->window));
   printf("\t\tChecksum: ");
   if(tcp_checksum(tcp, ip) == 0) {
      printf("Correct ");
   } else {
      printf("Incorrect ");
   } 
   printf("(0x%04x)\n", ntohs(tcp->checksum));
}

void print_ICMP(uint8_t *packet) {
   struct icmp_header *icmp = (struct icmp_header*)(packet);
  
   printf("\n\tICMP Header\n");

   printf("\t\tType: ");
   switch(icmp->type) {
      case 0x00:
         printf("Reply\n");
         break;
      case 0x08:
         printf("Request\n");
         break;
      default:
      printf("%d\n", icmp->type);
   }
}

void print_UDP(uint8_t *packet) {
   struct udp_header *udp = (struct udp_header*)(packet);
   int src_port = (int)ntohs(udp->src_port);
   int dest_port = (int)ntohs(udp->dest_port);
   printf("\n\tUDP Header\n");
   
   printf("\t\tSource Port:  ");
   TCP_UDP_port_print(src_port);   
   printf("\t\tDest Port:  ");
   TCP_UDP_port_print(dest_port);
}

void TCP_UDP_port_print(int port) {
   switch(port) {
      case 23:
         printf("Telnet");
         break;
      case 20:
         printf("FTP");
         break;
      case 80:
         printf("HTTP");
         break;
      case 53:
         printf("DNS");
         break;
      case 110:
         printf("POP3");
         break;
      case 25:
         printf("SMTP");
         break; 
      default:
         printf("%d", port);
         break;
   }
   printf("\n");
}

int tcp_checksum(struct tcp_header *tcp, struct ip_header *ip) {
   uint16_t cksum_buf[1500];
   uint8_t ip_header_size = 4*(ip->version_length&0x0F);
   uint16_t ip_total_len = ntohs(ip->total_length);
   uint16_t tcp_header_len = sizeof(struct tcp_pseudo_header) + ip_total_len - ip_header_size;
   uint16_t tcp_len = ip_total_len - ip_header_size;

   struct tcp_pseudo_header *pseudo = (struct tcp_pseudo_header *) cksum_buf;

   pseudo->src_address = ip->src;
   pseudo->dest_address = ip->dest;
   pseudo->reserved = 0;
   pseudo->protocol = 0x06;
   pseudo->seg_length = htons(tcp_len);

   memcpy((char*)cksum_buf + sizeof(struct tcp_pseudo_header), tcp, tcp_len);
   int checksum = in_cksum(cksum_buf, tcp_header_len);

   return checksum;
}