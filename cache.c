#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "cache.h"
#include "jbod.h"

//Uncomment the below code before implementing cache functions.
static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries) {
    //fail statements
    if (num_entries < 2){
	    printf("error 1");
	    return -1;
    }
    if (cache != NULL){
	    printf("error 2");
	    return -1;
    }
    if (num_entries > (JBOD_BLOCK_SIZE * JBOD_NUM_DISKS)){
	    printf("error 3");
	    return -1;
    }
    //create cache
    cache = calloc(num_entries, sizeof(cache_entry_t));
    if (cache == NULL){
	    printf("error 4");
	    return -1;
    }

    cache_size = num_entries;
    clock = 0;
    for(int i = 0; i < num_entries; i++){
	    cache[i].valid = false;
    }
    return 1;
}

int cache_destroy(void) {
    //fail case
    if (cache == NULL){
	    return -1;
    }
    //destroy cache
    free(cache);
    cache = NULL;
    clock = 0;
    cache_size = 0;
    return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  //fail case
  if ((buf == NULL) || (cache == NULL)){
	  return -1;
  }
  
  //lookup
  num_queries++;
  int i = 0;
  while (i < cache_size){
	  //if block in cache, update accesses and hits
	  if ((cache[i].valid == true) && (disk_num == cache[i].disk_num) && (block_num == cache[i].block_num)){
		  memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);
		  clock++;
		  cache[i].clock_accesses = clock;
		  num_hits++;
		  return 1;
	  }
	  i++;
  }

  //block not in cache
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  if (cache == NULL || buf == NULL){
	  return;
  }
  int i = 0;
  while (i < cache_size){
	  //if block in cache, update with new buf
	  if ((cache[i].valid == true) && (disk_num == cache[i].disk_num) && (block_num == cache[i].block_num)){
		  memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
		  clock++;
		  cache[i].clock_accesses = clock;
		  break;
	  }
	  i++;
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  //fail case
  if ((cache == NULL) || (buf == NULL)){
         	  return -1;
  }
  if ((disk_num < 0) || (disk_num >= JBOD_NUM_DISKS)){
	  return -1;
  }
  if ((block_num < 0) || (block_num >= JBOD_NUM_BLOCKS_PER_DISK)){
	  return -1;
  }

  //check for block
  for(int i = 0; i < cache_size; i++){
	  if ((cache[i].valid == true) && (disk_num == cache[i].disk_num) && (block_num == cache[i].block_num)){
		  return -1;
	  }
  }
  for(int i = 0; i < cache_size; i++){
	  if ((cache[i].valid == true) && (disk_num == cache[i].disk_num) && (block_num == cache[i].block_num)){
		  memcpy(cache[i].block,buf,JBOD_BLOCK_SIZE);
		  clock++;
		  cache[i].clock_accesses = clock++;
		  return 1;
	  }
  }

  //mru
  int new = -1;
  int old = -1;
  for(int i = 0; i < cache_size; i++){
	  if(cache[i].valid == false){
		  new = i;
		  break;
	  }
	  if(cache[i].clock_accesses > old){
		  old = cache[i].clock_accesses;
		  new = i;
	  }
  }

  //update vals
  cache[new].valid = true;
  cache[new].disk_num = disk_num;
  cache[new].block_num = block_num;
  //copy into cache
  memcpy(cache[new].block, buf, JBOD_BLOCK_SIZE);
  clock++;
  cache[new].clock_accesses = clock;
  return 1;
}

bool cache_enabled(void) {
  return cache != NULL;
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "num_hits: %d, num_queries: %d\n", num_hits, num_queries);
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}

int cache_resize(int new_num_entries) {
  //fail cases
  if((new_num_entries < 2) || (new_num_entries > (JBOD_NUM_DISKS * JBOD_BLOCK_SIZE))){
	  return -1;
  }
  
  if (cache == NULL){
	  return -1;
  }
  cache_entry_t *temp_cache = calloc(new_num_entries, sizeof(cache_entry_t));
  if(temp_cache == NULL){
	  return -1;
  }

  int i = 0;
  int valid_i = 0;
  while (i < cache_size && valid_i < new_num_entries){
	  if(cache[i].valid == true){
		  valid_i++;
		  temp_cache[valid_i] = cache[i];
	  }
	  i++;
  }

  //free cache then set cache to temp
  free(cache);
  cache = temp_cache;
  cache_size = new_num_entries;
  return 1;
}
