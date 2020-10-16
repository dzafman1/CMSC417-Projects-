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
#include <poll.h>
#include <signal.h>

#define MAX_COMMAND_LEN 66000

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

void read_n_bytes(void *buffer, int bytes_expected, int read_socket){
    int bytes_received = 0;
    int temp = 0;

    while(bytes_received < bytes_expected){
        temp = recv(read_socket, buffer + bytes_received, bytes_expected-bytes_received, 0);
        if(temp == -1){
            fprintf(stderr, "read failed\n");
            break;
        }
        bytes_received += temp;
    }

}

void send_n_bytes(void *buffer, int bytes_expected, int send_socket){
    int bytes_sent = 0;
    int temp = 0;

    while(bytes_sent < bytes_expected){
        temp = send(send_socket, buffer + bytes_sent, bytes_expected-bytes_sent, 0);
        if(temp == -1){
            printf("%d\n", send_socket);
            fprintf(stderr, "send failed\n");
            break;
        }
        bytes_sent += temp;
    }
}

void sendNick(char *command, char *name, int socket){
    char nickname[256] = {0};
    strcpy(nickname, command+6);
    uint32_t namLen = strlen(nickname);
    nickname[--namLen] = 0;
    
    if(namLen > 255){
        printf("Invalid command.\n");
    }else{
        uint8_t buffer[namLen + 8];

        uint32_t connectFlag = 0x04170f;
        uint8_t t0[4], t1[3];
        memcpy(t0, &connectFlag, 4);
        t1[0] = t0[2];
        t1[1] = t0[1];
        t1[2] = t0[0];
        memcpy(buffer + 4, t1, 3);

        uint8_t len_eight = namLen;
        memcpy(buffer + 7, &len_eight, 1);
        
        memcpy(buffer+8, nickname, namLen);
    
        namLen++;
        namLen = htonl(namLen);
        memcpy(buffer, &namLen, 4);
        send_n_bytes(buffer, len_eight+8, socket);
        
        uint8_t trash[8];
        read_n_bytes(&trash, 8, socket);
        strcpy(name, nickname);
        printf("Changed nick to '%s'.\n", nickname);
    }
}

void startConnection(char *command, char *nameF, int *connected, int *socket){
    char *colon = strchr(command, ':');
    uint16_t port = atoi(colon+1);
    char addr[16] = {0};
    strncpy(addr, command+9, colon-command-9);
    
    *socket = createSocket(addr, port);
    if(*socket >= 0){
        *connected = 1;
        uint8_t buffer[48] = {0};
        uint32_t connectLength = 0x29;
        connectLength = htonl(connectLength);
        memcpy(buffer, &connectLength, 4);
        
        uint32_t connectFlag = 0x04179b;
        uint8_t t0[4], t1[3];
        memcpy(t0, &connectFlag, 4);
        t1[0] = t0[2];
        t1[1] = t0[1];
        t1[2] = t0[0];
        memcpy(buffer + 4, t1, 3);

        char string[42] = {0};
        strcpy(string, "Greetings from the land of milk and honig");
        memcpy(buffer + 7, string, 41);
        send_n_bytes(buffer, 48, *socket);
        
        uint32_t nameLength, trash;
        read_n_bytes(&nameLength, 4, *socket);
        nameLength = ntohl(nameLength);
        char name[nameLength];
        memset(name,0,nameLength);

        read_n_bytes(&trash, 4, *socket);
        read_n_bytes(name, nameLength-1, *socket);
        strcpy(nameF, name);
        printf("Connected to the chat server. Your name is '%s'.\n", name);
    }else{
        printf("Could not connect to the chat server. (Invalid argument)\n");
    }
}

