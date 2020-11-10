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

#include <poll.h>
#include <signal.h>
#include "hash.h"
#include "chord.pb-c.h"
#include <math.h>


void add (uint8_t *input,  uint8_t *res, int power) {
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

int main(){
    uint8_t res[20] = {0};
    uint8_t input[20] = {0};
    
    input[18] = 1;
    add(input, res, 5);
    printf("%d\n", res[19]);
}