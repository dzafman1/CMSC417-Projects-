#ifndef _ES_C_
#define _ES_C_

/* $Id: es.c,v 1.1 2000/03/01 14:09:09 bobby Exp bobby $
 * ----
 * $Revision: 1.1 $
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <string.h>
#include <signal.h>
#include <poll.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <argp.h>
#include <unistd.h> 
#include <netdb.h>
#include <endian.h>
#include <math.h>


#include "queue.h"
#include "common.h"
#include "es.h"
#include "ls.h"
#include "rt.h"
#include "n2h.h"

static struct el* g_el;
#define MAX_NODES 255
#define MAX_BUF_LEN 256*4

int init_new_el()
{
	InitDQ(g_el, struct el);
	assert (g_el);

	g_el->es_head = 0x0;
	return (g_el != 0x0);
}

void add_new_es()
{
	struct es* n_es;
	struct el* n_el = (struct el*)
		getmem (sizeof(struct el));

	struct el* tail = g_el->prev;
	InsertDQ(tail, n_el);

	// added new es to tail
	// lets start a new queue here
  
	{
		struct es* nhead = tail->es_head;
		InitDQ(nhead, struct es);

		tail = g_el->prev;     
		tail->es_head = nhead; 

		n_es = nhead;

		n_es->ev = _es_null;
		n_es->peer0 = n_es->peer1 = 
			n_es->port0 = n_es->port1 =
			n_es->cost = -1;
		n_es->name = 0x0;
	}
}

void add_to_last_es(e_type ev,
	node peer0, int port0,
	node peer1, int port1,
	int cost, char *name)
{
	struct el* tail = g_el->prev;
	bool local_event = false;
  
	assert (tail->es_head);

	// check for re-defined link (for establish)
	// check for local event (for tear-down, update)
	switch (ev) {
		case _es_link:
			// a local event?
			if ((peer0 == get_myid()) || peer1 == get_myid())
				local_event = true;
			break;
		case _ud_link:
			// a local event?
			if (geteventbylink(name))
				local_event = true;
			break;
		case _td_link:
			// a local event?
			if (geteventbylink(name))
				local_event = true;
			break;
		default:
			printf("[es]\t\tUnknown event!\n");
			break;
	}

	if (!local_event) {
		printf("[es]\t Not a local event, skip\n");
		return;
	}

	printf("[es]\t Adding into local event\n");

	{
		struct es* es_tail = (tail->es_head)->prev;
    
		struct es* n_es = (struct es*)
			getmem (sizeof(struct es));
    
		n_es->ev = ev;
		n_es->peer0 = peer0;
		n_es->port0 = port0;
		n_es->peer1 = peer1;
		n_es->port1 = port1;
		n_es->cost = cost;
		n_es->name = (char *)
			getmem(strlen(name)+1);
		strcpy (n_es->name, name);

		InsertDQ (es_tail, n_es);
	}
}


int timer = 0;
bool flag = true;
void alarm_handler(int time_between_es){
	
	timer += time_between_es;
	flag = true;
	signal(SIGALRM, alarm_handler); 
	alarm(time_between_es);
}
/*
 * A simple walk of event sets: dispatch and print a event SET every 2 sec
 */

void get_link_name(node m, node o, char *string){
	string[0] = 'L';
	char me[4] = {0};
	char other[4] = {0};
	
	sprintf(me,"%d", m);
	sprintf(other,"%d", o);
	
	strcat(strcat(string, me), other);
}

int send_rt(uint8_t *buffer){
	struct rte *temp;
	uint16_t counter = 1;
	uint8_t type = 0x7;
	uint8_t version = 0x1;
	memcpy(buffer, &type, 1);
	memcpy(buffer + 1, &version , 1);

	for (temp = get_rt_head()->next; temp != get_rt_head(); temp = temp->next){
		memcpy(buffer + counter*4, &(temp->d), 1);
		uint32_t cost_temp = temp->c;
		cost_temp = htonl(cost_temp);
		cost_temp = cost_temp << 8;
		
		memcpy(buffer + counter*4 + 1, &cost_temp, 3);
		counter ++;
	}
	counter --;
	memcpy(buffer + 2, &counter, 2);
	return counter;
}