void sendJoin(char *command, char *oldRoom, int *inARoom, int socket){
    char password[256] = {0}, room[256] = {0};

    char *firstSpace = strchr(command, ' ');
    char *secondSpace = strchr(firstSpace + 1, ' ');
    if(secondSpace == command + strlen(command)){
        //no pasword
        strcpy(room, firstSpace + 1);
        room[strlen(room-1)] = 0;
    }else{
        strncpy(room, firstSpace + 1, secondSpace-firstSpace-1);
        strcpy(password, secondSpace+1);
        password[strlen(password)-1] = 0;
    }

    uint32_t totalLen = 2+strlen(password) + strlen(room);
    uint8_t passwordLen = strlen(password), roomLen = strlen(room);
    uint8_t buffer[7+totalLen];
    uint32_t tempTotalLen = totalLen;
    tempTotalLen = htonl(tempTotalLen);
    memcpy(buffer, &tempTotalLen, 4);

    uint32_t joinFlag = 0x041703;
    uint8_t t0[4], t1[3];
    memcpy(t0, &joinFlag, 4);
    t1[0] = t0[2];
    t1[1] = t0[1];
    t1[2] = t0[0];
    memcpy(buffer + 4, t1, 3);

    buffer[7] = roomLen;
    memcpy(buffer + 8, room, roomLen);
    buffer[8 + roomLen] = passwordLen;
    if(passwordLen == 0){
        send_n_bytes(buffer, 7+totalLen, socket);
    }else{
        memcpy(buffer + 9 + roomLen, password, passwordLen);
        send_n_bytes(buffer, 7+totalLen, socket);
    }

    uint32_t returnLen;
    read_n_bytes(&returnLen, 4, socket);
    returnLen = ntohl(returnLen);
    if(returnLen == 1){
        uint8_t trash[4];
        read_n_bytes(trash, 4, socket);
        *inARoom = 1;

        memset(oldRoom, 0, 256);
        strcpy(oldRoom, room);
        printf("Joined room '%s'.\n", room);
    }else{
        uint8_t trash[4];
        uint8_t talk[returnLen];
        read_n_bytes(trash, 4, socket);
        read_n_bytes(talk, returnLen-1, socket);
        talk[returnLen] = 0;
        printf("Command failed. (%s)\n", talk);
    }
}

void sendLeave(char *room, int *connected, int *inARoom, int socket){
    uint8_t buffer[8];
    uint32_t len = 0;
    len = htonl(len);

    uint32_t leaveFlag = 0x041706;
    uint8_t t0[4], t1[3];
    memcpy(t0, &leaveFlag, 4);
    t1[0] = t0[2];
    t1[1] = t0[1];
    t1[2] = t0[0];
    memcpy(buffer + 4, t1, 3);

    if(*inARoom){
        *inARoom = 0;
        send_n_bytes(buffer, 7, socket);
        read_n_bytes(buffer, 8, socket);
        printf("Left room '%s'.\n", room);
        memset(room, 0, 256);
    }else{
        *connected = 0;
        send_n_bytes(buffer, 7, socket);
        read_n_bytes(buffer, 8, socket);
        close(socket);
        printf("Left the chat server.\n");
        printf("Disconnected from the chat server (Reason: Server closed the connection)\n");
    }
}

void sendListUsers(char *room, int *inARoom, int socket){
    uint8_t buffer[7];
    uint32_t sendLen = 0;
    sendLen = htonl(sendLen);
    memcpy(buffer, &sendLen, 4);
    uint32_t sendFlag = 0x04170c;
    uint8_t t0[4], t1[3];
    memcpy(t0, &sendFlag, 4);
    t1[0] = t0[2];
    t1[1] = t0[1];
    t1[2] = t0[0];
    memcpy(buffer + 4, t1, 3);

    send_n_bytes(buffer, 7, socket);

    uint32_t listenTL, trash;
    read_n_bytes(&listenTL, 4, socket);
    listenTL = ntohl(listenTL);
    //TODO: check zero users case
    read_n_bytes(&trash, 4, socket);

    uint8_t readBuf[listenTL];
    memset(readBuf, 0, listenTL);

    uint32_t bufCounter = 0, readCounter = 0;
    listenTL --;
    while(readCounter < listenTL){
        uint8_t nextSegLen;
        read_n_bytes(&nextSegLen, 1, socket);
        
        read_n_bytes(readBuf + bufCounter, nextSegLen, socket);
        bufCounter += nextSegLen;
        readCounter += nextSegLen + 1;
        if(readCounter != listenTL)
            readBuf[bufCounter] = ' ';
        bufCounter ++;
    }
    
    if(*inARoom){
        printf("List of users in room '%s': %s\n", room, readBuf);
    }else{
        printf("List of all users: %s\n", readBuf);
    }
}

void sendListRooms(int socket){
    uint8_t buffer[7];
    uint32_t sendLen = 0;
    sendLen = htonl(sendLen);
    memcpy(buffer, &sendLen, 4);
    uint32_t sendFlag = 0x041709;
    uint8_t t0[4], t1[3];
    memcpy(t0, &sendFlag, 4);
    t1[0] = t0[2];
    t1[1] = t0[1];
    t1[2] = t0[0];
    memcpy(buffer + 4, t1, 3);

    send_n_bytes(buffer, 7, socket);

    uint32_t listenTL, trash;
    read_n_bytes(&listenTL, 4, socket);
    listenTL = ntohl(listenTL);
    read_n_bytes(&trash, 4, socket);

    uint8_t readBuf[listenTL];
    memset(readBuf, 0, listenTL);

    uint32_t bufCounter = 0, readCounter = 0;
    listenTL --;
    while(readCounter < listenTL){
        uint8_t nextSegLen;
        read_n_bytes(&nextSegLen, 1, socket);
        
        read_n_bytes(readBuf + bufCounter, nextSegLen, socket);
        bufCounter += nextSegLen;
        readCounter += nextSegLen + 1;
        if(readCounter != listenTL)
            readBuf[bufCounter] = ' ';
        bufCounter ++;
    }
    
    printf("List of rooms: %s\n", readBuf);
}

