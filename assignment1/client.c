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
#include <time.h>
#include <netdb.h>
#include <math.h>

struct client_arguments {
	char addr[16];
	int port; 
	int reqnum;
	int timeout;
};

typedef struct mes {
    uint32_t seq;
    float theta;
    float delta;
}Mes;

error_t client_parser(int key, char *arg, struct argp_state *state) {
	struct client_arguments *args = state->input;
	error_t ret = 0;
	switch(key) {
	case 'a':
		strncpy(args->addr, arg, 16);
		break;
	case 'p':
		/* Validate that port is correct and a number, etc!! */
		args->port = atoi(arg);
		break;
	case 'n':
		/* validate argument makes sense */
		args->reqnum = atoi(arg);
		break;
	case 't':
		/* validate argument makes sense */
		args->timeout = atoi(arg);
		break;
	default:
		ret = ARGP_ERR_UNKNOWN;
		break;
	}
	return ret;
}

int main(int argc, char *argv[]){
    struct argp_option options[] = {
		{ "addr", 'a', "addr", 0, "The IP address the server is listening at", 0},
		{ "port", 'p', "port", 0, "The port that is being used at the server", 0},
		{ "reqnum", 'n', "reqnum", 0, "The number of requests to send to the server", 0},
		{ "timeout", 't', "timeout", 0, "The client timeout time", 0},
		{0}
	};

	struct argp argp_settings = { options, client_parser, 0, 0, 0, 0, 0 };

	struct client_arguments args;
	bzero(&args, sizeof(args));

	if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0) {
		fprintf(stderr, "Got error in parse\n");
	}


    if(!strcmp("", args.addr)){
        fprintf(stderr, "IP addr must be specified\n");
        abort();
    }
    if(args.reqnum < 0){
        fprintf(stderr, "Num of TimeRequests must >=0\n");
        abort();
    }
    if(args.port <= 1024){
        fprintf(stderr, "You must use a port > 1024\n");
        abort();
    }
    
    
    fprintf(stderr, "Got %s on port %d with reqnum=%d timeout=%d\n",
        args.addr, args.port, args.reqnum, args.timeout);
           
    struct addrinfo addrCriteria; // Criteria for address match
    memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out structure
    addrCriteria.ai_family = AF_UNSPEC; // Any address family

    addrCriteria.ai_socktype = SOCK_DGRAM; // Only datagram sockets
    addrCriteria.ai_protocol = IPPROTO_UDP; // Only UDP protocol

    struct addrinfo *servAddr; // List of server addresses
    char port_str[1025] = {0};
    sprintf(port_str, "%d", args.port);
    int rtnVal = getaddrinfo(args.addr, port_str, &addrCriteria, &servAddr);
    if (rtnVal != 0)
        fprintf(stderr, "getaddrinfo failed\n");

    int sock = socket(servAddr->ai_family, servAddr->ai_socktype, servAddr->ai_protocol); // Socket descriptor for client
    if (sock < 0)
        fprintf(stderr, "socket failed\n");

    for(int i = 0; i < args.reqnum; i++){
        char buffer[22];
        uint32_t seq = i+1;
        uint16_t ver = 7;
        struct timespec tspec;
        clock_gettime(CLOCK_REALTIME,&tspec);
        time_t sec = tspec.tv_sec;
        long nanosec = tspec.tv_nsec;

        seq = htonl(seq);
        ver = htons(ver);
        uint64_t sec_nb = htobe64(sec);
        uint64_t nanosec_nb = htobe64(nanosec);

        memcpy(buffer, &seq, 4);
        memcpy(buffer+4, &ver, 2);
        memcpy(buffer+6, &sec_nb, 8);
        memcpy(buffer+14, &nanosec_nb, 8);

        sendto(sock, buffer, 22,0,servAddr->ai_addr, servAddr->ai_addrlen);
    }

    struct timeval timeout={args.timeout,0}; 
    setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval));
    struct sockaddr_storage fromAddr; // Source address of server
    socklen_t fromAddrLen = sizeof(fromAddr);

    Mes messages[args.reqnum];
    memset(messages, 0, args.reqnum*sizeof(Mes));

    for(int i = 0; i < args.reqnum; i++){
        char buffer[38]; 
        int recvlen = recvfrom(sock, buffer, 38, 0, (struct sockaddr *)&fromAddr, &fromAddrLen);
        if (recvlen >= 0) {
            struct timespec tspec;
            clock_gettime(CLOCK_REALTIME,&tspec);
            time_t client_sec2 = tspec.tv_sec;
            long client_nanosec2 = tspec.tv_nsec;

            uint32_t seq;
            memcpy(&seq, buffer,4);
            uint16_t ver;
            memcpy(&ver, buffer+4,2);
            uint64_t client_sec1;
            memcpy(&client_sec1, buffer+6,8);
            uint64_t client_nanosec1;
            memcpy(&client_nanosec1, buffer+14,8);
            uint64_t serv_sec;
            memcpy(&serv_sec, buffer+22,8);
            uint64_t serv_nanosec;
            memcpy(&serv_nanosec, buffer+30,8);
           
            seq = ntohl(seq);
            ver = ntohs(ver);
            client_sec1 = be64toh(client_sec1);
            client_nanosec1 = be64toh(client_nanosec1);
            serv_sec = be64toh(serv_sec);
            serv_nanosec = be64toh(serv_nanosec);

            if(messages[seq-1].seq != 0){
                //Duplicate TimeResponse
                i--;
            }else{
                long double temp = client_nanosec1/pow(10,9);
                long double t0 = client_sec1 + temp;
                temp = serv_nanosec/pow(10,9);
                long double t1 = serv_sec + temp;
                temp = client_nanosec2/pow(10,9);
                long double t2 = client_sec2 + temp;
        
                float theta = (t1-t0 + t1 - t2)/2;
                float delta = t2 - t0;
                messages[seq-1].seq = seq;
                messages[seq-1].delta = delta;
                messages[seq-1].theta = theta;
            }
        }
        else{
            break;
        }
    }

    close(sock);

    for(int i =0;i < args.reqnum; i++){
        if(messages[i].seq == 0)
            printf("%d: Dropped\n", i+1);
        else
            printf("%d: %.4f %.4f\n", i+1, messages[i].theta, messages[i].delta);
    }
}