#include "hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int printHash(char *lookup2, struct sha1sum_ctx *ctx) {
	uint8_t checksum[20];

	//sha1sum_update(ctx, (const uint8_t*)lookup1,  strlen(lookup1));
	int error = sha1sum_finish(ctx, (const uint8_t*)lookup2, strlen(lookup2), checksum);
	if (!error) {
		//printf("%s ", lookup);
		for(size_t i = 0; i < 20; ++i) {
			printf("%02x", checksum[i]);
		}
		putchar('\n');
	}

	sha1sum_reset(ctx);

	return error;
}

int main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;

	struct sha1sum_ctx *ctx = sha1sum_create(NULL, 0);
	if (!ctx) {
		fprintf(stderr, "Error creating checksum\n");
		return 0;
	}

	printf("These should match the examples in the assignment spec:\n");

	printHash("127.0.0.1:34000",  ctx);
	printHash("127.0.0.1:34001",  ctx);
	printHash("127.0.0.1:34002",  ctx);
	//printHash("2003", ctx);

	sha1sum_destroy(ctx);

	return EXIT_SUCCESS;
}