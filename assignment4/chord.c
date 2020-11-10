#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <argp.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include "hash.h"
#include "chord.pb-c.h"

int ts = 0, tff = 0, tcp = 0, TS, TFF, TCP;

void alarmHandler(){
	if(ts > 0){
		ts --;
	}else{
		ts = TS;
	}

	if(tff > 0){
		tff --;
	}else{
		tff = TFF;
	}

	if(tcp > 0){
		tcp --;
	}else{
		tcp = TCP;
	}

	signal(SIGALRM, alarmHandler);
	alarm(1);
}

struct chord_arguments {
	char myAddr[16], joinAddr[16];
	int myPort, joinPort, timeStabilize, timeFixFingers, timeCheckPrede, r, id;
};

struct Node{
	uint8_t ID[20];
	char ipAddr[16];
	int port;
};

error_t chord_parser(int key, char *arg, struct argp_state *state) {
	struct chord_arguments *args = state->input;
	error_t ret = 0;
	switch(key) {
	case 'a':
		strncpy(args->myAddr, arg, 16);
		break;
	case 'p':
		args->myPort = atoi(arg);
		break;
	case 100:
		strncpy(args->joinAddr, arg, 16);
		break;
	case 200:
		args->joinPort = atoi(arg);
		break;
	case 300:
		args->timeStabilize = atoi(arg);
		break;
	case 400:
		args->timeFixFingers = atoi(arg);
		break;
	case 500:
		args->timeCheckPrede = atoi(arg);
		break;
	case 'r':
		args->r = atoi(arg);
		break;
	case 'i':
		args->id = atoi(arg);
		break;
	default:
		ret = ARGP_ERR_UNKNOWN;
		break;
	}
	return ret;
}

void sha1Hash(char *target, uint8_t *result){
	struct sha1sum_ctx *ctx = sha1sum_create(NULL, 0);

	//sha1sum_update(ctx, (const uint8_t*)lookup1,  strlen(lookup1));
	sha1sum_finish(ctx, (const uint8_t*)target, strlen(target),result);
	sha1sum_destroy(ctx);
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
            fprintf(stderr, "send failed\n");
            break;
        }
        bytes_sent += temp;
    }
}

