/*
 * Memory pool.
 *
 * Author: ForeverCai <gdzhforever@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */
#ifndef __MEMPOOL_H_
#define __MEMPOOL_H_

#include <sys/types.h>
#include <stdint.h>
#include "list.h"
#include <semaphore.h>

#define MEMPOOL_VERSION		"0.0.1"
#define MEMPOOL_DATE		"2017-10-12"

typedef unsigned int smem_bufctl_t;

typedef struct smempool {
	void *smem;
	uint32_t mem_size;
	sem_t sem;
	uint32_t align;
	uint32_t ele_ssize;		/* element source size */
	uint32_t ele_asize;		/* element adjust size */
	uint32_t ele_num;
	smem_bufctl_t free;
	uint32_t inuse;
}smempool_t;

struct chunk {
	size_t psize, csize;
	struct list_head list;
};

struct free_area {
	struct list_head free_list;
	uint32_t nr_free;
};

typedef struct mmempool {
	void *mmem;
	uint32_t mem_size;
	uint32_t order_max;		/* kbytes max order */
	uint32_t order_min;		/* kbytes min order */
	struct free_area *free_area;
	uint32_t external_mem;
	sem_t sem;
}mmempool_t;

enum {
	MEMPOOL_PRINT_LEVEL_EMERG = -1,
	MEMPOOL_PRINT_LEVEL_VERBOSE = 0,
	MEMPOOL_PRINT_LEVEL_WARNING,
	MEMPOOL_PRINT_LEVEL_INFO,
	MEMPOOL_PRINT_LEVEL_DEBUG,
};


smempool_t *smempool_create(void *mem_ptr, uint32_t mem_size, uint32_t element_size, uint32_t align);
void smempool_destroy(smempool_t *mempool);
void *smempool_alloc(smempool_t *mempool);
void smempool_free(smempool_t *mempool, void *objp);

mmempool_t *mmempool_create(void *mem_ptr, uint32_t mem_size, uint32_t order_min, uint32_t order_max);
void mmempool_destroy(mmempool_t *mempool);
void *mmempool_alloc(mmempool_t *mempool, uint32_t kbsize);
void mmempool_free(mmempool_t *mempool, void *objp);
uint32_t mmempool_remain_size(mmempool_t *mempool);

void mmempool_dump(mmempool_t *mempool);

void mempool_set_debug_level(int level);

#endif
