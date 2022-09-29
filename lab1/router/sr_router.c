/**********************************************************************
 * file:  sr_router.c
 * date:  Mon Feb 18 12:50:42 PST 2002
 * Contact: casado@stanford.edu
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
    /* REQUIRES */
    assert(sr);

    /* Initialize cache and cache cleanup thread */
    sr_arpcache_init(&(sr->cache));

    pthread_attr_init(&(sr->attr));
    pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_t thread;

    pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);
    pthread_detach(thread);
    
    /* Add initialization code here! 
    sr->if_list = (struct sr_if *)malloc(sizeof(struct sr_if));
    sr->if_list->name[0] = 0;
    sr->if_list->addr[0] = 0;
    sr->if_list->next = NULL;*/
} /* -- sr_init -- */

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

#define ICMP_DATA_SIZE_ 60

void send_time_exceeded(struct sr_instance* sr, sr_ethernet_hdr_t * eth_hdr,sr_ip_hdr_t * ip_hdr, char * interface){
	        sr_ethernet_hdr_t* icmp_reply = (sr_ethernet_hdr_t *)malloc(sizeof(sr_ethernet_hdr_t)+sizeof(sr_ip_hdr_t)+sizeof(sr_icmp_t3_hdr_t));
                memcpy(icmp_reply->ether_shost,eth_hdr->ether_dhost,ETHER_ADDR_LEN);
                memcpy(icmp_reply->ether_dhost,eth_hdr->ether_shost,ETHER_ADDR_LEN);
                icmp_reply->ether_type = htons(ethertype_ip);

                sr_ip_hdr_t * icmp_reply_ip_hdr = (sr_ip_hdr_t *)((uint8_t *)icmp_reply + sizeof(sr_ethernet_hdr_t));
                icmp_reply_ip_hdr->ip_v = 4;
                icmp_reply_ip_hdr->ip_hl = 5;
                icmp_reply_ip_hdr->ip_tos = 0;
                icmp_reply_ip_hdr->ip_len = htons(sizeof(sr_ip_hdr_t)+sizeof(sr_icmp_t3_hdr_t));
                icmp_reply_ip_hdr->ip_id = 0;
                icmp_reply_ip_hdr->ip_off = htons(IP_DF);
                icmp_reply_ip_hdr->ip_ttl = 64;
                icmp_reply_ip_hdr->ip_p = ip_protocol_icmp;
                icmp_reply_ip_hdr->ip_src = (sr_get_interface(sr, interface))->ip; 
                icmp_reply_ip_hdr->ip_dst = ip_hdr->ip_src;
                icmp_reply_ip_hdr->ip_sum = 0;
                icmp_reply_ip_hdr->ip_sum = cksum(icmp_reply_ip_hdr, 20);

                sr_icmp_t3_hdr_t * icmp_reply_icmp_hdr = (sr_icmp_t3_hdr_t *)((uint8_t *)icmp_reply + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
                icmp_reply_icmp_hdr->icmp_type = 11;
                icmp_reply_icmp_hdr->icmp_code = 0;
                icmp_reply_icmp_hdr->icmp_sum = 0;
                icmp_reply_icmp_hdr->unused = 0;
                icmp_reply_icmp_hdr->next_mtu = 0;
                memcpy((uint8_t *)icmp_reply_icmp_hdr+8, (uint8_t *)ip_hdr,ICMP_DATA_SIZE);
                icmp_reply_icmp_hdr->icmp_sum = cksum(icmp_reply_icmp_hdr, sizeof(sr_icmp_t3_hdr_t));

                sr_send_packet(sr, (uint8_t *)icmp_reply, 70, interface);
                free(icmp_reply);
	return;
}

void sr_handlepacket(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{
  /* REQUIRES */
  assert(sr);
  assert(packet);
  assert(interface);

  printf("*** -> Received packet of length %d \n",len);

  /* fill in code here */
  uint8_t * packet_cpy = (uint8_t *)malloc(len);
  memcpy(packet_cpy, packet, len);
  char* interface_cpy = (char *)malloc(sr_IFACE_NAMELEN);
  strcpy(interface_cpy,interface);
  if(ethertype(packet) == ethertype_ip)
  {
	if(len < 21+24 || len > IP_MAXPACKET+24)
		{free(packet_cpy);free(interface_cpy);return;}
	/*check validity*/
	sr_ethernet_hdr_t * eth_hdr = (sr_ethernet_hdr_t *)packet_cpy;
	sr_ip_hdr_t * ip_hdr = (sr_ip_hdr_t *)((uint8_t *)packet_cpy + sizeof(sr_ethernet_hdr_t));
	sr_icmp_hdr_t * icmp_hdr = (sr_icmp_hdr_t *)((uint8_t *)packet_cpy + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
	uint32_t ip = (sr_get_interface(sr, interface_cpy))->ip;
	
	uint16_t sum = ip_hdr->ip_sum;
        ip_hdr->ip_sum = 0;
        if(cksum(ip_hdr, 20) != sum){
                free(packet_cpy);
                free(interface_cpy);
                return;
        }
	ip_hdr->ip_sum = sum;
        if(ip_hdr->ip_ttl < 1){
                send_time_exceeded(sr, eth_hdr, ip_hdr, interface_cpy); 
		free(packet_cpy);
		free(interface_cpy);
		return;
	}
        /*check validity*/

	struct sr_if* if_walker = sr->if_list;
	while(if_walker)
	{
		if(if_walker->ip == ip_hdr->ip_dst)
		{	
			if(ip_hdr->ip_p == ip_protocol_icmp && icmp_hdr->icmp_type == 8)
			{       
				sr_ethernet_hdr_t* echo_reply = (sr_ethernet_hdr_t *)malloc(sizeof(sr_ethernet_hdr_t)+sizeof(sr_ip_hdr_t)+sizeof(sr_icmp_hdr_t)+ICMP_DATA_SIZE_);
                       		memcpy(echo_reply->ether_shost,eth_hdr->ether_dhost,ETHER_ADDR_LEN);
                       		memcpy(echo_reply->ether_dhost,eth_hdr->ether_shost,ETHER_ADDR_LEN);
				echo_reply->ether_type = htons(ethertype_ip);

				sr_ip_hdr_t * echo_reply_ip_hdr = (sr_ip_hdr_t *)((uint8_t *)echo_reply + sizeof(sr_ethernet_hdr_t));
				echo_reply_ip_hdr->ip_v = 4;
				echo_reply_ip_hdr->ip_hl = 5;
				echo_reply_ip_hdr->ip_tos = 0;
				echo_reply_ip_hdr->ip_len = htons(sizeof(sr_ip_hdr_t)+sizeof(sr_icmp_hdr_t)+ICMP_DATA_SIZE_);
				echo_reply_ip_hdr->ip_id = ip_hdr->ip_id;
				echo_reply_ip_hdr->ip_off = htons(IP_DF);
				echo_reply_ip_hdr->ip_ttl = 64;
				echo_reply_ip_hdr->ip_p = ip_protocol_icmp;
				echo_reply_ip_hdr->ip_src = if_walker->ip; /*interface ip address is already reversed*/
				echo_reply_ip_hdr->ip_dst = ip_hdr->ip_src;
				echo_reply_ip_hdr->ip_sum = 0;
				echo_reply_ip_hdr->ip_sum = cksum(echo_reply_ip_hdr, 20);

				sr_icmp_hdr_t * echo_reply_icmp_hdr = (sr_icmp_hdr_t *)((uint8_t *)echo_reply + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
				echo_reply_icmp_hdr->icmp_type = 0;
				echo_reply_icmp_hdr->icmp_code = 0;
				echo_reply_icmp_hdr->icmp_sum = 0;
				memcpy((uint8_t *)echo_reply_icmp_hdr+sizeof(sr_icmp_hdr_t), (uint8_t *)icmp_hdr+sizeof(sr_icmp_hdr_t),ICMP_DATA_SIZE_);
				echo_reply_icmp_hdr->icmp_sum = cksum(echo_reply_icmp_hdr, sizeof(sr_icmp_hdr_t)+ICMP_DATA_SIZE_);

				sr_send_packet(sr, (uint8_t *)echo_reply, sizeof(sr_ethernet_hdr_t)+sizeof(sr_ip_hdr_t)+sizeof(sr_icmp_hdr_t)+ICMP_DATA_SIZE_,interface_cpy);
                       		free(echo_reply);
				break;
			}/*end sending echo reply*/
			else if(ip_hdr->ip_p == 6 || ip_hdr->ip_p == 17){
				sr_ethernet_hdr_t* icmp_reply = (sr_ethernet_hdr_t *)malloc(sizeof(sr_ethernet_hdr_t)+sizeof(sr_ip_hdr_t)+sizeof(sr_icmp_t3_hdr_t));
                                memcpy(icmp_reply->ether_shost,eth_hdr->ether_dhost,ETHER_ADDR_LEN);
                                memcpy(icmp_reply->ether_dhost,eth_hdr->ether_shost,ETHER_ADDR_LEN);
                                icmp_reply->ether_type = htons(ethertype_ip);

                                sr_ip_hdr_t * icmp_reply_ip_hdr = (sr_ip_hdr_t *)((uint8_t *)icmp_reply + sizeof(sr_ethernet_hdr_t));
                                icmp_reply_ip_hdr->ip_v = 4;
                                icmp_reply_ip_hdr->ip_hl = 5;
                                icmp_reply_ip_hdr->ip_tos = 0;
                                icmp_reply_ip_hdr->ip_len = htons(sizeof(sr_ip_hdr_t)+sizeof(sr_icmp_t3_hdr_t));
                                icmp_reply_ip_hdr->ip_id = 0;
                                icmp_reply_ip_hdr->ip_off = 0;
                                icmp_reply_ip_hdr->ip_ttl = 64;
                                icmp_reply_ip_hdr->ip_p = ip_protocol_icmp;
                                icmp_reply_ip_hdr->ip_src = sr_get_interface(sr, interface_cpy)->ip; 
                                icmp_reply_ip_hdr->ip_dst = ip_hdr->ip_src;
                                icmp_reply_ip_hdr->ip_sum = 0;
                                icmp_reply_ip_hdr->ip_sum = cksum(icmp_reply_ip_hdr, 20);

                                sr_icmp_t3_hdr_t * icmp_reply_icmp_hdr = (sr_icmp_t3_hdr_t *)((uint8_t *)icmp_reply + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
                                icmp_reply_icmp_hdr->icmp_type = 3;
                                icmp_reply_icmp_hdr->icmp_code = 3;
                                icmp_reply_icmp_hdr->icmp_sum = 0;
                                icmp_reply_icmp_hdr->unused = 0;
                                icmp_reply_icmp_hdr->next_mtu = 0;
                                memcpy((uint8_t *)icmp_reply_icmp_hdr+8, (uint8_t *)ip_hdr,ICMP_DATA_SIZE);
				icmp_reply_icmp_hdr->icmp_sum = cksum(icmp_reply_icmp_hdr, sizeof(sr_icmp_t3_hdr_t));
                                
                                sr_send_packet(sr, (uint8_t *)icmp_reply, 70, interface_cpy);
                                free(icmp_reply);
                                break;
			}/*end TCP or UDP for me*/
		}/*end its for me*/
		if_walker = if_walker->next;
	}/*end search*/
	if(if_walker != NULL){
		free(packet_cpy);
		free(interface_cpy);
		return;
	}
	/*below is for packets not for me*/

	struct sr_rt* rt_walker = sr->routing_table;
	struct sr_rt* rt_entry = NULL;
	while(rt_walker){
		if(((rt_walker->dest.s_addr & rt_walker->mask.s_addr) == (ip_hdr->ip_dst & rt_walker->mask.s_addr)) && (rt_entry==NULL || rt_walker->mask.s_addr > rt_entry->mask.s_addr)){
			rt_entry = rt_walker;
		}
                rt_walker = rt_walker->next;
	}/*go through routing table*/

	if(rt_entry == NULL){
	/*send ICMP net unreachable*/
 		sr_ethernet_hdr_t* icmp_reply = (sr_ethernet_hdr_t *)malloc(sizeof(sr_ethernet_hdr_t)+sizeof(sr_ip_hdr_t)+sizeof(sr_icmp_t3_hdr_t));
                memcpy(icmp_reply->ether_shost,eth_hdr->ether_dhost,ETHER_ADDR_LEN);
                memcpy(icmp_reply->ether_dhost,eth_hdr->ether_shost,ETHER_ADDR_LEN);
                icmp_reply->ether_type = htons(ethertype_ip);

                sr_ip_hdr_t * icmp_reply_ip_hdr = (sr_ip_hdr_t *)((uint8_t *)icmp_reply + sizeof(sr_ethernet_hdr_t));
                icmp_reply_ip_hdr->ip_v = 4;
                icmp_reply_ip_hdr->ip_hl = 5;
                icmp_reply_ip_hdr->ip_tos = 0;
                icmp_reply_ip_hdr->ip_len = htons(sizeof(sr_ip_hdr_t)+sizeof(sr_icmp_t3_hdr_t));
                icmp_reply_ip_hdr->ip_id = 0;
                icmp_reply_ip_hdr->ip_off = htons(IP_DF);
                icmp_reply_ip_hdr->ip_ttl = 64;
                icmp_reply_ip_hdr->ip_p = ip_protocol_icmp;
                icmp_reply_ip_hdr->ip_src = ip; 
                icmp_reply_ip_hdr->ip_dst = ip_hdr->ip_src;
                icmp_reply_ip_hdr->ip_sum = 0;
                icmp_reply_ip_hdr->ip_sum = cksum(icmp_reply_ip_hdr, 20);

                sr_icmp_t3_hdr_t * icmp_reply_icmp_hdr = (sr_icmp_t3_hdr_t *)((uint8_t *)icmp_reply + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
                icmp_reply_icmp_hdr->icmp_type = 3;
                icmp_reply_icmp_hdr->icmp_code = 0;
                icmp_reply_icmp_hdr->icmp_sum = 0;
                icmp_reply_icmp_hdr->unused = 0;
                icmp_reply_icmp_hdr->next_mtu = 0;
		memcpy((uint8_t *)icmp_reply_icmp_hdr+8, (uint8_t *)ip_hdr,ICMP_DATA_SIZE);
                icmp_reply_icmp_hdr->icmp_sum = cksum(icmp_reply_icmp_hdr, sizeof(sr_icmp_t3_hdr_t));

                sr_send_packet(sr, (uint8_t *)icmp_reply, 70, interface_cpy);
                free(icmp_reply);
		free(packet_cpy);
		free(interface_cpy);
		return;
	}/*end sending ICMP net unreachable*/
	
	struct sr_arpentry * arp_entry = sr_arpcache_lookup(&(sr->cache), (rt_entry->gw).s_addr);
	if(arp_entry == NULL){
		/*arp not found, add request*/
		sr_arpcache_queuereq(&(sr->cache), (rt_entry->gw).s_addr, packet_cpy, len, rt_entry->interface);
		free(packet_cpy);
		free(interface_cpy);
		return;
	}/*end add arp request*/
	
	/*send packet*/
	if(ip_hdr->ip_ttl <= 1){
		char itf_name[sr_IFACE_NAMELEN];
                struct sr_if* walker_if = sr->if_list;
                while(walker_if){
                           if(!strncmp((char *)walker_if->addr, (char *)eth_hdr->ether_dhost, ETHER_ADDR_LEN))
                            {strcpy(itf_name, walker_if->name);break;}
                           walker_if = walker_if->next;
                }

                send_time_exceeded(sr, eth_hdr, ip_hdr, itf_name); 
        }else{
	memcpy(eth_hdr->ether_shost,sr_get_interface(sr, rt_entry->interface)->addr,ETHER_ADDR_LEN);
	memcpy(eth_hdr->ether_dhost,(uint8_t *)arp_entry->mac,ETHER_ADDR_LEN);
	ip_hdr->ip_sum = 0;
	ip_hdr->ip_ttl = ip_hdr->ip_ttl-1;
        ip_hdr->ip_sum = cksum(ip_hdr, 20);
        /*modify packet*/
	sr_send_packet(sr, (uint8_t *)packet_cpy, len, rt_entry->interface);
	}
	free(arp_entry);
	free(packet_cpy);
        free(interface_cpy);
  }/*end of ip*/

  else if(ethertype(packet) == ethertype_arp)
  {  
     sr_arp_hdr_t *arp_hdr = (sr_arp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));
     if(ntohs(arp_hdr->ar_op) == 1)
     {
	struct sr_if*  if_walker = sr->if_list;
	while(if_walker)
    	{
	       if(if_walker->ip == arp_hdr->ar_tip)
		{
			sr_ethernet_hdr_t* arp_reply = (sr_ethernet_hdr_t *)malloc(sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t));
	        	memcpy(arp_reply->ether_shost,if_walker->addr,ETHER_ADDR_LEN);
       			memcpy(arp_reply->ether_dhost,arp_hdr->ar_sha,ETHER_ADDR_LEN);
			arp_reply->ether_type = htons(ethertype_arp); 


			sr_arp_hdr_t *arp_reply_arp_hdr = (sr_arp_hdr_t *)((uint8_t *)arp_reply + sizeof(sr_ethernet_hdr_t));
			arp_reply_arp_hdr->ar_hrd = htons(arp_hrd_ethernet);
			arp_reply_arp_hdr->ar_pro = htons(ethertype_ip);
			arp_reply_arp_hdr->ar_hln = 6;
			arp_reply_arp_hdr->ar_pln = 4;
			arp_reply_arp_hdr->ar_op = htons(arp_op_reply);
		        memcpy(arp_reply_arp_hdr->ar_sha,if_walker->addr,ETHER_ADDR_LEN);
			arp_reply_arp_hdr->ar_sip = if_walker->ip;
			memcpy(arp_reply_arp_hdr->ar_tha,arp_hdr->ar_sha,ETHER_ADDR_LEN);
			arp_reply_arp_hdr->ar_tip = arp_hdr->ar_sip;
			sr_send_packet(sr, (uint8_t *)arp_reply, len,if_walker->name);
			free(arp_reply);
			break;
     		}/*end if found*/
		 if_walker = if_walker->next;
	}/*end while walker*/
     }/*end of arp request*/
     else if (ntohs(arp_hdr->ar_op) == 2)
     {
	struct sr_arpentry * arp_pair = sr_arpcache_lookup(&(sr->cache),arp_hdr->ar_sip);

	if(arp_pair == NULL)
	{
		struct sr_arpreq * arpreq = sr_arpcache_insert(&(sr->cache), arp_hdr->ar_sha, arp_hdr->ar_sip);
		if(arpreq)
		{
			struct sr_packet * walker = arpreq->packets;
			while(walker)
			{
				sr_ethernet_hdr_t * pkt = (sr_ethernet_hdr_t *)(walker->buf);
				sr_ip_hdr_t * pkt_ip = (sr_ip_hdr_t *)((uint8_t *)pkt + sizeof(sr_ethernet_hdr_t));

				if(pkt_ip->ip_ttl <= 1){
					char itf_name[sr_IFACE_NAMELEN];
					struct sr_if* walker_if = sr->if_list;
					while(walker_if){
						if(!strncmp((char *)walker_if->addr, (char *)pkt->ether_dhost, ETHER_ADDR_LEN))
							{strcpy(itf_name, walker_if->name);break;}
						walker_if = walker_if->next;
					}
                                       send_time_exceeded(sr, pkt, pkt_ip, itf_name); 
                                }else{
				memcpy(pkt->ether_shost,sr_get_interface(sr, walker->iface)->addr,ETHER_ADDR_LEN);
			        memcpy(pkt->ether_dhost,arp_hdr->ar_sha,ETHER_ADDR_LEN);
				pkt_ip->ip_sum = 0;
			        pkt_ip->ip_ttl = pkt_ip->ip_ttl-1;
			        pkt_ip->ip_sum = cksum(pkt_ip, 20);
			        /*modify packet*/
				sr_send_packet(sr, walker->buf, walker->len, walker->iface);}
				walker = walker->next;
			}

			sr_arpreq_destroy(&(sr->cache), arpreq);
		}

	}

	free(arp_pair);
     } /*end of arp reply*/

     free(packet_cpy);
     free(interface_cpy);
  }/*end arp*/
}/* end sr_ForwardPacket */

