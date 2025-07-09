#include "debug.h"
#include "cache.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>


static uint32_t get_set_index(Cache_State *c, uint32_t addr) {
    return addr >> c->log2_block_size & (c->num_sets - 1);
    }


static uint32_t get_tag(Cache_State *c, uint32_t addr) {
   return addr >> (c->log2_block_size + c->log2_num_sets);
}

static int log2_int(unsigned int v) {
    unsigned int r = 0; // r will be lg(v)

    while (v >>= 1) // unroll for more speed...
    {
        r++;
    }

    return r;
}

static void cache_lru_lru_replacement(Cache_State *c, Cache_Block *set, uint32_t tag) {
    // Assume the first block is the least recently used
    int lru_index = 0;
    int min_access = set[0].last_access;

    // Iterate through all ways to find the LRU block
    for (int i = 1; i < c->num_ways; i++) {
        Cache_Block *block = set + i;
        if (block->last_access < min_access) {
            min_access = block->last_access;
            lru_index = i;
        }
    }

    // Replace the LRU block with new tag and update metadata
    Cache_Block *replace_block = set + lru_index;
    replace_block->tag = tag;
    replace_block->valid = true;
    replace_block->last_access = c->timestamp;

    // Update insert counter for replacement policies that track insertion order
    c->insert_counter++;
    replace_block->insert_counter = c->insert_counter;
}

static void cache_fifo_replacement(Cache_State *c, Cache_Block *set, uint32_t tag) {
    // Assume the first block is the oldest (FIFO)
    int fifo_index = 0;
    int min_insert_counter = set[0].insert_counter;

    // Find the block with the smallest insert_counter (oldest)
    for (int i = 1; i < c->num_ways; i++) {
        if (set[i].insert_counter < min_insert_counter) {
            min_insert_counter = set[i].insert_counter;
            fifo_index = i;
        }
    }

    // Replace the FIFO block with new tag and update metadata
    Cache_Block *replace_block = set + fifo_index;
    replace_block->tag = tag;
    replace_block->valid = true;
    replace_block->last_access = c->timestamp;

    // Update insert counter for tracking future FIFO order
    c->insert_counter++;
    replace_block->insert_counter = c->insert_counter;
}


void cache_init(Cache_State *c, int total_size, int block_size, int num_ways,
                Cache_Policy policy, bool debug) {
    if (total_size == 0) {
    // cache is disabled, distribute a single block
    c->blocks = (Cache_Block *)calloc(1, sizeof(Cache_Block));
    return;
}

    c->policy = policy;
    c->total_size = total_size;
    c->block_size = block_size;
    c->num_ways = num_ways;
    c->num_sets = total_size / (block_size * num_ways);

    c->log2_block_size = log2_int(c->block_size);
    c->log2_num_sets = log2_int(c->num_sets);

    c->blocks = (Cache_Block *)calloc(c->num_sets * c->num_ways, sizeof(Cache_Block));

    c->insert_counter = 0;
    c->timestamp = 0;
    c->debug = debug;
}

enum Cache_Result cache_access(Cache_State *c, uint32_t addr) {
    if (c->total_size == 0) {
        // cache disabled: HIT on second access, then evict
        Cache_Block *block = c->blocks;

        if (block->valid && (block->tag == addr)) {
            block->valid = false;
            return CACHE_HIT;
        } else {
            block->tag = addr;
            block->valid = true;
            return CACHE_MISS;
        }
    }

    c->timestamp++;

    uint32_t set_index = get_set_index(c, addr);
    uint32_t tag = get_tag(c, addr);

    Cache_Block *set = c->blocks + set_index * c->num_ways;
    Cache_Block *cache_block;

    // 1. Check for HIT
    for (int i = 0; i < c->num_ways; i++) {
        cache_block = set + i;
        if (cache_block->valid && cache_block->tag == tag) {
            cache_block->last_access = c->timestamp;
            return CACHE_HIT;
        }
    }

    // 2. Check for invalid block (cold miss)
    for (int i = 0; i < c->num_ways; i++) {
        cache_block = set + i;
        if (!cache_block->valid) {
            cache_block->valid = true;
            cache_block->tag = tag;
            cache_block->last_access = c->timestamp;
            c->insert_counter++;
            cache_block->insert_counter = c->insert_counter;
            return CACHE_MISS;
        }
    }

    // 3. All blocks are valid → apply replacement policy
    switch (c->policy) {
        case Cache_LRU_LRU:
            cache_lru_lru_replacement(c, set, addr);
            break;
        case Cache_FIFO:
            cache_fifo_replacement(c, set, addr);
            break;
        default:
            fprintf(stderr, "Unknown cache policy\n");
            exit(1);
    }

    return CACHE_MISS;
}