void servSockSetUp(int *servSock, char *addr, int port){
	if ((*servSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
		fprintf(stderr, "socket creation failed\n");
	}

	// Construct local address structure
	struct sockaddr_in servAddr; // Local address
	memset(&servAddr, 0, sizeof(servAddr)); // Zero out structure
	servAddr.sin_family = AF_INET; // IPv4 address family
	inet_pton(AF_INET, addr, &servAddr.sin_addr.s_addr);
	servAddr.sin_port = htons(port); // Local port
	
	// Bind the socket to the socketAddr
	if (bind(*servSock, (struct sockaddr*) &servAddr, sizeof(servAddr)) < 0){
		fprintf(stderr, "Binding socket failed\n");
	}

	//listen to at most five clients
	if (listen(*servSock, 160) < 0){
		fprintf(stderr, "listening failed\n");
	}
}

void printHash(uint8_t *hash){
	for(size_t i = 0; i < 20; ++i) {
			printf("%02x", hash[i]);
	}
}

void initializeNode(struct Node *node, char *ipaddr, int port){
	strcpy(node->ipAddr, ipaddr);
	node->port = port;
	char tempString[25] = {0};
	strcat(tempString, ipaddr);
	strcat(tempString, ":");
	char tempPort[10] = {0};
	sprintf(tempPort, "%d", port);
	strcat(tempString, tempPort);
	sha1Hash(tempString, node->ID);
}

void createRing(struct Node *myNode, int r, struct Node *susList, struct Node *fingTable){
	for(int i = 0; i < r; i++){
		susList[i] = *myNode;
	}

	for(int i = 1; i < 161; i++){
		fingTable[i] = *myNode;
	}
}

int createSendSocket(char *ipAddr, int port){
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

	struct timeval timeout;
	timeout.tv_sec  = 0;  
	timeout.tv_usec = 5000 * 1000; //timeout in 0.5 seconds
	setsockopt(clientSock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    if (connect(clientSock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0){
        fprintf(stderr, "socket connection timeout\n");
        return -1;
    }
    return clientSock;
}

//Returns 1 if h1 bigger, -1 if h2 is bigger, 0 if tie
int compareHashes(uint8_t *h1, uint8_t *h2){
	for(int i = 0;i<20;i++){
		if(h1[i] > h2[i]){
			return 1;
		}else if(h1[i] < h2[i]){
			return -1;
		}
	}
	return 0;
}

int between(uint8_t *btw, uint8_t *h1, uint8_t *h2){
	if(compareHashes(h2, h1) == 1){
		if(compareHashes(btw, h1) == 1 && compareHashes(h2, btw) == 1){
			return 1;
		}else{
			return 0;
		}
	}else{
		if(compareHashes(btw, h1) == 1 || compareHashes(h2, btw) == 1){
			return 1;
		}else{
			return 0;
		}
	}
}

struct Node *closetPrecedingNode(uint8_t *ID, struct Node *fingerTable, struct Node *self){
	for(int i = 160; i>=1; i--){
		struct Node *nI = &fingerTable[i];
		struct Node empty = {0};
		if(between(nI->ID, self->ID, ID) && memcmp(&empty, nI, sizeof(struct Node)) !=0 ){
			return nI;
		}
	}
	return self;
}

void printNode(struct Node *node){
	printHash(node->ID);
	printf(" %s %d\n", node->ipAddr, node->port);
}

int sendFindSuccessorRPC(struct Node *nodeToAsk, uint8_t *ID){
	int sendSocket = createSendSocket(nodeToAsk->ipAddr, nodeToAsk->port);

	if(sendSocket == -1){
		return -1;
	}

	Protocol__FindSuccessorArgs findSucArgs = PROTOCOL__FIND_SUCCESSOR_ARGS__INIT;
	findSucArgs.id.data = ID;
	findSucArgs.id.len = 20;

	size_t argsLen = protocol__find_successor_args__get_packed_size(&findSucArgs);
	uint8_t argsSerial[argsLen];
	memset(argsSerial, 0, argsLen);
	protocol__find_successor_args__pack(&findSucArgs, argsSerial);
	
	Protocol__Call call = PROTOCOL__CALL__INIT;
	call.name = "find_successor";
	call.args.len = argsLen;
	call.args.data = argsSerial;

	size_t callSerialLen = protocol__call__get_packed_size(&call);
	uint8_t callSerial[callSerialLen];
	memset(callSerial, 0, callSerialLen);

	protocol__call__pack(&call, callSerial);

	uint8_t buffer[8+callSerialLen];
	memset(buffer, 0, 8+callSerialLen);
	uint64_t lenInNB = htobe64(8+callSerialLen);
	memcpy(buffer, &lenInNB, 8);
	memcpy(buffer+8, callSerial, callSerialLen);
	send_n_bytes(buffer, 8+callSerialLen, sendSocket);
	return sendSocket;
}

int handleFindSuccessorRPC(uint8_t **findSuccessorRetSerial, size_t *findSuccessorRetSerialLen, uint8_t *feedData, size_t feedDataLen, struct Node *self,
							struct Node *successorList, struct Node *fingerTable){
	Protocol__FindSuccessorArgs *args = protocol__find_successor_args__unpack(NULL, feedDataLen, feedData);

	if(args == NULL) {
    	fprintf(stderr, "findSuccessor Unpack Error\n");
		abort();
  	}

	uint8_t *targetID = args->id.data;
	
	struct Node successor = successorList[0];
	Protocol__FindSuccessorRet retSucs = PROTOCOL__FIND_SUCCESSOR_RET__INIT;
	Protocol__Node node = PROTOCOL__NODE__INIT;

	Protocol__Return *ret = NULL;
	Protocol__FindSuccessorRet *findSucRet = NULL;
	int flag = 0;
	if(between(targetID, self->ID, successor.ID) == 1 || memcmp(self->ID, successor.ID, 20) == 0){
		//The node is the biggest node in the ring now){
		//First case, just returns the successor
		node.address = successor.ipAddr;
		node.port = successor.port;
		node.id.data = successor.ID;
		node.id.len = 20;
		retSucs.node = &node;
		
	}else{
		struct Node *cPN = closetPrecedingNode(targetID, fingerTable, self);
		if(cPN == self){
			//If cPN returns self
			node.address = self->ipAddr;
			node.port = self->port;
			node.id.data = self->ID;
			node.id.len = 20;
			retSucs.node = &node;
		}else{
			//If cPN requires asking some other nodes
			int sendSocket = sendFindSuccessorRPC(cPN, targetID);

			uint64_t tL;
			read_n_bytes(&tL, 8, sendSocket);
			tL = be64toh(tL) - 8;
			uint8_t buffer[tL];
			read_n_bytes(buffer, tL, sendSocket);
			close(sendSocket);

			Protocol__Return *ret = protocol__return__unpack(NULL, tL, buffer);
			Protocol__FindSuccessorRet *findSucRet = protocol__find_successor_ret__unpack(NULL, ret->value.len, ret->value.data);

			if(ret->success){
				node.address = findSucRet->node->address;
				node.port = findSucRet->node->port;
				node.id.data = findSucRet->node->id.data;
				node.id.len = 20;
				retSucs.node = &node;
				flag = 1;
			}else{
				fprintf(stderr, "findSuccessor using the closet preceding node failed\n");
				return 0;
			}
		}
	}
	
	*findSuccessorRetSerialLen = protocol__find_successor_ret__get_packed_size(&retSucs);
	*findSuccessorRetSerial = (uint8_t *)malloc(*findSuccessorRetSerialLen);
	protocol__find_successor_ret__pack(&retSucs, *findSuccessorRetSerial);
	protocol__find_successor_args__free_unpacked(args, NULL);
	if(flag){
		protocol__find_successor_ret__free_unpacked(findSucRet, NULL);
		protocol__return__free_unpacked(ret, NULL);
	}
	return 1;
}

int handleGetSuccessorListRPC(uint8_t **getSuccessorListRetSerial, size_t *getSuccessorListRetSerialLen, uint8_t *feedData, size_t feedDataLen, 
							struct Node *successorList,  int r){
	Protocol__GetSuccessorListArgs *args = protocol__get_successor_list_args__unpack(NULL, feedDataLen, feedData);
	
	if(args == NULL) {
    	fprintf(stderr, "getSuccessorList Unpack Error\n");
		abort();
  	}

	Protocol__GetSuccessorListRet retListSucs = PROTOCOL__GET_SUCCESSOR_LIST_RET__INIT;
	retListSucs.n_successors = r;

	retListSucs.successors = malloc(r * sizeof(Protocol__Node *));

	Protocol__Node tempNodes[r];
	for(int i = 0; i < r; i++){
		Protocol__Node  n = PROTOCOL__NODE__INIT;
		n.port = successorList[i].port;
		n.id.data = successorList[i].ID;
		n.id.len = 20;
		n.address = successorList[i].ipAddr;
		tempNodes[i] = n;
	}

	for(int i = 0;i <r;i++){
		retListSucs.successors[i] = &tempNodes[i];
	}

	*getSuccessorListRetSerialLen = protocol__get_successor_list_ret__get_packed_size(&retListSucs);
	*getSuccessorListRetSerial = (uint8_t *)malloc(*getSuccessorListRetSerialLen);
	protocol__get_successor_list_ret__pack(&retListSucs, *getSuccessorListRetSerial);
	protocol__get_successor_list_args__free_unpacked(args, NULL);
	free(retListSucs.successors);
	return 1;
}

int handleGetPredecessorRPC(uint8_t **getPredecessorRetSerial, size_t *getPredecessorRetSerialLen, uint8_t *feedData, size_t feedDataLen,
							struct Node *predecessor){
	Protocol__GetPredecessorArgs *args = protocol__get_predecessor_args__unpack(NULL, feedDataLen, feedData);

	if(args == NULL) {
    	fprintf(stderr, "getPredecessor Unpack Error\n");
		abort();
  	}

	Protocol__GetPredecessorRet retPrede = PROTOCOL__GET_PREDECESSOR_RET__INIT;
	Protocol__Node node = PROTOCOL__NODE__INIT;
	struct Node temp = {0};
	int flag;

	if(memcmp(predecessor, &temp, sizeof(struct Node)) != 0){
		node.address = predecessor->ipAddr;
		node.port = predecessor->port;
		node.id.len = 20;
		node.id.data = predecessor->ID;
		flag = 1;
	}else{
		
		flag = 0;
	}
	
	retPrede.node = &node;
	*getPredecessorRetSerialLen = protocol__get_predecessor_ret__get_packed_size(&retPrede);
	*getPredecessorRetSerial = (uint8_t *)malloc(*getPredecessorRetSerialLen);
	protocol__get_predecessor_ret__pack(&retPrede, *getPredecessorRetSerial);
	protocol__get_predecessor_args__free_unpacked(args, NULL);

	return flag;
}


int handleNotifyRPC(uint8_t **notifyRetSerial, size_t *notifyRetSerialLen, uint8_t *feedData, size_t feedDataLen,
					struct Node *prede, struct Node *self){
	Protocol__NotifyArgs *args = protocol__notify_args__unpack(NULL, feedDataLen, feedData);

	if(args == NULL) {
    	fprintf(stderr, "notify Unpack Error\n");
		abort();
  	}
	struct Node temp = {0};
	if(memcmp(&temp, prede, 20) == 0 || between(args->node->id.data, prede->ID, self->ID)){
		strcpy(prede->ipAddr, args->node->address);
		memcpy(prede->ID, args->node->id.data, 20);
		prede->port = args->node->port;
	}

	Protocol__NotifyRet retNotify = PROTOCOL__NOTIFY_RET__INIT;
	
	*notifyRetSerialLen = protocol__notify_ret__get_packed_size(&retNotify);
	*notifyRetSerial = (uint8_t *)malloc(*notifyRetSerialLen);
	protocol__notify_ret__pack(&retNotify, *notifyRetSerial);
	protocol__notify_args__free_unpacked(args, NULL);
	
	//has value should be 0
	return 0;
}

int handleCheckPredecessorRPC(uint8_t **checkPredeRetSerial, size_t *checkPredeRetSerialLen, uint8_t *feedData, size_t feedDataLen){
	Protocol__CheckPredecessorArgs *args = protocol__check_predecessor_args__unpack(NULL, feedDataLen, feedData);

	if(args == NULL) {
    	fprintf(stderr, "check predecessor Unpack Error\n");
		abort();
  	}

	Protocol__CheckPredecessorRet retCheckPrede = PROTOCOL__CHECK_PREDECESSOR_RET__INIT;

	*checkPredeRetSerialLen = protocol__check_predecessor_ret__get_packed_size(&retCheckPrede);
	*checkPredeRetSerial = (uint8_t *)malloc(*checkPredeRetSerialLen);
	protocol__check_predecessor_ret__pack(&retCheckPrede, *checkPredeRetSerial);
	protocol__check_predecessor_args__free_unpacked(args, NULL);

	//has value should be 0
	return 0;
}

void sendReturnRPC(uint8_t *retV, size_t retLen, int hasvalue, int clntSocket){
	Protocol__Return ret = PROTOCOL__RETURN__INIT;	

	ret.success = 1;
	if (hasvalue) {
		ret.value.data = retV;
		ret.value.len = retLen;
		ret.has_value = hasvalue;
	}else{
		ret.has_value = hasvalue;
	}

	size_t retSerialLen = protocol__return__get_packed_size(&ret);
	uint8_t retSerial[retSerialLen];
	memset(retSerial, 0, retSerialLen);
	
	protocol__return__pack(&ret, retSerial);

	uint8_t buffer[8+retSerialLen];
	memset(buffer, 0, 8+retSerialLen);

	uint64_t tL = 8+retSerialLen;
	tL = htobe64(tL);
	memcpy(buffer, &tL, 8);
	memcpy(buffer+8, retSerial, retSerialLen);
	send_n_bytes(buffer, 8+retSerialLen, clntSocket);
}

int sendGetSuccessorListRPC(struct Node *nodeToAsk){
	int sendSocket = createSendSocket(nodeToAsk->ipAddr, nodeToAsk->port);

	Protocol__GetSuccessorListArgs getSucListArgs = PROTOCOL__GET_SUCCESSOR_LIST_ARGS__INIT;
	size_t argsLen = protocol__get_successor_list_args__get_packed_size(&getSucListArgs);
	uint8_t argsSerial[argsLen];
	memset(argsSerial, 0, argsLen);
	protocol__get_successor_list_args__pack(&getSucListArgs, argsSerial);
	
	Protocol__Call call = PROTOCOL__CALL__INIT;
	call.name = "get_successor_list";
	call.args.len = argsLen;
	call.args.data = argsSerial;

	size_t callSerialLen = protocol__call__get_packed_size(&call);
	uint8_t callSerial[callSerialLen];
	memset(callSerial, 0, callSerialLen);

	protocol__call__pack(&call, callSerial);

	uint8_t buffer[8+callSerialLen];
	memset(buffer, 0, 8+callSerialLen);
	uint64_t lenInNB = htobe64(8+callSerialLen);
	memcpy(buffer, &lenInNB, 8);
	memcpy(buffer+8, callSerial, callSerialLen);
	send_n_bytes(buffer, 8+callSerialLen, sendSocket);

	return sendSocket;
}	

int sendGetPredecessorRPC(struct Node *nodeToAsk){
	int sendSocket = createSendSocket(nodeToAsk->ipAddr, nodeToAsk->port);

	if(sendSocket == -1){
		return sendSocket;
	}

	Protocol__GetPredecessorArgs getPredeArgs = PROTOCOL__GET_PREDECESSOR_ARGS__INIT;
	size_t argsLen = protocol__get_predecessor_args__get_packed_size(&getPredeArgs);
	uint8_t argsSerial[argsLen];
	memset(argsSerial, 0, argsLen);
	protocol__get_predecessor_args__pack(&getPredeArgs, argsSerial);
	
	Protocol__Call call = PROTOCOL__CALL__INIT;
	call.name = "get_predecessor";
	call.args.len = argsLen;
	call.args.data = argsSerial;

	size_t callSerialLen = protocol__call__get_packed_size(&call);
	uint8_t callSerial[callSerialLen];
	memset(callSerial, 0, callSerialLen);

	protocol__call__pack(&call, callSerial);

	uint8_t buffer[8+callSerialLen];
	memset(buffer, 0, 8+callSerialLen);
	uint64_t lenInNB = htobe64(8+callSerialLen);
	memcpy(buffer, &lenInNB, 8);
	memcpy(buffer+8, callSerial, callSerialLen);
	send_n_bytes(buffer, 8+callSerialLen, sendSocket);

	return sendSocket;
}

int sendNotifyRPC(struct Node *nodeToAsk, struct Node *from){
	int sendSocket = createSendSocket(nodeToAsk->ipAddr, nodeToAsk->port);

	if(sendSocket == -1){
		return sendSocket;
	}

	Protocol__NotifyArgs notifyArgs = PROTOCOL__NOTIFY_ARGS__INIT;
	Protocol__Node node = PROTOCOL__NODE__INIT;
	node.id.data = from->ID;
	node.id.len = 20;
	node.address = from->ipAddr;
	node.port = from->port;
	notifyArgs.node = &node;

	size_t argsLen = protocol__notify_args__get_packed_size(&notifyArgs);
	uint8_t argsSerial[argsLen];
	memset(argsSerial, 0, argsLen);
	protocol__notify_args__pack(&notifyArgs, argsSerial);
	
	Protocol__Call call = PROTOCOL__CALL__INIT;
	call.name = "notify";
	call.args.len = argsLen;
	call.args.data = argsSerial;

	size_t callSerialLen = protocol__call__get_packed_size(&call);
	uint8_t callSerial[callSerialLen];
	memset(callSerial, 0, callSerialLen);

	protocol__call__pack(&call, callSerial);

	uint8_t buffer[8+callSerialLen];
	memset(buffer, 0, 8+callSerialLen);
	uint64_t lenInNB = htobe64(8+callSerialLen);
	memcpy(buffer, &lenInNB, 8);
	memcpy(buffer+8, callSerial, callSerialLen);
	send_n_bytes(buffer, 8+callSerialLen, sendSocket);

	return sendSocket;
}

int sendCheckPredecessorRPC(struct Node *nodeToAsk){
	int sendSocket = createSendSocket(nodeToAsk->ipAddr, nodeToAsk->port);

	if(sendSocket == -1){
		return sendSocket;
	}

	Protocol__CheckPredecessorArgs checkPredeArgs = PROTOCOL__CHECK_PREDECESSOR_ARGS__INIT;

	size_t argsLen = protocol__check_predecessor_args__get_packed_size(&checkPredeArgs);
	uint8_t argsSerial[argsLen];
	memset(argsSerial, 0, argsLen);
	protocol__check_predecessor_args__pack(&checkPredeArgs, argsSerial);
	
	Protocol__Call call = PROTOCOL__CALL__INIT;
	call.name = "check_predecessor";
	call.args.len = argsLen;
	call.args.data = argsSerial;

	size_t callSerialLen = protocol__call__get_packed_size(&call);
	uint8_t callSerial[callSerialLen];
	memset(callSerial, 0, callSerialLen);

	protocol__call__pack(&call, callSerial);

	uint8_t buffer[8+callSerialLen];
	memset(buffer, 0, 8+callSerialLen);
	uint64_t lenInNB = htobe64(8+callSerialLen);
	memcpy(buffer, &lenInNB, 8);
	memcpy(buffer+8, callSerial, callSerialLen);
	send_n_bytes(buffer, 8+callSerialLen, sendSocket);

	return sendSocket;
}

void stabilize(struct Node *successorList, int r, struct Node *self, struct Node *successor, struct Node *prede){
	for(int i = 0; i < r; i++){
		if(memcmp(self, &successorList[i], sizeof(struct Node)) == 0){
			struct Node tN = {0};
			if(memcmp(prede, &tN, sizeof(struct Node)) != 0){
				if(between(prede->ID, self->ID, successorList[i].ID )){
					for(int t = r-1; t >= 1; t--){
						successorList[t] = successorList[t-1];
					}
					successorList[0] = *prede;
					*successor = *prede;
				}
			}
			return;
		}

		int socket = sendGetPredecessorRPC(&successorList[i]);
		
		if(socket != -1){
			uint64_t tL;
			read_n_bytes(&tL, 8, socket);
			tL = be64toh(tL) - 8;
			uint8_t buffer[tL];
			read_n_bytes(buffer, tL, socket);

			Protocol__Return *ret = protocol__return__unpack(NULL, tL, buffer);
			Protocol__GetPredecessorRet *getPredeRet = protocol__get_predecessor_ret__unpack(NULL, ret->value.len, ret->value.data);
			//Free unpack later
			close(socket);

			socket = sendGetSuccessorListRPC(&successorList[i]);
			
			if(socket != -1){
				uint64_t tL2;
				read_n_bytes(&tL2, 8, socket);
				tL2 = be64toh(tL2) - 8;
				uint8_t buffer2[tL2];
				read_n_bytes(buffer2, tL2, socket);
				
				Protocol__Return *ret2 = protocol__return__unpack(NULL, tL2, buffer2);
				Protocol__GetSuccessorListRet *getSucListRet = protocol__get_successor_list_ret__unpack(NULL, ret2->value.len, ret2->value.data);
				
				successorList[0] = successorList[i];
				*successor = successorList[i];
				for(int j = 1; j < r; j++){
					memcpy(successorList[j].ID, getSucListRet->successors[j-1]->id.data, 20);
					strcpy(successorList[j].ipAddr, getSucListRet->successors[j-1]->address);
					successorList[j].port = getSucListRet->successors[j-1]->port;
				}
				
				protocol__get_successor_list_ret__free_unpacked(getSucListRet, NULL);
				protocol__return__free_unpacked(ret2, NULL);
				close(socket);
				
				//If the successor's predecessor is not null and falls into the 
				if(ret->has_value && between(getPredeRet->node->id.data, self->ID, successorList[i].ID ) ){
					struct Node successorsPrede = {0};
					initializeNode(&successorsPrede, getPredeRet->node->address, getPredeRet->node->port);
					socket = sendGetSuccessorListRPC(&successorsPrede);

					if(socket != -1){
						uint64_t tL3;
						read_n_bytes(&tL3, 8, socket);
						tL3 = be64toh(tL3) - 8;
						uint8_t buffer3[tL3];
						read_n_bytes(buffer3, tL3, socket);

						Protocol__Return *ret3 = protocol__return__unpack(NULL, tL3, buffer3);
						Protocol__GetSuccessorListRet *getSucListRet3 = protocol__get_successor_list_ret__unpack(NULL, ret3->value.len, ret3->value.data);

						successorList[0] = successorsPrede;
						*successor = successorsPrede;
						for(int t = 1; t < r; t++){
							memcpy(successorList[t].ID, getSucListRet3->successors[t-1]->id.data, 20);
							strcpy(successorList[t].ipAddr, getSucListRet3->successors[t-1]->address);
							successorList[t].port = getSucListRet3->successors[t-1]->port;
						}
						
						protocol__get_successor_list_ret__free_unpacked(getSucListRet3, NULL);
						protocol__return__free_unpacked(ret3, NULL);
						close(socket);
					}
				}
				//Notify the successor 
				*successor = successorList[0];
				socket = sendNotifyRPC(successor, self);
				uint64_t tL4;
				read_n_bytes(&tL4, 8, socket);
				tL4 = be64toh(tL4) - 8;
				uint8_t buffer4[tL4];
				read_n_bytes(buffer4, tL4, socket);
				Protocol__Return *ret4 = protocol__return__unpack(NULL, tL4, buffer4);
				Protocol__NotifyRet *notifyRet = protocol__notify_ret__unpack(NULL, ret4->value.len, ret4->value.data);

				protocol__notify_ret__free_unpacked(notifyRet, NULL);
				protocol__return__free_unpacked(ret4, NULL);
				close(socket);

				break;
			}
			protocol__get_predecessor_ret__free_unpacked(getPredeRet, NULL);
			protocol__return__free_unpacked(ret, NULL);
		}
	}
	//All nodes lower than i have dead,
}

void checkPredecessor(struct Node *prede){
	int sendSocket = sendCheckPredecessorRPC(prede);

	if(sendSocket == -1){
		struct Node temp = {0};
		*prede = temp;
	}else{
		uint64_t tL;
		read_n_bytes(&tL, 8, sendSocket);
		tL = be64toh(tL) - 8;
		uint8_t buffer[tL];
		read_n_bytes(buffer, tL, sendSocket);

		Protocol__Return *ret = protocol__return__unpack(NULL, tL, buffer);
		Protocol__CheckPredecessorRet *checkPredeRet = protocol__check_predecessor_ret__unpack(NULL, ret->value.len, ret->value.data);
		
		protocol__check_predecessor_ret__free_unpacked(checkPredeRet, NULL);
		protocol__return__free_unpacked(ret, NULL);
		close(sendSocket);
	}
}

void addHashToPowerOfTwo (uint8_t *input,  uint8_t *res, int power) {
	int	carry = 0, base = pow(2, 8) - 1, sum;

    uint8_t B[20] = {0};
    int quotient = power/8, mod = power % 8;
    B[20 - quotient -1] = pow(2, mod);

	for (int i=19; i>=0; i--) {
		sum = input[i] + B[i] + carry;
		if (sum >= base) {
			carry = 1;
			sum -= base;
		} else
			carry = 0;
		res[i] = sum;
	}
}

void fixFingers(struct Node *fingerTable, struct Node *successorList, struct Node *self, int *next){
	*next = *next +1;

	if(*next > 160)
		*next = 1;

	uint8_t hashAdded[20] = {0};
	addHashToPowerOfTwo(self->ID, hashAdded, *next-1);
	
	Protocol__FindSuccessorArgs findSucArgs = PROTOCOL__FIND_SUCCESSOR_ARGS__INIT;
	findSucArgs.id.data = hashAdded;
	findSucArgs.id.len = 20;
	size_t argsLen = protocol__find_successor_args__get_packed_size(&findSucArgs);
	uint8_t argsSerial[argsLen];
	memset(argsSerial, 0, argsLen);
	protocol__find_successor_args__pack(&findSucArgs, argsSerial);

	uint8_t *findSuccessorRetSerial = NULL;
	size_t findSuccessorRetSerialLen;

	
	int success = handleFindSuccessorRPC(&findSuccessorRetSerial, &findSuccessorRetSerialLen, argsSerial, 
						argsLen, self, successorList, fingerTable);
	
	if(!success){
		fprintf(stderr, "fix fingers failed\n");
	}else{
		Protocol__FindSuccessorRet *findSucRet = protocol__find_successor_ret__unpack(NULL, 
		findSuccessorRetSerialLen, findSuccessorRetSerial);
		
		struct Node newSucNode = {0}; 
		memcpy(newSucNode.ID, findSucRet->node->id.data,20);
		newSucNode.port = findSucRet->node->port;
		strcpy(newSucNode.ipAddr, findSucRet->node->address);

		fingerTable[*next] = newSucNode;
		int i;

		for(i = *next + 1; i <= 160 ; i++){
			uint8_t hashAddedTemp[20] = {0};
			addHashToPowerOfTwo(self->ID, hashAddedTemp, i-1);
			
			if(between(hashAddedTemp, self->ID, newSucNode.ID) == 1){
				fingerTable[i] = newSucNode;
			}else{
				break;
			}
		}
		
		*next = i-1;
	}
	free(findSuccessorRetSerial);
}

float timedifferenceMsec(struct timeval t0, struct timeval t1)
{
    return (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f;
}

int main(int argc, char *argv[]){
    struct argp_option options[] = {
		{ "addr", 'a', "myAddr", 0, "The IP address that the Chord client will bind to, as well as advertise to other nodes.", 0},
		{ "port", 'p', "myPort", 0, "The port that the Chord client will bind to and listen on. Represented as a base-10 integer.", 0},
		{ "ja", 100, "joinAddr", 0, "The IP address of the machine running a Chord node. The Chord client will join this node’s ring.", 0},
		{ "jp", 200, "joinPort", 0, "The port that an existing Chord node is bound to and listening on. The Chord client will join this node’s ring", 0},
        { "ts", 300, "timeStabilize", 0, "The time in milliseconds between invocations of ‘stabilize’.",0},
		{ "tff", 400, "timeFixFingers", 0, "The time in milliseconds between invocations of ‘fix fingers’.",0},
		{ "tcp",500,"timeCheckPrede",0,"The time in milliseconds between invocations of ‘check predecessor’.",0},
		{ "r",'r',"r",0,"The number of successors maintained by the Chord client.",0},
		{ "i",'i',"id",0,"The identifier (ID) assigned to the Chord client which will override the ID computed by the SHA1 sum of the client’s IP address and port number.",0},
		{0}
	};
	struct argp argp_settings = { options, chord_parser, 0, 0, 0, 0, 0 };

	struct chord_arguments args;
	
	bzero(&args, sizeof(args));

	if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0) {
		fprintf(stderr, "Got error in parse\n");
	}

	struct Node self = {0};
	struct Node successor;
	struct Node prede = {0};
	struct Node successorList[args.r];
	struct Node fingerTable[161];
	memset(successorList, 0, args.r * sizeof(struct Node));
	memset(successorList, 0, 161 * sizeof(struct Node));

	TCP = args.timeCheckPrede / 1000;
	TFF = args.timeFixFingers / 1000;
	TS = args.timeStabilize / 1000;
	
	//KeyBoard Poll
	struct pollfd readSockets[2] = {0};
    readSockets[0].fd = STDIN_FILENO;
	readSockets[0].events = POLLIN;

	//Server Poll
	int servSock;
	servSockSetUp(&servSock, args.myAddr, args.myPort);
	readSockets[1].fd = servSock;
	readSockets[1].events = POLLIN;

	//Same setup for both create and join
	memset(&prede, 0, sizeof(struct Node));
	initializeNode(&self, args.myAddr, args.myPort);

	char joinPortStr[10] = {0};
	sprintf(joinPortStr, "%d", args.joinPort);
	if(!strcmp("", args.joinAddr) && !strcmp("0", joinPortStr)){
        //Create ring
		successor = self;
		createRing(&self, args.r, successorList, fingerTable);
    }else if(strcmp("", args.joinAddr) && strcmp("0", joinPortStr)){
		//Join existing node
		struct Node nodeJoin = {0};
		initializeNode(&nodeJoin, args.joinAddr, args.joinPort);
		memset(fingerTable, 0, sizeof(struct Node)*161);
		int sendSocket = sendFindSuccessorRPC(&nodeJoin, self.ID);
		uint64_t tL;
		read_n_bytes(&tL, 8, sendSocket);
		tL = be64toh(tL) - 8;
		uint8_t buffer[tL];
		read_n_bytes(buffer, tL, sendSocket);

		Protocol__Return *ret = protocol__return__unpack(NULL, tL, buffer);
		Protocol__FindSuccessorRet *findSucRet = protocol__find_successor_ret__unpack(NULL, ret->value.len, ret->value.data);

		strcpy(successor.ipAddr, findSucRet->node->address);
		memcpy(successor.ID, findSucRet->node->id.data, 20);
		successor.port = findSucRet->node->port;
		successorList[0] = successor;
		
		protocol__find_successor_ret__free_unpacked(findSucRet, NULL);
		protocol__return__free_unpacked(ret, NULL);
		close(sendSocket);

		//So far, get successor has ended
		sendSocket = sendGetSuccessorListRPC(&nodeJoin);
		uint64_t tL2;
		read_n_bytes(&tL2, 8, sendSocket);
		tL2 = be64toh(tL2) - 8;
		uint8_t buffer2[tL2];
		read_n_bytes(buffer2, tL2, sendSocket);

		Protocol__Return *ret2 = protocol__return__unpack(NULL, tL2, buffer2);
		Protocol__GetSuccessorListRet *getSucListRet = protocol__get_successor_list_ret__unpack(NULL, ret2->value.len, ret2->value.data);
		
		for(int i = 1; i < args.r; i++){
			memcpy(successorList[i].ID, getSucListRet->successors[i-1]->id.data, 20);
			strcpy(successorList[i].ipAddr, getSucListRet->successors[i-1]->address);
			successorList[i].port = getSucListRet->successors[i-1]->port;
		}

		protocol__get_successor_list_ret__free_unpacked(getSucListRet, NULL);
		protocol__return__free_unpacked(ret2, NULL);
		close(sendSocket);
	}else{
		fprintf(stderr, "ja & jp must be both specified or unspecfied\n");
        abort();
	}
	
	// signal(SIGALRM, alarmHandler);
	// alarm(1);

	printf("> ");
	fflush(stdout);

	struct timeval tsPrev, tffPrev, tcpPrev;
	gettimeofday(&tsPrev, NULL);
	gettimeofday(&tffPrev, NULL);
	gettimeofday(&tcpPrev, NULL);
	int next = 0;

	while(1){
		char command[100] = {0};
        
        if (poll(readSockets, 2, 0) > 0 ){
			if(readSockets[0].revents && POLLIN){    
				//keyboard Input   
				fgets(command, 100, stdin);
                if(strstr(command, "Lookup") == command){
                    char lookupWord[strlen(command+7)];
					memset(lookupWord, 0, strlen(command+7));
					memcpy(lookupWord, command+7, strlen(command+7)-1);
					uint8_t hash[20] = {0};
					sha1Hash(lookupWord, hash);
					printf("< %s ", lookupWord);
					printHash(hash);
					putchar('\n');
				
					Protocol__FindSuccessorArgs findSucArgs = PROTOCOL__FIND_SUCCESSOR_ARGS__INIT;
					findSucArgs.id.data = hash;
					findSucArgs.id.len = 20;
					size_t argsLen = protocol__find_successor_args__get_packed_size(&findSucArgs);
					uint8_t argsSerial[argsLen];
					memset(argsSerial, 0, argsLen);
					protocol__find_successor_args__pack(&findSucArgs, argsSerial);

					uint8_t *findSuccessorRetSerial = NULL;
					size_t findSuccessorRetSerialLen;
					int success = handleFindSuccessorRPC(&findSuccessorRetSerial, &findSuccessorRetSerialLen, argsSerial, 
										argsLen, &self, successorList, fingerTable);

					if(!success){
						fprintf(stderr, "handleFindSuccessorRPC failed\n");
					}else{
						Protocol__FindSuccessorRet *findSucRet = protocol__find_successor_ret__unpack(NULL, 
						findSuccessorRetSerialLen, findSuccessorRetSerial);
						printf("< ");
						printHash(findSucRet->node->id.data);
						printf(" %s %d\n",  findSucRet->node->address, findSucRet->node->port);
					}
					
                }else if(strstr(command, "PrintState") == command){
					printf("< Self ");
					printNode(&self);
					for(int i = 0; i < args.r; i++){
						printf("< Successor[%d] ", i+1);
						printNode(&successorList[i]);
					}
					for(int i = 1; i < 161; i++){
						printf("< Finger[%d] ", i);
						printNode(&fingerTable[i]);
					}
				}
				printf("> ");
				fflush(stdout);
			}else if(readSockets[1].revents && POLLIN){
				struct sockaddr_in clntAddr; 
        		socklen_t clntAddrLen = sizeof(clntAddr);
       			int clntSock = accept(servSock, (struct sockaddr *) &clntAddr, &clntAddrLen);

				if(clntSock < 0){
            		fprintf(stderr, "accepting socket failed\n");
        		}else{
            		char clntName[INET_ADDRSTRLEN]; // String to contain client address
            		if (inet_ntop(AF_INET, &clntAddr.sin_addr.s_addr, clntName, sizeof(clntName)) != NULL){
                		//fprintf(stderr, "Handling client %s/%d\n", clntName, ntohs(clntAddr.sin_port));
					}else{
						fprintf(stderr, "Unable to get client address\n");
					}
					uint64_t length;
					read_n_bytes(&length, 8, clntSock);

					length = be64toh(length) - 8;
					uint8_t buffer[length];
					memset(buffer, 0, length);
					read_n_bytes(buffer, length, clntSock);

					Protocol__Call *call = protocol__call__unpack(NULL, length, buffer);
					if (call == NULL) {
						fprintf(stderr, "Call Unpack failed\n");
						abort();
					}
					//printf("Handling request: %s\n", call->name);
					//Handing different types of nodes' queries
					if (strcmp(call->name, "find_successor") == 0) {
						uint8_t *findSuccessorRetSerial = NULL;
						size_t findSuccessorRetSerialLen;
						int hasvalue = handleFindSuccessorRPC(&findSuccessorRetSerial, &findSuccessorRetSerialLen, call->args.data, 
										call->args.len, &self, successorList, fingerTable);
						
						sendReturnRPC(findSuccessorRetSerial, findSuccessorRetSerialLen, hasvalue, clntSock);
						free(findSuccessorRetSerial);
						close(clntSock);
					} else if(strcmp(call->name, "get_successor_list") == 0) {
						uint8_t *getSuccessorListRetSerial = NULL;
						size_t getSuccessorListRetSerialLen;

						int hasvalue = handleGetSuccessorListRPC(&getSuccessorListRetSerial, &getSuccessorListRetSerialLen, call->args.data, 
										call->args.len, successorList, args.r);
						sendReturnRPC(getSuccessorListRetSerial, getSuccessorListRetSerialLen, hasvalue, clntSock);
						
						free(getSuccessorListRetSerial);
						close(clntSock);
					} else if(strcmp(call->name, "get_predecessor") == 0){
						uint8_t *getPredecessorRetSerial = NULL;
						size_t getPredecessorRetSerialLen;

						int hasvalue = handleGetPredecessorRPC(&getPredecessorRetSerial, &getPredecessorRetSerialLen, call->args.data,
										call->args.len, &prede);
						sendReturnRPC(getPredecessorRetSerial, getPredecessorRetSerialLen, hasvalue, clntSock);
						free(getPredecessorRetSerial);
						close(clntSock);
					} else if(strcmp(call->name, "notify") == 0){
						uint8_t *notifyRetSerial = NULL;
						size_t notifyRetSerialLen;

						int hasvalue = handleNotifyRPC(&notifyRetSerial, &notifyRetSerialLen, call->args.data,
										call->args.len, &prede, &self);
						sendReturnRPC(notifyRetSerial,notifyRetSerialLen, hasvalue, clntSock);
						free(notifyRetSerial);
						close(clntSock);
					} else if(strcmp(call->name, "check_predecessor") == 0){
						uint8_t *checkPredeRetSerial = NULL;
						size_t checkPredeRetSerialLen;

						int hasvalue = handleCheckPredecessorRPC(&checkPredeRetSerial, &checkPredeRetSerialLen, call->args.data,
										call->args.len);
						sendReturnRPC(checkPredeRetSerial,checkPredeRetSerialLen, hasvalue, clntSock);
						free(checkPredeRetSerial);
						close(clntSock);
					} 
					protocol__call__free_unpacked(call, NULL);
				}
			}
		}

		struct timeval timeNow;
		gettimeofday(&timeNow, NULL);

		if(timedifferenceMsec(tsPrev, timeNow) > args.timeStabilize){
			stabilize(successorList, args.r, &self, &successor, &prede);
			
			gettimeofday(&tsPrev, NULL);
		}
		else if(timedifferenceMsec(tffPrev, timeNow) > args.timeFixFingers){
			fixFingers(fingerTable, successorList, &self, &next);
			gettimeofday(&tffPrev, NULL);
		}else if(timedifferenceMsec(tcpPrev, timeNow) > args.timeCheckPrede){
			checkPredecessor(&prede);
			gettimeofday(&tcpPrev, NULL);
		}
	}
}