void sendMessage(char *command, int socket){
    char destName[256] = {0}, msg[65536] = {0};

    char *firstSpace = strchr(command, ' ');
    char *secondSpace = strchr(firstSpace + 1, ' ');
    
    strncpy(destName, firstSpace + 1, secondSpace-firstSpace-1);
    strcpy(msg, secondSpace+1);
    msg[strlen(msg)-1] = 0;

    uint16_t mesLen = strlen(msg);
    uint8_t destNameLen = strlen(destName);

    uint32_t tl = 1+mesLen+2+destNameLen;
    uint32_t tl_n = htonl(tl);
    uint8_t buffer[7+tl];
    memcpy(buffer, &tl_n, 4);

    uint32_t sendFlag = 0x041712;
    uint8_t t0[4], t1[3];
    memcpy(t0, &sendFlag, 4);
    t1[0] = t0[2];
    t1[1] = t0[1];
    t1[2] = t0[0];
    memcpy(buffer + 4, t1, 3);

    memcpy(buffer+7, &destNameLen, 1);
    uint16_t mesLenT = htons(mesLen);
    memcpy(buffer+8, destName, destNameLen);
    memcpy(buffer+8+destNameLen, &mesLenT, 2);
    memcpy(buffer+10+destNameLen, msg, mesLen);
    
    send_n_bytes(buffer, 7+tl, socket);
    
    printf("> %s: %s\n", destName, msg);
}

void roomTalk(char *message, char*room, char *name, int socket){
    uint16_t mesLen = strlen(message) -1;
    uint8_t roomNamLen = strlen(room);

    uint32_t tl = 1+roomNamLen+2+mesLen;
    uint32_t tl_n = htonl(tl);
    uint8_t buffer[7+tl];
    memcpy(buffer, &tl_n, 4);

    uint32_t sendFlag = 0x041715;
    uint8_t t0[4], t1[3];
    memcpy(t0, &sendFlag, 4);
    t1[0] = t0[2];
    t1[1] = t0[1];
    t1[2] = t0[0];
    memcpy(buffer + 4, t1, 3);

    uint16_t meslentemp = htons(mesLen);
    memcpy(buffer+7, &roomNamLen , 1);
    memcpy(buffer+8, room, roomNamLen);
    memcpy(buffer+8+roomNamLen, &meslentemp, 2);
    memcpy(buffer+10+roomNamLen, message, mesLen);
    
    send_n_bytes(buffer, 7+tl, socket);

    message[mesLen] = 0;
    
    printf("[%s] %s: %s\n", room, name, message);
}

int sendFlag = 0;

void alarmHandler(){
    sendFlag = 1;
    signal(SIGALRM, alarmHandler);
    alarm(6);
}

