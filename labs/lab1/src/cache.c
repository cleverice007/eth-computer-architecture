#include "debug.h"
#include "cache.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void cache_init(Cache_State *c, int total_size, int block_size, int num_ways,
                Cache_Policy policy, bool debug) {
    assert(c != NULL);
    assert(total_size > 0 && block_size > 0 && num_ways > 0);

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