void walk_el(int update_time, int time_between, int verb)
{
	struct el *el;
	struct es *es_hd;
	struct es *es;

	assert (g_el->next);
  	unsigned int temp_neg = -1;
	//print_el();

	/* initialize link set, routing table, and routing table */
	create_ls();
	create_rt();
	init_rt_from_n2h();
	
	alarm_handler(update_time);
	uint8_t old_link[MAX_NODES] = {0};

	for (el = g_el->next ; el != g_el ; el = el->next) {
		
		assert(el);
		es_hd = el->es_head;
		assert (es_hd);
		struct pollfd sockets[MAX_NODES] = {0};
		int socket_counter = 0;
	
		printf("[es] >>>>>>>>>> Dispatch next event set <<<<<<<<<<<<<\n");
		for (es=es_hd->next ; es!=es_hd ; es=es->next) {
			printf("[es] Dispatching next event ... \n");
			
			print_event(es);
			
			dispatch_event(es);
		}
		
		
		print_rt();

		bool update_flag = false;
		for (struct rte *i = get_rt_head()->next; i != get_rt_head(); i= i->next){
			update_flag = false;
			node me = get_myid();
			node dest = i->d;
			node nh = i->nh;
			char dest_string[8] = {0};
			char nh_string[8] = {0};

			if(me < dest){
				get_link_name(me, dest, dest_string);
			}else{
				get_link_name(dest, me, dest_string);
			}

			if(me < nh){
				get_link_name(me, nh, nh_string);
			}else{
				get_link_name(nh, me, nh_string);
			}

			if(dest == nh){
				struct link *temp = find_link(nh_string);
				if(temp == 0x0 && old_link[dest] != 0x0){
					printf("dd\n");
					update_rte(dest, temp_neg, dest);
					update_flag = true;
				}else if(temp!= 0x0 && old_link[dest] != temp->c){
					
					update_rte(dest, temp->c, dest);
					update_flag = true;
				}
			}else{
				struct link *dest_lk = find_link(dest_string);
				struct link *nh_lk = find_link(nh_string);
				if(dest_lk == 0x0){
					if(nh_lk == 0x0){
						update_rte(dest, temp_neg, dest);
						update_flag = true;
					}else{
						uint8_t old_nh_cost = old_link[nh];
						uint8_t new_total_cost = i->c - old_nh_cost + nh_lk->c;
						if(old_nh_cost != nh_lk->c){
							update_rte(dest, new_total_cost, nh);
							update_flag = true;
						}
					}
				}else{
					if(nh_lk == 0x0){
						update_rte(dest, dest_lk->c, dest);
						update_flag = true;
					}else{
						uint8_t old_nh_cost = old_link[nh];
						uint8_t new_total_cost = i->c - old_nh_cost + nh_lk->c;
						if(dest_lk -> c < new_total_cost){
							update_rte(dest, dest_lk->c, dest);
							update_flag = true;
						}else{
							if(old_nh_cost != nh_lk->c){
								update_rte(dest, new_total_cost, nh);
								update_flag = true;
							}
						}
					}
				}
			}
			if(update_flag == false){
				break;
			}
			if(!verb){
				print_rte(find_rte(dest));
			}else{
				print_rt();
			}
		}
		
		for(unsigned int i = 0; i<=MAX_NODES;i++){
			node me = get_myid();
			if(i == me){
				continue;
			}
			char dest_string[8] = {0};
			if(me < i){
				get_link_name(me, i, dest_string);
			}else{
				get_link_name(i, me, dest_string);
			}
			
			struct link *temp = find_link(dest_string);
			
			if(temp == 0x0){
				old_link[i] = temp_neg;
			}else{
				old_link[i] = temp->c;
				
			}
		}
		
		node nodes[MAX_NODES];
		//Set  up poll for all links in this noded
		for (struct link *i = get_head()->next; i != get_head(); i= i->next){
			if (i->sockfd0 != -1){
				sockets[socket_counter].fd = i->sockfd0;
				sockets[socket_counter].events = POLLIN;
				nodes[socket_counter] = i->peer1;
				socket_counter ++;
			}else if(i->sockfd1 != -1){
				sockets[socket_counter].fd = i->sockfd1;
				sockets[socket_counter].events = POLLIN;
				nodes[socket_counter] = i->peer0;
				socket_counter ++;
			}
		}		
	
		//Running distance vector
		for(;;){
			// if(nextSet == false){
			// 	nextSet = true;
			// 	break;
			// }
			if (poll(sockets, socket_counter, 0) > 0){
				for (int i = 0; i< socket_counter; i++){
					if(sockets[i].revents && POLLIN){
						uint8_t buffer[MAX_BUF_LEN];
						struct sockaddr_storage clntAddr; 
						socklen_t clntAddrLen = sizeof(clntAddr);
						recvfrom(sockets[i].fd, buffer, MAX_BUF_LEN, 0, (struct sockaddr *) &clntAddr, &clntAddrLen); 
						
						uint16_t num_updates;
						memcpy(&num_updates,buffer+2,2);
						num_updates = ntohs(num_updates);

						uint8_t dest;
						uint32_t min_cost = 0;
					
						for(int j = 0; j < num_updates; j++){
							memcpy(&dest, buffer+4+4*j, 1);
							memcpy(&min_cost, buffer+4+4*j+1, 3);
							min_cost = min_cost >> 8;
							min_cost = ntohl(min_cost);
							
							printf("received update dest %d mincost %d\n", dest, min_cost);
							struct rte *r = find_rte(nodes[i]);
							//Get the new link cost
							if(get_myid() == dest){
								continue;
							}
							
							if(r->c > min_cost + old_link[nodes[i]]){
								update_rte(dest, min_cost + old_link[nodes[i]], nodes[i]);
								if(!verb){
									print_rte(find_rte(dest));
								}else{
									print_rt();
								}
							}
							min_cost = 0;
						}
					}
				}
			}

			if(flag == true){
				for (struct link *i = get_head()->next; i != get_head(); i= i->next){
					if(i->sockfd0 == -1 && i->sockfd1 == -1){
						continue;
					}
					int socket;
					int destport;
					node n;
					if (i->sockfd0 != -1){
						socket = i->sockfd0;
						destport = i->port1;
						n = i->peer1;
					}else if(i->sockfd1 != -1){
						socket = i->sockfd1;
						destport = i->port0;
						n = i->peer0;
					}
					
					struct addrinfo addrCriteria; // Criteria for address match
					memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out structure
					addrCriteria.ai_family = AF_UNSPEC; // Any address family
					addrCriteria.ai_socktype = SOCK_DGRAM; // Only datagram sockets
					addrCriteria.ai_protocol = IPPROTO_UDP; // Only UDP protocol
					struct addrinfo *servAddr; // List of server addresses
					char port_str[1025] = {0};
					sprintf(port_str, "%d", destport);
					int rtnVal = getaddrinfo(gethostbyname(gethostbynode(n))->h_name, port_str, &addrCriteria, &servAddr);
					if (rtnVal != 0)
						fprintf(stderr, "getaddrinfo failed\n");
					
					uint8_t buffer[1024] = {0};
					int num_entry = send_rt(buffer);
					uint8_t new_buffer[(num_entry +1)*4];
					
					memcpy(new_buffer, buffer, (num_entry +1)*4);
					sendto(socket, buffer, (num_entry +1)*4, 0, servAddr->ai_addr, servAddr->ai_addrlen);
				}
				flag = false;
			}
			
			if(timer >= time_between){
				timer = 0;
				break;
			}
		}

	}
}



