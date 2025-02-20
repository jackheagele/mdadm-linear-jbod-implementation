#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h"
#include "jbod.h"
#include "mdadm.h"
#include "net.h"

/*Use your mdadm code*/
int mounted = 0;
int write_permission = 0;
uint32_t helper(uint32_t diskID, uint32_t blockID, uint32_t cmd);

int mdadm_mount(void) {
    if (mounted==1) {
      return -1;
    }
    int result = jbod_client_operation(JBOD_MOUNT << 12, NULL);
    if (result == 0) {
      mounted = 1;
      return 1;
    }
    return -1;
}

int mdadm_unmount(void) {
    if (mounted==0) {
      return -1;
    }
    int result = jbod_client_operation(JBOD_UNMOUNT << 12, NULL);
    if (result == 0) {
      mounted = 0;
      return 1;
    }
    return -1;
}

int mdadm_write_permission(void){
    int result = jbod_client_operation(JBOD_WRITE_PERMISSION << 12, NULL);
    if (result == 0) {
        write_permission = 1;
        return 0;
    }
    return -1; 
}


int mdadm_revoke_write_permission(void){
    int result = jbod_client_operation(JBOD_REVOKE_WRITE_PERMISSION << 12, NULL);
    if (result == 0) {
        write_permission = 0;
        return 0; 
    }
    return -1;
}


int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  	if (len > 0 && buf == NULL) {
    		return -4;
  	}
	if (len > 1024) {
		return -2;
	}
	if (mounted == 0) {
		return -3;
	}
	if (addr + len > (JBOD_NUM_DISKS * JBOD_NUM_BLOCKS_PER_DISK * JBOD_BLOCK_SIZE)) {
		return -1;
	}

	uint32_t num_read = 0;
	uint32_t cur_addr = addr;

	while (num_read < len) {
		uint32_t disk_num = cur_addr / JBOD_DISK_SIZE;
		uint32_t block_num = (cur_addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
		uint32_t offset = (cur_addr % JBOD_DISK_SIZE) % JBOD_BLOCK_SIZE;
		uint8_t block[JBOD_BLOCK_SIZE];

		if (cache_enabled() && cache_lookup(disk_num, block_num, block) == 1) {
         		//cache hit
       		} 
		else {
        	        uint32_t cmd = (JBOD_SEEK_TO_DISK << 12) | disk_num;
            	    jbod_client_operation(cmd, NULL);
           	        cmd = (JBOD_SEEK_TO_BLOCK << 12) | (block_num << 4);
         	        jbod_client_operation(cmd, NULL);
          	        cmd = (JBOD_READ_BLOCK << 12);
           	        jbod_client_operation(cmd, block);

       		    	if (cache_enabled()) {
               			cache_insert(disk_num, block_num, block);
               		}
        	}

		uint32_t copy_bytes = JBOD_BLOCK_SIZE - offset;
		if (copy_bytes > len - num_read) {
			copy_bytes = len - num_read; 
		}

		memcpy(buf + num_read, block + offset, copy_bytes);
		num_read += copy_bytes;
		cur_addr += copy_bytes;

	}
	return num_read;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  	if (mounted == 0) {
    		return -3;
  	}
	if (write_permission == 0) {
		return -5;
	}
	if (len > 1024) {
    		return -2;
  	}
	if (len > 0 && buf == NULL) {
    		return -4;
  	}
  	if (addr + len > (JBOD_NUM_DISKS * JBOD_NUM_BLOCKS_PER_DISK * JBOD_BLOCK_SIZE)) {
    		return -1;
  	}

	uint32_t num_written = 0;
	uint32_t cur_addr = addr;

	while (num_written < len){
		uint32_t disk_num = cur_addr / JBOD_DISK_SIZE;
		uint32_t block_num = (cur_addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
		uint32_t offset = cur_addr % JBOD_BLOCK_SIZE;

		uint32_t cmd = helper(disk_num, 0, JBOD_SEEK_TO_DISK);
		jbod_client_operation(cmd, NULL);
		cmd = helper(0, block_num, JBOD_SEEK_TO_BLOCK);
		jbod_client_operation(cmd, NULL);
		uint32_t bytes_rem = JBOD_BLOCK_SIZE - offset;
		uint32_t copy_bytes = (bytes_rem < (len - num_written)) ? bytes_rem : (len - num_written);

		uint8_t cur_buf[JBOD_BLOCK_SIZE];
		jbod_client_operation(JBOD_READ_BLOCK << 12, cur_buf);
		cmd = helper(disk_num, 0, JBOD_SEEK_TO_DISK);
		jbod_client_operation(cmd, NULL);
		cmd = helper(0, block_num, JBOD_SEEK_TO_BLOCK);
		jbod_client_operation(cmd, NULL);
		memcpy(cur_buf + offset, buf + num_written, copy_bytes);
		cmd = helper(0, 0, JBOD_WRITE_BLOCK);
		jbod_client_operation(cmd, cur_buf);

		if (cache_enabled()) {
    		cache_update(disk_num, block_num, cur_buf);
		}

		num_written += copy_bytes;
		cur_addr += copy_bytes;
	}
	return num_written;
}

uint32_t helper(uint32_t diskID, uint32_t blockID, uint32_t cmd) {
	return (blockID << 4) | (cmd<< 12) | (diskID);
}