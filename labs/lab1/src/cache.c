#include "debug.h"
#include "cache.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

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
    if(c->total_size ==0){
        Cache_Block *block = c->blocks;
    
    if (block->valid && (block->tag == addr)) {
        block->valid = false;
        return CACHE_HIT;
    }
    else {
        block->tag = addr;
        block->valid = true;
        return CACHE_MISS;
    }
    }
}