/*
 * -------------------------------------
 * Dispatch one event
 * -------------------------------------
 */
void dispatch_event(struct es* es)
{
	assert(es);

	switch (es->ev) {
		case _es_link:
			add_link(es->peer0, es->port0, es->peer1, es->port1,
				es->cost, es->name);
			break;
		case _ud_link:
			ud_link(es->name, es->cost);
			break;
		case _td_link:
			del_link(es->name);
			break;
		default:
			printf("[es]\t\tUnknown event!\n");
			break;
	}

}

/*
 * print out the whole event LIST
 */
void print_el()
{
	struct el *el;
	struct es *es_hd;
	struct es *es;

	assert (g_el->next);

	printf("\n\n");
	printf("[es] >>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<\n");
	printf("[es] >>>>>>>>>> Dumping all event sets  <<<<<<<<<<<<<\n");
	printf("[es] >>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<\n");

	for (el = g_el->next ; el != g_el ; el = el->next) {

		assert(el);
		es_hd = el->es_head;
		assert (es_hd);
	
		printf("\n[es] ***** Dumping next event set *****\n");

		for (es=es_hd->next ; es!=es_hd ; es=es->next)
			print_event(es);
		
	}     
}

/*
 * print out one event: establish, update, or, teardown
 */
void print_event(struct es* es)
{
	assert(es);

	switch (es->ev) {
		case _es_null:
			printf("[es]\t----- NULL event -----\n");
			break;
		case _es_link:
			printf("[es]\t----- Establish event -----\n");
			break;
		case _ud_link:
			printf("[es]\t----- Update event -----\n");
			break;
		case _td_link:
			printf("[es]\t----- Teardown event -----\n");
			break;
		default:
			printf("[es]\t----- Unknown event-----\n");
			break;
	}
	printf("[es]\t link-name(%s)\n",es->name);
	printf("[es]\t node(%d)port(%d) <--> node(%d)port(%d)\n", 
		es->peer0,es->port0,es->peer1,es->port1);
	printf("[es]\t cost(%d)\n", es->cost);
}

struct es *geteventbylink(char *lname)
{
	struct el *el;
	struct es *es_hd;
	struct es *es;

	assert (g_el->next);
	assert (lname);

	for (el = g_el->next ; el != g_el ; el = el->next) {

		assert(el);
		es_hd = el->es_head;
		assert (es_hd);
	
		for (es=es_hd->next ; es!=es_hd ; es=es->next)
			if (!strcmp(lname, es->name))
				return es;
	}
	return 0x0;
}

#endif