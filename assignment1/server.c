#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <argp.h>
#include <unistd.h> 
#include <netdb.h>
#include <endian.h>
#include <math.h>

struct server_arguments {
	int port;
	int drop_percent;
};

typedef struct listnode {
    time_t last_update;
    int seq;
    char addr[1025];
    char port[1025];
    struct listnode *next;
}Node;

Node *update_list(Node *head, char *addr, char *port, int new_seq, time_t cur_time)
{
    if(head == NULL){
        Node *newnode = calloc(sizeof(Node), 1);
        newnode->seq = new_seq;
        newnode->next = NULL;
        newnode->last_update = cur_time;
        strcpy(newnode->addr, addr);
        strcpy(newnode->port, port);
        return newnode;
    }
    
    Node *current = head;
    Node *prev = NULL;
    int flag = 0;

    while(current != NULL)
    {   
        if(!strcmp(current->addr, addr) && !strcmp(current->port, port)){
            flag = 1;
            if(difftime(cur_time, current->last_update) >= 120){
                current->seq = new_seq;
                current->last_update = cur_time;
            }else{
                if(current->seq > new_seq){
                    printf("%s:%s %d %d\n", current->addr, current->port, new_seq, current->seq);
                }else{
                    current->seq = new_seq;
                    current->last_update = cur_time;
                }
            }
        }else{
            if(difftime(cur_time, current->last_update) >= 120){
                current->seq = 0;
                current->last_update = cur_time;
            }
        }
        prev = current ;
        current = current -> next;
    }
    
    if(!flag){;
        //not connected before
        Node *newnode = calloc(sizeof(Node), 1);
        newnode->seq = new_seq;
        newnode->next = NULL;
        newnode->last_update = cur_time;
        strcpy(newnode->addr, addr);
        strcpy(newnode->port, port);
        prev->next = newnode;
    }

    return head;
}

error_t server_parser(int key, char *arg, struct argp_state *state) {
	struct server_arguments *args = state->input;
	error_t ret = 0;
	switch(key) {
        case 'p':
            args->port = atoi(arg);
            break;
        case 'd':
            args->drop_percent = atoi(arg);
            break;
        default:
            ret = ARGP_ERR_UNKNOWN;
            break;
	}
	return ret;
}

int main(int argc, char *argv[]){
    //Parsing command line arguments
    struct server_arguments args;
	bzero(&args, sizeof(args));
	struct argp_option options[] = {
		{ "port", 'p', "port", 0, "The port to be used for the server" ,0},
		{ "drop_percent", 'd', "drop_percent", 0, "The percent a packet to be dropped" ,0},
        {0}
	};
	struct argp argp_settings = { options, server_parser, 0, 0, 0, 0, 0 };
	if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0) {
         fprintf(stderr, "Got an error condition when parsing\n");
	}       

    if(args.port <= 1024){
        fprintf(stderr, "You must use a port > 1024\n");
        abort();
    }

    fprintf(stderr, "Got port %d and drop percent %d\n", args.port, args.drop_percent);

    struct addrinfo addrCriteria; // Criteria for address
    memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out structure
    addrCriteria.ai_family = AF_UNSPEC; // Any address family
    addrCriteria.ai_flags = AI_PASSIVE; // Accept on any address/port
    addrCriteria.ai_socktype = SOCK_DGRAM; // Only datagram socket
    addrCriteria.ai_protocol = IPPROTO_UDP; // Only UDP socket

    struct addrinfo *servAddr; // List of server addresses
    char port_str[1025] = {0};
    sprintf(port_str, "%d", args.port);
    int rtnVal = getaddrinfo(NULL, port_str, &addrCriteria, &servAddr);
    if (rtnVal != 0)
        fprintf(stderr, "getaddrinfo() failed\n");

    // Create socket for incoming connections
    int sock = socket(servAddr->ai_family, servAddr->ai_socktype, servAddr->ai_protocol);
    if (sock < 0)
        fprintf(stderr, "socket() failed\n");

    // Bind to the local address
    if (bind(sock, servAddr->ai_addr, servAddr->ai_addrlen) < 0)
        fprintf(stderr, "bind() failed\n");

    // Free address list allocated by getaddrinfo()
    freeaddrinfo(servAddr);

    srand(time(NULL));
    Node *list = NULL;
    for(;;){
        struct sockaddr_storage clntAddr; // Client address
        // Set Length of client address structure (in-out parameter)
        socklen_t clntAddrLen = sizeof(clntAddr);

        // Block until receive message from a client
        char buffer[22]; // I/O buffer
        // Size of received message
        recvfrom(sock, buffer, 22, 0, (struct sockaddr *) &clntAddr, &clntAddrLen); 

        int random = ((rand()%(100+1)+0));
        
        if(args.drop_percent == 0 || random > args.drop_percent){
            struct timespec tspec;
            clock_gettime(CLOCK_REALTIME,&tspec);
            time_t serv_sec = tspec.tv_sec;
            long serv_nanosec = tspec.tv_nsec;

            socklen_t client_len = sizeof(struct sockaddr_storage);
            char client_addr[NI_MAXHOST] ={0};
            char client_port[NI_MAXSERV] ={0};
            getnameinfo((struct sockaddr *)&clntAddr, client_len, client_addr, sizeof(client_addr), client_port, sizeof(client_port), NI_NUMERICHOST | NI_NUMERICSERV);

            uint32_t client_seq;
            memcpy(&client_seq, buffer, 4);
            client_seq = ntohl(client_seq); 
            uint64_t client_sec;
            memcpy(&client_sec, buffer+6, 8);
            client_sec = be64toh(client_sec);
            uint64_t client_nanosec;
            memcpy(&client_nanosec,buffer+14,  8);
            client_nanosec = be64toh(client_nanosec);
            list = update_list(list, client_addr, client_port, client_seq, serv_sec);

            char buffer[38];
            client_seq = htonl(client_seq); 
            client_sec = htobe64(client_sec);
            client_nanosec = htobe64(client_nanosec);
            uint64_t serv_sec_nb = htobe64(serv_sec);
            uint64_t serv_nanosec_nb = htobe64(serv_nanosec);
            uint16_t ver = 7;
            ver = htons(ver);
            memcpy(buffer, &client_seq, 4);
            memcpy(buffer+4, &ver, 2);
            memcpy(buffer+6, &client_sec, 8);
            memcpy(buffer+14, &client_nanosec, 8);
            memcpy(buffer+22, &serv_sec_nb, 8);
            memcpy(buffer+30, &serv_nanosec_nb, 8);
            
            sendto(sock, buffer, 38, 0, (struct sockaddr *) &clntAddr, sizeof(clntAddr));
        }
    }
}