int main(){
    int connected = 0, socket = -1, inARoom = 0;
    char room[256] = {0}, name[256] = {0};
    struct pollfd sockets[2] = {0};
    sockets[0].fd = STDIN_FILENO;
	sockets[0].events = POLLIN;
    
    signal(SIGALRM, alarmHandler);
	alarmHandler();
    sendFlag = 0;

    while(1){
       
        char command[MAX_COMMAND_LEN] = {0};

        if (poll(sockets, 2, 0) > 0 ){
           
            if(sockets[0].revents && POLLIN){
                //standard input
                fgets(command, MAX_COMMAND_LEN, stdin);
                if(strstr(command, "\\quit") == command){
                    printf("Quitting...\n");
                    if(connected){
                        close(socket);
                    }
                    break;
                }

                if(connected){
                    if(strstr(command, "\\disconnect") == command){
                        alarm(0);
                        sendFlag = 0;
                        connected = 0;
                        inARoom = 0;
                        socket = -1;
                        sockets[1].fd = -1;
                        memset(room, 0, 256);
                        memset(name, 0, 256);
                        close(socket);
                        printf("Disconnected from the chat server.\n");
                    }else if(strstr(command, "\\nick") == command){
                        sendFlag = 0;
                        signal(SIGALRM, alarmHandler);
                        alarm(6);
                        sendNick(command, name, socket);
                    }else if(strstr(command, "\\join") == command){
                        sendFlag = 0;
                        signal(SIGALRM, alarmHandler);
                        alarm(6);
                        sendJoin(command, room, &inARoom, socket);
                    }else if(strstr(command, "\\leave") == command){
                        sendFlag = 0;
                        signal(SIGALRM, alarmHandler);
                        alarm(6);
                        sendLeave(room, &connected, &inARoom, socket);
                    }else if(strstr(command, "\\list users") == command){
                        sendFlag = 0;
                        signal(SIGALRM, alarmHandler);
                        alarm(6);
                        sendListUsers(room, &inARoom, socket);
                    }else if(strstr(command, "\\list rooms") == command){
                        sendFlag = 0;
                        signal(SIGALRM, alarmHandler);
                        alarm(6);
                        sendListRooms(socket);
                    }else if(strstr(command, "\\connect") == command){
                        char *comma = strchr(command, ':');
                        if(comma && comma-command > 9 && *(comma+1) != ' ' && *(comma+1) != '\n'){
                            printf("Already connected to a chat server.\n");
                        }else{
                            printf("Invalid command.\n");
                        }
                    }else if(strstr(command, "\\msg") == command){
                        sendFlag = 0;
                        signal(SIGALRM, alarmHandler);
                        alarm(6);
                        sendMessage(command, socket);
                    }else if(strstr(command, "\\") == command){
                        printf("Invalid command.\n");
                    }else{
                        sendFlag = 0;
                        signal(SIGALRM, alarmHandler);
                        alarm(6);
                        if(inARoom){
                            roomTalk(command, room, name, socket);
                        }else{
                            printf("Command failed. (You shout into the void and hear nothing.)\n");
                        }
                    }
                }else{
                    if(strstr(command, "\\connect") == command){
                        //连不上时有invalid ip和invalid argument两种情况
                        startConnection(command, name, &connected, &socket);
                        sendFlag = 0;
                        
                        signal(SIGALRM, alarmHandler);
                        alarm(6);
                        sockets[1].fd = socket;
                        sockets[1].events = POLLIN;
                    }else{
                        printf("Not connected to a chat server.\n");
                }
            }
        }

        if(connected && sockets[1].revents && POLLIN){
            uint32_t totalLen;
            read_n_bytes(&totalLen, 4, sockets[1].fd);
            totalLen = ntohl(totalLen);

            uint8_t flag[3], t1[3];
            read_n_bytes(flag, 3, sockets[1].fd);
            t1[0] = flag[2];
            t1[1] = flag[1];
            t1[2] = flag[0];
            uint32_t flagInt = 0;
            memcpy(&flagInt, t1, 3);

            if(flagInt == 0x041715){
                //room talk
                uint8_t roomNameLen, senderNameLen;
                read_n_bytes(&roomNameLen, 1, sockets[1].fd);
                char roomName[roomNameLen+1];
                memset(roomName, 0, roomNameLen+1);
                read_n_bytes(roomName, roomNameLen, sockets[1].fd);
                read_n_bytes(&senderNameLen, 1, sockets[1].fd);
                char senderName[senderNameLen+1];
                memset(senderName, 0, senderNameLen+1);
                read_n_bytes(senderName, senderNameLen, sockets[1].fd);

                uint16_t mesLen;
                read_n_bytes(&mesLen, 2, sockets[1].fd);
                mesLen = ntohs(mesLen);
                char message[mesLen+1];
                memset(message, 0, mesLen+1);
                read_n_bytes(message, mesLen, sockets[1].fd);

                printf("[%s] %s: %s\n", roomName, senderName, message);
            }else if(flagInt ==0x041712){
                uint8_t senderNameLen;
                read_n_bytes(&senderNameLen, 1, sockets[1].fd);
                char senderName[senderNameLen+1];
                memset(senderName, 0, senderNameLen+1);
                read_n_bytes(senderName, senderNameLen, sockets[1].fd);

                uint16_t mesLen;
                read_n_bytes(&mesLen, 2, sockets[1].fd);
                mesLen = ntohs(mesLen);
                char message[mesLen+1];
                memset(message, 0, mesLen+1);
                read_n_bytes(message, mesLen, sockets[1].fd);

                printf("< %s: %s\n", senderName, message);
            }else if(flagInt ==0x04179a){
                uint8_t trash[totalLen];
                read_n_bytes(trash, totalLen, sockets[1].fd);
            }
        }
    }
       
    if(sendFlag && connected){
        sendFlag = 0;

        uint8_t buffer[39];
        uint32_t tl = 32;
        uint32_t tl_temp = htonl(tl);
        memcpy(buffer, &tl_temp, 4);
        
        uint32_t sendFlag = 0x041713;
        uint8_t t0[4], t1[3];
        memcpy(t0, &sendFlag, 4);
        t1[0] = t0[2];
        t1[1] = t0[1];
        t1[2] = t0[0];
        memcpy(buffer + 4, t1, 3);

        uint8_t wtf = 31;
        memcpy(buffer + 7, &wtf, 1);

        char string[] = "staying alive, staying alive...";
        memcpy(buffer+8, string, 31);
        send_n_bytes(buffer, 39, socket);
    }
        
    }
    if(socket >= 0){
        sockets[1].fd = -1;
        close(socket);
    }
}