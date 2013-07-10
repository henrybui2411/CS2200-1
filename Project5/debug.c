#include "student_cache.h"

int decode_offset(address_t address, student_cache_t *cache);
int decode_index(address_t address, student_cache_t *cache);
int decode_tag(address_t address, student_cache_t *cache);

int calc_obits(student_cache_t *cache);
int calc_ibits(student_cache_t *cache);
int calc_tbits(student_cache_t *cache);

cache_block* find_block(address_t addr, student_cache_t *cache);
cache_block* find_invalid(address_t addr, student_cache_t *cache);
cache_block* find_LRU(address_t addr, student_cache_t *cache);
cache_block* get_block_from_way(int index, cache_way *way); 

void print_LRU(student_cache_t *cache, int block_index);

void set_used(student_cache_t *cache, int block_index, int way_index);

int two_power_of(int power);

int student_read(address_t addr, student_cache_t *cache, stat_t *stats){
    cache_block *block;
    
    print_LRU(cache,decode_index(addr,cache));
    /* Check if in cache */
    block = find_block(addr,cache);
    if(block == NULL) {
        /* It's a miss so look for an invalid spot */
        block = find_invalid(addr,cache);
        if(block == NULL) {
            /* No invalids so evict the LRU */
            block = find_LRU(addr,cache);
        }
        /* Set up the block for the new address */
        block->valid = 1;
        block->tag = decode_tag(addr,cache);
        return 0;
    }
    return 1;
}

void print_LRU(student_cache_t *cache, int block_index) {
    printf("LRU FOR INDEX: %d\n",block_index);
    cache_LRU *current = cache->LRUs + block_index;
    while(current->next != NULL) {
        printf("WAY: %d -->",current->way_index);
        current = current->next;
    }
    printf("WAY: %d\n",current->way_index);
}

void set_used(student_cache_t *cache, int block_index, int way_index) {
    printf("SETTING USED FOR INDEX: %d AND WAY: %d\n",block_index,way_index);
    int hit = 0;
    cache_LRU *current = cache->LRUs + block_index;
    while(current->next != NULL) {
        if(current->way_index == way_index) {
            hit = 1;
        }
        if(hit) {
            current->way_index = current->next->way_index;
        }
        current = current->next;
    }
    current->way_index = way_index;
}

cache_block* find_invalid(address_t addr, student_cache_t *cache) {
    int i;
    int index = decode_index(addr,cache);
    cache_block *block;
    for(i=0; i<cache->ways_size; i++) {
        block = get_block_from_way(index,cache->ways+i);
        if(!block->valid) {
            set_used(cache,index,i); 
            return block;
        } 
    }
    return NULL;
}

cache_block* find_block(address_t addr, student_cache_t *cache) {
    int i;
    int index = decode_index(addr,cache);
    int tag = decode_tag(addr,cache);
    cache_block *block;
    for(i=0; i<cache->ways_size; i++) {
       block = get_block_from_way(index,cache->ways+i);
       if(block->valid && block->tag == tag) {
           set_used(cache,index,i); 
           return block;
       }
    }
    return NULL;
}

cache_block* find_LRU(address_t addr, student_cache_t *cache) {
    int index = decode_index(addr,cache);
    cache_LRU *lru;
    cache_block *block;
    lru = cache->LRUs + index; 
    block = get_block_from_way(index,cache->ways + lru->way_index);
    set_used(cache,index,lru->way_index);
    return block;

}


cache_block* get_block_from_way(int index, cache_way *way) {
    return way->blocks + index;
}

int student_write(address_t addr, student_cache_t *cache, stat_t *stats){
/*
    cache_block *block = find_block(addr,cache);
    if(block != NULL) {
        block->dirty = 1;
        return 1;
    }*/
    return 0;
}

int decode_offset(address_t address, student_cache_t *cache) {
    int obits = calc_obits(cache);
    return address & ((1 << obits) - 1);
}

int decode_index(address_t address, student_cache_t *cache) {
    int obits = calc_obits(cache);
    int ibits = calc_ibits(cache);
    return (address & (((1 << ibits) - 1) << obits)) >> obits;
}

int decode_tag(address_t address, student_cache_t *cache) {
    int obits = calc_obits(cache);
    int ibits = calc_ibits(cache);
    int tbits = calc_tbits(cache);
    return (address & (((1 << tbits) - 1) << obits << ibits)) >> obits >> ibits; 
}

int calc_obits(student_cache_t *cache) {
    return cache->B; 
}

int calc_ibits(student_cache_t *cache) {
    return cache->C - cache->B - cache-> S;
}

int calc_tbits(student_cache_t *cache) {
    return 32 - calc_obits(cache) - calc_ibits(cache);
}

student_cache_t *allocate_cache(int C, int B, int S, int WP, stat_t* statistics){
    int i,j;
    cache_way *way;
    cache_block *block;
    cache_LRU *lru;

    student_cache_t *cache = malloc(sizeof(student_cache_t));    

    cache->WP = WP;
    cache->C = C;
    cache->B = B;
    cache->S = S;
    cache->ways_size = two_power_of(S);
    cache->ways = calloc(cache->ways_size, sizeof(cache_way));
    cache->LRUs_size = two_power_of(C-B-S);
    cache->LRUs = calloc(cache->LRUs_size, sizeof(cache_LRU));

    for(i=0; i<cache->LRUs_size; i++) {
        lru = cache->LRUs + i;
        lru->way_index = 0;
        for(j=0; j<cache->ways_size; j++) {
           lru->way_index = j;
           if(j != cache->ways_size - 1) {
               lru->next = malloc(sizeof(cache_LRU));
               lru = lru->next;
           }
        }
    }

    for(i=0; i<cache->ways_size; i++) {
        way = cache->ways + i;
        way->blocks_size = two_power_of(C-B-S);
        printf("BLOCKS_SIZE:%d\n",way->blocks_size);
        way->blocks = calloc(way->blocks_size, sizeof(cache_block));

        for(j=0; j<way->blocks_size;j++) {
            block = way->blocks + i;
            block->valid = 0;
            block->dirty = 0;
            block->tag = 0;
        }
    }
	return cache;
}

int two_power_of(int power) {
    return 1 << power;
}

void finalize_stats(student_cache_t *cache, stat_t *statistics){

}

