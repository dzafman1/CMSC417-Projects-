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
#include "hash.h"

struct server_arguments {
	int port;
	char *salt;
	size_t salt_len;
};

error_t server_parser(int key, char *arg, struct argp_state *state) {
	struct server_arguments *args = state->input;
	error_t ret = 0;
	switch(key) {
	case 'p':
		/* Validate that port is correct and a number, etc!! */
		args->port = atoi(arg);
		if (0 /* port is invalid */) {
			argp_error(state, "Invalid option for a port, must be a number");
		}
		break;
	case 's':
		args->salt_len = strlen(arg);
		args->salt = malloc(args->salt_len+1);
		strcpy(args->salt, arg);
		break;
	default:
		ret = ARGP_ERR_UNKNOWN;
		break;
	}
	return ret;
}

void read_n_bytes(void *buffer, size_t bytes_expected, int read_socket){
    size_t bytes_received = 0;
    size_t temp = 0;

    while(bytes_received < bytes_expected){
        temp = recv(read_socket, buffer + bytes_received, bytes_expected-bytes_received, 0);
        bytes_received += temp;
    }
}

void send_n_bytes(void *buffer, size_t bytes_expected, int send_socket){
    size_t bytes_sent = 0;
    size_t temp = 0;

    while(bytes_sent < bytes_expected){
        temp = send(send_socket, buffer + bytes_sent, bytes_expected-bytes_sent, 0);
        bytes_sent += temp;
    }
}

int main(int argc, char *argv[]){
    struct server_arguments args;

	bzero(&args, sizeof(args));

	struct argp_option options[] = {
		{ "port", 'p', "port", 0, "The port to be used for the server" ,0},
		{ "salt", 's', "salt", 0, "The salt to be used for the server. Zero by default", 0},
		{0}
	};
	struct argp argp_settings = { options, server_parser, 0, 0, 0, 0, 0 };
	if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0) {
         fprintf(stderr, "Got an error condition when parsing\n");
	}

    fprintf(stderr, "Got port %d and salt %s with length %ld\n", args.port, args.salt, args.salt_len);
    
    int servSock; // Socket descriptor for server
    if ((servSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
        fprintf(stderr, "socket creation failed\n");
    }

    // Construct local address structure
    struct sockaddr_in servAddr; // Local address
    memset(&servAddr, 0, sizeof(servAddr)); // Zero out structure
    servAddr.sin_family = AF_INET; // IPv4 address family
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY); // Any incoming interface
    servAddr.sin_port = htons(args.port); // Local port
    
    // Bind the socket to the socketAddr
    if (bind(servSock, (struct sockaddr*) &servAddr, sizeof(servAddr)) < 0){
        fprintf(stderr, "Binding socket failed\n");
    }

    //listen to at most five clients
    if (listen(servSock, 5) < 0){
        fprintf(stderr, "listening failed\n");
    }

    for(;;){
        struct sockaddr_in clntAddr; // Client address
        // Set length of client address structure (in-out parameter)
        socklen_t clntAddrLen = sizeof(clntAddr);

        int clntSock = accept(servSock, (struct sockaddr *) &clntAddr, &clntAddrLen);
        if(clntSock < 0){
            fprintf(stderr, "accepting socket failed\n");
        }else{
            if (fork() == 0){
                //child
                char clntName[INET_ADDRSTRLEN]; // String to contain client address
                if (inet_ntop(AF_INET, &clntAddr.sin_addr.s_addr, clntName, sizeof(clntName)) != NULL){
                    fprintf(stderr, "Handling client %s/%d\n", clntName, ntohs(clntAddr.sin_port));
                }else{
                    fprintf(stderr, "Unable to get client address\n");
                }
                
                //Receving client initialization
                int32_t type;
                read_n_bytes(&type, 4, clntSock);
                int32_t hashreq_num;
                read_n_bytes(&hashreq_num, 4, clntSock);;
                fprintf(stderr, "Server will get %d hash Requests\n", ntohl(hashreq_num));
                
                //Sending out achkonwledgement
                uint32_t type_nb = htonl(2);
                uint32_t reqnum_nb = htonl(40*hashreq_num);
                send_n_bytes(&type_nb, 4, clntSock);
                send_n_bytes(&reqnum_nb, 4, clntSock);

                int32_t counter;
                uint32_t mes_length;
                struct checksum_ctx *ctx;
                if(args.salt_len == 0){
                    ctx = checksum_create(NULL, 0);
                }else{
                    ctx = checksum_create(((uint8_t *)args.salt), sizeof(args.salt)-1);
                }
                if (!ctx) {
                    fprintf(stderr, "Error creating checksum\n");
                    return 0;
                }
                for(counter = 0; counter < hashreq_num; counter++){
                    //Hash request
                    read_n_bytes(&type, 4, clntSock);
                    read_n_bytes(&mes_length, 4, clntSock);
                    mes_length = ntohl(mes_length);

                    void *read_buffer = NULL;
                    int update_times= 0;
                    if(mes_length <= 4096){
                        read_buffer = calloc(1, mes_length);
                        read_n_bytes(read_buffer, mes_length, clntSock);
                    }else{
                        read_buffer = calloc(1, 4096);
                        int remainder = mes_length%4096;
                        update_times = (mes_length-remainder)/4096;
                        for(int i = 0; i < update_times; i++){
                            read_n_bytes(read_buffer, 4096, clntSock);
                            checksum_update(ctx, read_buffer);
                        }
                        free(read_buffer);
                        read_buffer = calloc(1,remainder);
                        read_n_bytes(read_buffer, remainder, clntSock);
                    }
                    
                    uint8_t checksum[32];
                    checksum_finish(ctx, read_buffer, mes_length-4096*update_times, checksum);
                    checksum_reset(ctx);

                    //Hash response
                    uint32_t counter_nb = htonl(counter);
                    type_nb = htonl(4);
                    send_n_bytes(&type_nb, 4, clntSock);
                    send_n_bytes(&counter_nb, 4, clntSock);
                    send_n_bytes(checksum, 32, clntSock);

                    free(read_buffer);
                }
                checksum_destroy(ctx);
                close(clntSock);
                exit(0);
            }else{
                close(clntSock);
            }
        }
        free(args.salt);
    }
}