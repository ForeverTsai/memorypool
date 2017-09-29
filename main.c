/*
 * Memory pool test.
 *
 * Author: ForeverCai <gdzhforever@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>

#include "mempool.h"

#define KSIZE(n) (n<<10)
#define MSIZE(n) (n<<20)

struct private_data {
	//uint32_t a;
	//uint32_t b;
	//uint32_t c;
	char buf[1024];
};


#define TEST_NUM	20
#define TEST_COUNT	TEST_NUM
#define TEST_RANK	((uint32_t)rand())
void smempool_test(void)
{
	smempool_t *mempool;
	char *obj[TEST_NUM];
	int i;

	srand(time(NULL));
	memset(obj, 0, sizeof(obj));
	/* mempool init */
	mempool = smempool_create(NULL, KSIZE(16), sizeof(struct private_data), 32);

	/* mempool alloc & free */
	for (i = 0; i < TEST_NUM; i++) {
		obj[i] = smempool_alloc(mempool);
	}
	for (i = 0; i < TEST_NUM; i++) {
		smempool_free(mempool, obj[TEST_RANK%TEST_NUM]);
	}

	/* mempool destroy */
	smempool_destroy(mempool);
}

#define MALLOC_COUNT	20
#define GETCHAR		0

void mmempool_test()
{
	mmempool_t *mempool;
#if GETCHAR
	char c;
#endif
	char *p[MALLOC_COUNT];
	int i,j,k;


	/* mempool init */
	mempool = mmempool_create(NULL, MSIZE(1), 0, 10);
	/* mempool alloc & free */
#if 0

	mmempool_alloc(mempool, 300<<10);
	mmempool_dump(mempool);
	mmempool_alloc(mempool, 127<<10);
	mmempool_dump(mempool);
	exit(0);
#endif
	srand(time(NULL));
	memset(p, 0, sizeof(p));
	while (1) {
malloc_process:
		i = rand()%MALLOC_COUNT;
		if (p[i] != NULL)
			goto free_process;
		printf("**********malloc**********\n");
		p[i] = mmempool_alloc(mempool, rand()%(300<<10));
		if (p[i] == NULL)
			printf("**********malloc failed**********\n");
		printf("[[malloc ptr=%p]]\n", p[i]);
		for (k=0;k<MALLOC_COUNT;k++) {
			printf("p%-2d= %-16p", k, p[k]);
			if ((k+1)%5 == 0)
				printf("\n");
		}
		printf("\n");
		mmempool_dump(mempool);
#if GETCHAR
		c = getchar();
		if (c == 'q')
			break;
		else if (c == 'f')
			goto free_all;
#endif
free_process:
		j = rand()%MALLOC_COUNT;
		if (p[j] == NULL)
			goto malloc_process;
		printf("**********free**********\n");
		for (k=0;k<MALLOC_COUNT;k++) {
			printf("p%-2d=%-16p ", k, p[k]);
			if ((k+1)%5 == 0)
				printf("\n");
		}
		printf("\n");
		printf("[[free ptr=%p]]\n", p[j]);
		mmempool_free(mempool, p[j]);
		p[j] = NULL;
		mmempool_dump(mempool);
#if GETCHAR
		c = getchar();
		if (c == 'q')
			break;
		else if (c == 'f')
			goto free_all;
#endif
		printf("**********remain size:%uKB**********\n", mmempool_remain_size(mempool)>>10);

	}
#if GETCHAR
free_all:
#endif
	for(j=0;j<MALLOC_COUNT;j++) {
		mmempool_free(mempool, p[j]);
	}
	mmempool_dump(mempool);
	/* mempool destroy */
	mmempool_destroy(mempool);

}

void display_usage(void)
{
	printf( "\n"
		"Usage: mempool [OPTIONS]\n"
		"mempool demo, contain smempool, mmempool.\n"
		"Options:\n"
		"-s --smem      Single Memory Pool Demo.\n"
		"-m --mmem      Multiple Memory Pool Demo.\n"
		"-t --thread    Multiple thread test.\n\n"
		);
	exit(0);
}

void display_version(void)
{
	printf( "MemPool version: %s \n"
		"        date: %s \n", MEMPOOL_VERSION, MEMPOOL_DATE);
	exit(0);
}

int main(int argc, char *argv[])
{
	int option_index = 0,c;
	int smem = 0, mmem = 0;
	const char *short_options = "smtd:vh";
	const struct option long_options[] = {
		{"smem", no_argument, 0, 's'},
		{"mmem", no_argument, 0, 'm'},
		{"thread", no_argument, 0, 't'},
		{"debug", required_argument, 0, 'd'},
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
		{NULL, 0, 0, 0},
	};

	if (argc == 1)
		display_usage();

	for (;;) {
		c = getopt_long(argc, argv, short_options, long_options, &option_index);
		if (c == EOF) {
			break;
		}
		switch(c) {
			case 's':
				smem = 1;
				break;
			case 'm':
				mmem = 1;
				break;
			case 't':
				break;
			case 'd':
				mempool_set_debug_level(atoi(optarg));
				break;
			case 'v':
				display_version();
				break;
			case 'h':
			case '?':
				display_usage();
				break;
		}
	}
	if (optind == 1)
		display_usage();
	if (smem)
		smempool_test();
	if (mmem)
		mmempool_test();

	return 0;
}
