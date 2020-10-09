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
#include <math.h>

#define MAX_COMMAND_LEN 66000
enum commands{Connect, Quit};

int createSocket(char *ipAddr, int port){
    int clientSock;
    if ((clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
        fprintf(stderr, "socket creation failed\n");
        return -1;
    }
    
    struct sockaddr_in servAddr; // Server address
    memset(&servAddr, 0, sizeof(servAddr)); // Zero out structure
    servAddr.sin_family = AF_INET; // IPv4 address family
    inet_pton(AF_INET, ipAddr, &servAddr.sin_addr.s_addr);
    servAddr.sin_port = htons(port);

    if (connect(clientSock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0){
        fprintf(stderr, "socket connection failed\n");
        return -1;
    }
    return clientSock;
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

int main(){
    int connected = 0, socket = -1;
    
    while(1){
        char command[MAX_COMMAND_LEN] = {0};
        fgets (command, MAX_COMMAND_LEN, stdin);
        
        if(connected){
            /*
                if command == connect
                    Print: Already connected to a chat server.
            */
        }else{
            /*if the command is connect
                if client can be established
                    Send "Greetings...", get your name assigned by the server
                    Print: Connected to the chat server. Your name is 'rand0'.
                else
                    Print: Not connected to a chat server.
                endif
              else
                Print: Not connected to a chat server.
              endif
            */
           if(strstr(command, "\\connect") == command){
               char *colon = strchr(command, ':');
               uint16_t port = atoi(colon+1);
               char addr[16] = {0};
               strncpy(addr, command+9, colon-command-9);
                
               socket = createSocket(addr, port);
               if(socket >= 0){
                   uint8_t buffer[48] = {0,0,0,0x29,0x04,0x17,0x9b,'G','r','e','e','t','i','n','g','s',
                   0x20,'f','r','o','m',0x20,'t','h','e',0x20, 'l','a','n','d',0x20,'o','f',0x20,
                   'm','i','l','k',0x20,'a','n','d',0x20,'h','o','n','i','g'};

                   send_n_bytes(buffer, 48, socket); 

                   uint8_t readBuf[13]

               }else{
                   printf("Not connected to a chat server.\n");
               }
           }else if(strstr(command, "\\quit") == command){
               printf("Quitting...\n");
               break;
           }else{
               printf("Not connected to a chat server.\n");
           }
        }
        
    }
    if(socket >= 0)
        close(socket);
}