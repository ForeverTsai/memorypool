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
#include "mempool.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

static int debug = 0;

#define PRINT_COLOR

#ifdef PRINT_COLOR
#define PRINT_COLOR_END		"\033[m"
#define PRINT_COLOR_GRAY	"\033[1;30m"
#define PRINT_COLOR_RED		"\033[1;31;1;4m"
#define PRINT_COLOR_GREEN	"\033[1;32m"
#define PRINT_COLOR_YELLOW	"\033[1;33m"
#define PRINT_COLOR_BLUE	"\033[1;34m"
#define PRINT_COLOR_PURPLE	"\033[1;35m"
#define PRINT_COLOR_CYAN	"\033[1;36m"
#define PRINT_COLOR_WHITE	"\033[1;37m"
#else
#define PRINT_COLOR_END
#define PRINT_COLOR_GRAY
#define PRINT_COLOR_RED
#define PRINT_COLOR_GREEN
#define PRINT_COLOR_YELLOW
#define PRINT_COLOR_BLUE
#define PRINT_COLOR_PURPLE
#define PRINT_COLOR_CYAN
#define PRINT_COLOR_WHITE
#endif

#ifdef DEBUG
#define Debug(fmt, args...) \
		printf("[%s:%s] line=%d---"fmt"", __FILE__, __func__, __LINE__, ##args)

#define dbg(debug_level, fmt, args...) \
	if (debug > debug_level)  { \
		switch(debug_level) { \
			case MEMPOOL_PRINT_LEVEL_EMERG: \
				printf(PRINT_COLOR_RED"[%s:%s] line=%d---"fmt""PRINT_COLOR_END, \
					__FILE__, __func__, __LINE__, ##args); \
				break; \
			case MEMPOOL_PRINT_LEVEL_VERBOSE: \
				printf(PRINT_COLOR_GRAY"[%s:%s] line=%d---"fmt""PRINT_COLOR_END, \
					__FILE__, __func__, __LINE__, ##args); \
				break; \
			default: \
				printf("[%s:%s] line=%d---"fmt"", __FILE__, __func__, __LINE__, ##args); \
				break; \
		} \
	}

#define pr_debug(fmt,args...)	dbg(MEMPOOL_PRINT_LEVEL_DEBUG, fmt, ##args)
#define pr_info(fmt,args...)	dbg(MEMPOOL_PRINT_LEVEL_INFO, fmt, ##args)
#define pr_wrn(fmt,args...)	dbg(MEMPOOL_PRINT_LEVEL_WARNING, fmt, ##args)
#define pr_ver(fmt,args...)	dbg(MEMPOOL_PRINT_LEVEL_VERBOSE, fmt, ##args)
#define pr_emerg(fmt,args...)	dbg(MEMPOOL_PRINT_LEVEL_EMERG, fmt, ##args)


#define dump_struct(handle, e, f)  \
	Debug(#handle"->%-12s = "#f"\n", #e, handle->e)

#define dump_mempool	dump_struct

#else
#define Debug(fmt, args...)
#define dbg(fmt, args...)

#define pr_debug(fmt,args...)
#define pr_info(fmt,args...)
#define pr_wrn(fmt,args...)
#define pr_ver(fmt,args...)
#define pr_emerg(fmt,args...)
#endif

#define ALIGN_SIZE	16
#define ALIGN_MASK	(~(ALIGN_SIZE-1))
#define ALIGN(size, align)	(((size)+align-1)&(~(align-1)))

void mempool_set_debug_level(int level)
{
	debug = level;
}

static inline smem_bufctl_t *smem_bufctl(smempool_t *smem)
{
	return (smem_bufctl_t *)(smem+1);
}

static inline void *index_to_obj(smempool_t *mempool, uint32_t idx)
{
	return mempool->smem + mempool->ele_asize * idx;
}

static inline uint32_t reciprocal_divide(uint32_t A, uint32_t R)
{
	return (uint32_t)(((uint64_t)A * R) >> 32);
}

static inline uint32_t obj_to_index(smempool_t *mempool, void *objp)
{
	uint32_t offset = (objp - mempool->smem);
#if 0
	return reciprocal_divide(offset, mempool->ele_asize);
#else
	return (offset/mempool->ele_asize);
#endif
}

/*
 * 指定大小为size的内存池
 *
 *	+--------------------------------------+
 *      |         |            | |    |        |
 *      | smempool| smem_bufctl| |ele0|......  |
 *      |         |            | |    |        |
 *	+--------------------------------------+
 *
 */

smempool_t *smempool_create(void *mem_ptr, uint32_t mem_size, uint32_t element_size, uint32_t align)
{
	smempool_t *mempool;
	int i;

	if (!mem_size)
		return NULL;
	if (!mem_ptr)
		mem_ptr  = (uint8_t *)malloc(mem_size);

	mempool = (smempool_t *)mem_ptr;
	sem_init(&mempool->sem, 0, 0);
	mempool->mem_size = mem_size;
	mempool->align = (!align) ? ALIGN_SIZE : align;
	mempool->ele_ssize = element_size;
	mempool->ele_asize = ALIGN((element_size), mempool->align);
	mempool->ele_num = (mempool->mem_size-sizeof(smempool_t))/(sizeof(smem_bufctl_t)+mempool->ele_asize);
	mempool->smem = mem_ptr+mempool->mem_size-(mempool->ele_num*mempool->ele_asize);
	mempool->free = 0;
	mempool->inuse = 0;
	for (i=0;i<mempool->ele_num;i++)
		smem_bufctl(mempool)[i]=i+1;

#ifdef DEBUG
#if 1
	dump_mempool(mempool, smem, "%p");
	dump_mempool(mempool, mem_size, "%u");
	dump_mempool(mempool, align, "%u");
	dump_mempool(mempool, ele_ssize, "%u");
	dump_mempool(mempool, ele_asize, "%u");
	dump_mempool(mempool, ele_num, "%u");
#endif
#endif
	sem_post(&mempool->sem);

	return mempool;
}

void smempool_destroy(smempool_t *mempool)
{
	if (!mempool)
		return ;
	sem_destroy(&mempool->sem);
	free(mempool);
}

void *smempool_alloc(smempool_t *mempool)
{
	void *objp;
	smem_bufctl_t next;

	if (!mempool || mempool->inuse == mempool->ele_num)
		return NULL;
	sem_wait(&mempool->sem);
	objp = index_to_obj(mempool, mempool->free);
	mempool->inuse++;
	next = smem_bufctl(mempool)[mempool->free];
	smem_bufctl(mempool)[mempool->free] = 0;
	mempool->free = next;

	sem_post(&mempool->sem);
	pr_debug("inuse=%u,free=%u,objp=%p\n", mempool->inuse, mempool->free, objp);

	return objp;
}

void smempool_free(smempool_t *mempool, void *objp)
{
	uint32_t objnr;

	if (!objp)
		return;

	sem_wait(&mempool->sem);
	objnr = obj_to_index(mempool, objp);
	if (smem_bufctl(mempool)[objnr] != 0) {
		sem_post(&mempool->sem);
		return ;
	}
	smem_bufctl(mempool)[objnr] = mempool->free;
	mempool->free = objnr;
	mempool->inuse--;

	sem_post(&mempool->sem);
	pr_debug("inuse=%u,free=%u,objp=%p,objnr=%u,bufctl=%u\n",
		mempool->inuse, mempool->free, objp, objnr, smem_bufctl(mempool)[objnr]);
	return ;
}


/*
 * 指定包含大小为size(必须为2的n次方,建议4K以上),size*2^1,size*2^2...size*2^(m-1)
 * 连续m种大小的内存池
 *
 */

#define order2bytes(n) (1<<(n))

#define OVERHEAD (2*sizeof(size_t))
#define CHUNK_SIZE(c)	((c)->csize & -4)
#define CHUNK_PSIZE(c)	((c)->psize & -4)
#define PREV_CHUNK(c)	((struct chunk *)((char *)(c) - CHUNK_PSIZE(c)))
#define NEXT_CHUNK(c)	((struct chunk *)((char *)(c) + CHUNK_SIZE(c)))
#define CHUNK_TO_MEM(c) (void *)((char *)(c) + OVERHEAD)
#define MEM_TO_CHUNK(p) (struct chunk *)((char *)(p) - OVERHEAD)

#define C_INUSE		((size_t)1)
#define C_LAST		((size_t)2)

mmempool_t *mmempool_create(void *mem_ptr, uint32_t mem_size, uint32_t order_min, uint32_t order_max)
{
	int free_area_num,i,j;
	size_t last_size=0;
	struct chunk *c = NULL;
	mmempool_t *mempool;
	char *mmem = NULL;

	if (mem_size == 0 || order_min > order_max)
		return NULL;


	mempool = (mmempool_t *)malloc(sizeof(mmempool_t));

	sem_init(&mempool->sem, 0, 0);

	if (!mem_ptr) {
		mempool->mmem = malloc(mem_size);
/*
		pr_emerg("LAST->psize=0x%x, LAST->csize=0x%x\n",
			(uint32_t)((struct chunk *)(mempool->mmem+mem_size))->psize,
			(uint32_t)((struct chunk *)(mempool->mmem+mem_size))->csize);
*/
		mempool->external_mem = 0;
	} else {
		mempool->mmem = mem_ptr;
		mempool->external_mem = 1;
	}
	pr_debug("!!!!!!mmem = %p\n", mempool->mmem);
	mempool->mem_size = mem_size;
	mempool->order_max = order_max;
	mempool->order_min = order_min;

	order_max += 10;
	order_min += 10;

	/* free_area init */
	free_area_num = order_max-order_min + 1;
	mempool->free_area = (struct free_area *)malloc(free_area_num * sizeof(struct free_area));
	pr_debug("!!!!!!free_area= %p\n", mempool->free_area);

	mmem = mempool->mmem;
	for (i = 1; i < free_area_num + 1; i++) {
		struct list_head *head = &mempool->free_area[free_area_num-i].free_list;
		if (i==1)
			mempool->free_area[free_area_num-i].nr_free = mem_size>>(order_max+1-i);
		else
			mempool->free_area[free_area_num-i].nr_free = (mem_size>>(order_max+1-i))&0x01;
		INIT_LIST_HEAD(head);
		pr_debug("order=%u, nr_free=%u\n", order_max+1-i, mempool->free_area[free_area_num-i].nr_free);
		for (j = 0; j < mempool->free_area[free_area_num-i].nr_free; j++) {
			c = (struct chunk *)mmem;
			pr_debug("!!!!!!chunk=%p\n", c);
			c->psize = last_size;
			c->csize = order2bytes(order_max+1-i);
			last_size = c->csize;
			mmem += c->csize;
			list_add_tail(&c->list, head);
			pr_debug("psize=%u, csize=%u\n", (uint32_t)c->psize, (uint32_t)c->csize);
		}
	}
	if (c != NULL)
		c->csize |= C_LAST;
	sem_post(&mempool->sem);
	return mempool;
}

void mmempool_destroy(mmempool_t *mempool)
{
	if (!mempool)
		return;
	if (!mempool->external_mem)
		free(mempool->mmem);
	sem_destroy(&mempool->sem);
	free(mempool);
}

uint32_t mmempool_remain_size(mmempool_t *mempool)
{
	uint32_t size=0,i=0;

	if (!mempool)
		return 0;
	sem_wait(&mempool->sem);
	for (i=mempool->order_min;i<=mempool->order_max;i++) {
		size += mempool->free_area[i].nr_free << (10+i);
	}
	sem_post(&mempool->sem);

	return size;
}

static inline int32_t byte2kborder(uint32_t bytes)
{
	int32_t len,order;

	len = sizeof(bytes)*8;
	for (order=len-1;order>=0;order--){
		if (bytes&(1<<order))
			break;
	}
	//pr_debug("bytes=%u,0x%x,order=%u\n", bytes, bytes, order);
	if (bytes&((1<<order)-1))
		order++;
	if (order == 0)
		order = -1;
	else if (order <= 10)
		order = 0;
	else
		order -= 10;
	//pr_debug("actual order=%u\n", order);
	return order;
}

void mmempool_dump(mmempool_t *mempool)
{
	uint32_t i;
	uint32_t free_area_num = mempool->order_max-mempool->order_min + 1;
	uint32_t free_area_count[2][free_area_num];

	sem_wait(&mempool->sem);
	memset(free_area_count, 0, sizeof(free_area_count));
	pr_ver("===== mmempool dump =====\n");
	free_area_num = mempool->order_max-mempool->order_min + 1;
	for (i = 1; i < free_area_num + 1; i++) {
		struct list_head *head = &mempool->free_area[free_area_num-i].free_list;
		struct list_head *pos;
		struct chunk *tmp;

		free_area_count[0][free_area_num-i] = mempool->free_area[free_area_num-i].nr_free;
		pr_ver("area=%p, order=%u, kbsize=%uKB, nr_free=%u\n",
			&mempool->free_area[free_area_num-i],
			mempool->order_max+1-i,
			(1<<(mempool->order_max+1-i)),
			mempool->free_area[free_area_num-i].nr_free);
		list_for_each(pos, head) {
			tmp = list_entry(pos, struct chunk, list);
			pr_ver("chunk---psize=%uKB,csize=%uKB\n", (uint32_t)tmp->psize>>10, (uint32_t)tmp->csize>>10);
		}
	}
	struct chunk *c = (struct chunk *)mempool->mmem;
	pr_ver("+-----------+\n");
	uint32_t kbsize = 0;
	uint32_t order = 0;
	char *mem = NULL;
	uint32_t last_count = 0;
	while (1) {
		kbsize =(uint32_t)c->csize>>10;
		mem = CHUNK_TO_MEM(c);
		order = byte2kborder(CHUNK_SIZE(c));

		free_area_count[1][order-mempool->order_min]++;
		if (c->csize&C_LAST) {
			last_count++;
			if (CHUNK_SIZE(c)>>10 == 0) {
				fprintf(stderr, PRINT_COLOR_RED"Last chunk, but csize=%u\n"PRINT_COLOR_END, (uint32_t)CHUNK_SIZE(c));
				exit(-1);
			}
			if (c->csize&C_INUSE) {
				pr_ver("| %-4uKB-use| ----- LAST-%p\n", kbsize, mem);
				free_area_count[1][order-mempool->order_min]--;
			} else
				pr_ver("| %-4uKB    | ----- LAST\n", kbsize);
			break;
		} else {
			if (c->csize&C_INUSE && NEXT_CHUNK(c)->psize&C_INUSE) {
				pr_ver("| %-4uKB-use| ----- %p\n", kbsize, mem);
				free_area_count[1][order-mempool->order_min]--;
			} else if (c->csize&C_INUSE || NEXT_CHUNK(c)->psize&C_INUSE) {
				pr_ver("| %-4uKB-err| ----- %p\n", kbsize, mem);
				exit(-1);
			} else
				pr_ver("| %-4uKB    |\n", kbsize);
		}
		if (CHUNK_SIZE(c) != CHUNK_PSIZE(NEXT_CHUNK(c))) {
			fprintf(stderr, PRINT_COLOR_RED"c->csize=%uKB, next->psize=%uKB, maybe error...\n"PRINT_COLOR_END, (uint32_t)CHUNK_SIZE(c)>>10, (uint32_t)CHUNK_PSIZE(NEXT_CHUNK(c))>>10);
			exit(-1);
		}
		c = NEXT_CHUNK(c);
		pr_ver("+-----------+\n");
	}
	pr_ver("+-----------+\n");
	for(i=0;i<free_area_num;i++) {
		if (free_area_count[0][i] != free_area_count[1][i]) {
			fprintf(stderr, PRINT_COLOR_RED"ERROR! order=%u chunk nr_free different: %u!=%u \n"PRINT_COLOR_END,
				i+mempool->order_min, free_area_count[0][i], free_area_count[1][i]);
			exit(-1);
		}
	}
	if (last_count != 1) {
		fprintf(stderr, PRINT_COLOR_RED"ERROR! last_count = %u\n"PRINT_COLOR_END, last_count);
		exit(-1);
	}
/*
	pr_emerg("LAST->psize=0x%x, LAST->csize=0x%x\n",
		(uint32_t)((struct chunk *)(mempool->mmem+mempool->mem_size))->psize,
		(uint32_t)((struct chunk *)(mempool->mmem+mempool->mem_size))->csize);
*/
	pr_ver("=========================\n");
	sem_post(&mempool->sem);
}


static void expand(struct chunk *c, uint32_t low, uint32_t high, struct free_area *area)
{
	uint32_t kbsize = 1 << high;
	uint32_t last_chunk=0;
	pr_debug("low=%u, high=%u\n", low, high);

	if (c->csize&C_LAST)
		last_chunk = 1;
	c->csize |= C_INUSE;
	if (last_chunk)
		c->csize |= C_LAST;
	else
		NEXT_CHUNK(c)->psize |= C_INUSE;
	while (high > low) {
		struct chunk *newc;
		/* resize c */
		area--;
		high--;
		kbsize >>= 1;

		c->csize = kbsize<<10;
		c->csize |= C_INUSE;

		/* size newc */
		newc = NEXT_CHUNK(c);
		newc->psize = CHUNK_SIZE(c);
		newc->psize |= C_INUSE;
		newc->csize = kbsize<<10;
		if (last_chunk) {
			newc->csize |= C_LAST;
			last_chunk = 0;
		} else
			NEXT_CHUNK(newc)->psize = CHUNK_SIZE(newc);
		pr_debug("expand chunk---new chunk: psize=%uKB,csize=%uKB\n", (uint32_t)newc->psize>>10, (uint32_t)newc->csize>>10);
		list_add_tail(&newc->list, &area->free_list);
		area->nr_free++;
		pr_debug("expand chunk---new area: order=%u,nr_free=%u\n", high, area->nr_free);
	}
}

static inline uint32_t kbsize2order(mmempool_t *mempool, uint32_t kbsize)
{
	uint32_t kborder_min,kborder_max;
	uint32_t order;

	kborder_min = mempool->order_min;
	kborder_max = mempool->order_max;

	pr_debug("kbsize=%uKB ---- 0x%x\n", kbsize, kbsize);
	/* kbsize large than order_max kbsize */
	pr_debug("(1U<<(kborder_max+1)) = 0x%x\n", (1U<<(kborder_max+1)));
	if ((kbsize&(~((1U<<(kborder_max+1))-1))) != 0)
		return -1;
	/* calculate order */
	pr_info("order min=%u - 0x%x, max=%u - 0x%x\n", kborder_min, 1<<kborder_min, kborder_max, 1<<kborder_max);
	for (order = kborder_max; order > kborder_min-1; order--){
			pr_debug("kbsize=0x%x, order=%u\n", kbsize, order);
			if ((kbsize&(1<<order)) != 0) {
				if (kbsize&((1U<<order)-1))
					order++;
				break;
			}
	}
	return order;
}

static void *mmempool_alloc_with_kborder(mmempool_t *mempool, int32_t kborder)
{
	uint32_t order,cur_order,idx;
	struct free_area *area;
	struct chunk *c;

	if (kborder < 0)
		return NULL;
	order = kborder;

	if (order < mempool->order_min || order > mempool->order_max)
		return NULL;

	sem_wait(&mempool->sem);
	pr_info("calculate order=%u\n", order);
	pr_debug("find order:\n");
	for (cur_order = order; cur_order <= mempool->order_max; cur_order++) {
		idx = cur_order - mempool->order_min;
		area = &mempool->free_area[idx];
		pr_debug("prepare area=%p, list=%p, nr_free=%u\n", area, &area->free_list, area->nr_free);
		pr_debug("cur_order=%u, idx=%u,area->nr_free=%u\n", cur_order, idx, area->nr_free);
		if (list_empty(&area->free_list))
			continue;
		c = list_first_entry(&area->free_list, struct chunk, list);
		list_del(&c->list);
		area->nr_free--;
		expand(c, order, cur_order, area);

		sem_post(&mempool->sem);
		return CHUNK_TO_MEM(c);
	}

	sem_post(&mempool->sem);
	pr_info("malloc return NULL\n");
	return NULL;
}

void *mmempool_alloc(mmempool_t *mempool, uint32_t size)
{
	int32_t kborder;

	size += 16;
	kborder = byte2kborder(size);
	pr_info("size=%u, kborder=%d\n", size, kborder);
	return mmempool_alloc_with_kborder(mempool, kborder);
}



static struct chunk *split(mmempool_t *mempool, struct chunk *c)
{
	uint32_t i,j,idx;
	uint32_t size;
	struct chunk *new;

	pr_debug("max:%u - %uKB, min=%u - %uKB...csize=%uKB\n",
		mempool->order_max, 1<<mempool->order_max,
		mempool->order_min, 1<<mempool->order_min,
		(uint32_t)CHUNK_SIZE(c)>>10);
	for (i=mempool->order_max; i >= mempool->order_min; i--) {
		size = (1<<(i+10));
		if (CHUNK_SIZE(c) >= size) {
			pr_debug("size=%uKB, CHUNK_SIZE(c)/size=%u\n", size, (uint32_t)CHUNK_SIZE(c)/size);
			uint32_t chunk_num = CHUNK_SIZE(c)>>(i+10);
			for (j=0;j<chunk_num;j++) {
				pr_debug("CHUNK_SIZE(c)=%uKB, j=%u\n", (uint32_t)CHUNK_SIZE(c)>>10, j);
				if (CHUNK_SIZE(c) == size) {
					pr_debug("split last chunk, c=%p, size=%uKB\n", c, (uint32_t)CHUNK_SIZE(c)>>10);
					idx = i-mempool->order_min;
					list_add_tail(&c->list, &mempool->free_area[idx].free_list);
					mempool->free_area[idx].nr_free++;
					return c;
				}
				new = c;
				c = ((struct chunk *)((char *)(c) + size));
				c->csize = (CHUNK_SIZE(new)-size) | C_INUSE | (new->csize&C_LAST);
				/* init new (free) chunk */
				idx = i-mempool->order_min;
				new->csize = size;
				list_add_tail(&new->list, &mempool->free_area[idx].free_list);
				mempool->free_area[idx].nr_free++;
				/* init remain chunk */
				c->psize = CHUNK_SIZE(new);
			}
		}
	}
	pr_info("split finish, c=%p, size=%uKB\n", c, (uint32_t)CHUNK_SIZE(c)>>10);
	return c;
}

static struct chunk *combine_chunk(mmempool_t *mempool, struct chunk *cur, uint32_t cur_order)
{
	uint32_t order;
	size_t k;
	struct chunk *prev, *next;

	uint32_t idx = 0;
	/* back */
	while(1) {
		k = cur->psize;
		if (k == 0)
			break;
		if (!(k&C_INUSE)) {
			pr_info("back combine chunk!\n");
			prev = PREV_CHUNK(cur);
			pr_info("prev size = %uKB, prev=%p\n", (uint32_t)cur->psize>>10, prev);
			order = byte2kborder(CHUNK_SIZE(prev));
			if (order == mempool->order_max)
				break;
			list_del(&prev->list);
			idx = order-mempool->order_min;
			pr_info("prev size=%uKB, order=%u\n", (uint32_t)CHUNK_SIZE(prev)>>10, order);
			mempool->free_area[idx].nr_free--;
			prev->csize = (cur->csize + CHUNK_SIZE(prev)) | C_INUSE;
			cur = prev;
			pr_info("++++++add back size:%uKB\n", (uint32_t)(CHUNK_SIZE(cur)>>10));
		} else
			break;
	}
	if (!(cur->csize&C_LAST))
		NEXT_CHUNK(cur)->psize = cur->csize;
	else
		goto split_chunk;
	/* forward */
	next = NEXT_CHUNK(cur);
	while(1) {
		k = next->csize;
		if (!(k&C_INUSE)) {
			order = byte2kborder(CHUNK_SIZE(next));
			if (order == mempool->order_max)
				break;
			idx = order-mempool->order_min;
			list_del(&next->list);
			mempool->free_area[idx].nr_free--;
			cur->csize = (cur->csize + CHUNK_SIZE(next)) |C_INUSE;
			pr_info("++++++add forward size:%uKB\n", (uint32_t)(CHUNK_SIZE(cur)>>10));
			if (k&C_LAST) {
				cur->csize |= C_LAST;
				break;
			} else {
				next = NEXT_CHUNK(cur);
				next->psize = cur->csize;
			}
		} else
			break;
	}
split_chunk:
	/* split */
	pr_info("split!!!!!  chunk:%p, size=%uKB\n", cur, (uint32_t)(CHUNK_SIZE(cur)>>10));
	cur = split(mempool, cur);
	cur->csize &= ~C_INUSE;
	if (!(cur->csize&C_LAST)) {
		NEXT_CHUNK(cur)->psize = cur->csize;
	}

	return cur;
}

void mmempool_free(mmempool_t *mempool, void *objp)
{
	struct chunk *self;
	uint32_t order;

	pr_info("mempool=%p, objp=%p\n", mempool, objp);
	if (objp == NULL)
		return;
	self = MEM_TO_CHUNK(objp);
	if (!(self->csize&C_INUSE))
		return;
	sem_wait(&mempool->sem);

	order = byte2kborder(CHUNK_SIZE(self));
	pr_info("self=%p, csize=%uKB, psize=%uKB\n", self, (uint32_t)CHUNK_SIZE(self)>>10, (uint32_t)CHUNK_PSIZE(self)>>10);

	/* combine chunk */
	self = combine_chunk(mempool, self, order);
	sem_post(&mempool->sem);
}
