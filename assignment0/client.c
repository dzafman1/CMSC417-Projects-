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

struct client_arguments {
	char ip_address[16]; /* You can store this as a string, but I probably wouldn't */
	int port; /* is there already a structure you can store the address
	           * and port in instead of like this? */
	int hashnum;
	int smin;
	int smax;
	char *filename; /* you can store this as a string, but I probably wouldn't */
};

error_t client_parser(int key, char *arg, struct argp_state *state) {
	struct client_arguments *args = state->input;
	error_t ret = 0;
	int len;
	switch(key) {
	case 'a':
		/* validate that address parameter makes sense */
		strncpy(args->ip_address, arg, 16);
		if (0 /* ip address is goofytown */) {
			argp_error(state, "Invalid address");
		}
		break;
	case 'p':
		/* Validate that port is correct and a number, etc!! */
		args->port = atoi(arg);
		if (0 /* port is invalid */) {
			argp_error(state, "Invalid option for a port, must be a number");
		}
		break;
	case 'n':
		/* validate argument makes sense */
		args->hashnum = atoi(arg);
		break;
	case 300:
		/* validate arg */
		args->smin = atoi(arg);
		break;
	case 301:
		/* validate arg */
		args->smax = atoi(arg);
		break;
	case 'f':
		/* validate file */
		len = strlen(arg);
		args->filename = malloc(len + 1);
		strcpy(args->filename, arg);
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
    struct argp_option options[] = {
		{ "addr", 'a', "addr", 0, "The IP address the server is listening at", 0},
		{ "port", 'p', "port", 0, "The port that is being used at the server", 0},
		{ "hashreq", 'n', "hashreq", 0, "The number of hash requests to send to the server", 0},
		{ "smin", 300, "minsize", 0, "The minimum size for the data payload in each hash request", 0},
		{ "smax", 301, "maxsize", 0, "The maximum size for the data payload in each hash request", 0},
		{ "file", 'f', "file", 0, "The file that the client reads data from for all hash requests", 0},
		{0}
	};

	struct argp argp_settings = { options, client_parser, 0, 0, 0, 0, 0 };

	struct client_arguments args;
	bzero(&args, sizeof(args));

	if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0) {
		printf("Got error in parse\n");
	}

	/* If they don't pass in all required settings, you should detect
	 * this and return a non-zero value from main */
	printf("Got %s on port %d with n=%d smin=%d smax=%d filename=%s\n",
	       args.ip_address, args.port, args.hashnum, args.smin, args.smax, args.filename);

    int clientSock;

    if ((clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
        printf("socket creation failed\n");
    }
    
    struct sockaddr_in servAddr; // Server address
    memset(&servAddr, 0, sizeof(servAddr)); // Zero out structure
    servAddr.sin_family = AF_INET; // IPv4 address family
    inet_pton(AF_INET, args.ip_address, &servAddr.sin_addr.s_addr);
    servAddr.sin_port = htons(args.port);

    if (connect(clientSock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0){
        puts("client socket connection failed");
    }

    int32_t type = 1;
    int32_t num_req = args.hashnum;
    int32_t total_len = 0;

    //Sending out initialization
    send_n_bytes(&type, 4, clientSock);
    send_n_bytes(&num_req, 4, clientSock);

    //Reading the achknowledgement
    read_n_bytes(&type, 4, clientSock);
    read_n_bytes(&total_len, 4, clientSock);
    printf("The total length sent out by server is %d\n", total_len);
    srand(time(NULL));
    FILE *file = fopen (args.filename , "r");
    int counter;
    for(counter = 0; counter < args.hashnum; counter++){
        int32_t random_len =  rand()%(args.smin-args.smax) + args.smin;
        void *send_buffer = malloc(random_len);
        fread(send_buffer, random_len, 1, file);

		int32_t hashreq_type = 3;
		send_n_bytes(&hashreq_type, 4, clientSock);
		send_n_bytes(&random_len, 4, clientSock);
        send_n_bytes(send_buffer, random_len, clientSock);
        char trash_buffer[8] = {0};
        uint8_t receive_buffer[33] = {0};

        read_n_bytes(trash_buffer, 8, clientSock);
        read_n_bytes(receive_buffer, 32, clientSock);

		printf("%d: 0x", counter+1);
		for(int i = 0; i < 32; ++i) {
			printf("%02x",  receive_buffer[i]);
		}
		putchar('\n');
        
        free(send_buffer);
    }
    fclose(file);
    close(clientSock);
}