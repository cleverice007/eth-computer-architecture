#ifndef _CACHE_H_
#define _CACHE_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct Cache_Block Cache_Block;
typedef struct Cache_State Cache_State;


struct Cache_Block {
    uint32_t tag;
    bool valid;
    // to implement lru or mru
    int last_access;
    // implement fifo or lifo
    int insert_time;
};

typedef enum Cache_Policy{
   Cache_LRU_LRU,
   Cache_FIFO
} Cache_Policy;

typedef void (*replacement_func_t)(Cache_State *c, Cache_Block *set, uint32_t tag);

typedef enum Cache_Result { CACHE_MISS, CACHE_HIT } Cache_Result;

struct Cache_State{
    int total_size;
    int block_size;
    int log2_block_size;
    int num_blocks;
    int num_sets;
    int log2_num_sets;
    int num_ways;
    Cache_Block *blocks;
    Cache_Policy policy; // LRU, FIFO, etc.
    // Global clock: incremented on every access, used for LRU and FIFO tracking
    int timestamp;

};

// initialize cache state
void cache_init(Cache_State *c, int total_size, int block_size, int num_ways,
                Cache_Policy policy, bool debug);

// access cache state
enum Cache_Result cache_access(Cache_State *c, uint32_t addr);

// free cache state
void cache_free(Cache_State *c);


#endif