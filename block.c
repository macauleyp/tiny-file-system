   /*
 *  Copyright (C) 2019 CS416 Spring 2019
 *	
 *	Tiny File System
 *
 *	File:	block.c
 *  Author: Yujie REN
 *	Date:	April 2019
 *
 */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "block.h"

//Disk size set to 32MB
#define DISK_SIZE	32*1024*1024

int diskfile = -1;

//Creates a file which is your new emulated disk
void dev_init(const char* diskfile_path) {
    if (diskfile >= 0) {
		return;
    }
    
    diskfile = open(diskfile_path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (diskfile < 0) {
		perror("disk_open failed");
		exit(EXIT_FAILURE);
    }
	
    ftruncate(diskfile, DISK_SIZE);
}

//Function to open the disk file
int dev_open(const char* diskfile_path) {
    if (diskfile >= 0) {
		return 0;
    }
    
    diskfile = open(diskfile_path, O_RDWR, S_IRUSR | S_IWUSR);
    if (diskfile < 0) {
		perror("disk_open failed");
		return -1;
    }
	return 0;
}

void dev_close() {
    if (diskfile >= 0) {
		close(diskfile);
    }
}

//Read a block from the disk
int bio_read(const int block_num, void *buf) {
    int retstat = 0;
    retstat = pread(diskfile, buf, BLOCK_SIZE, block_num*BLOCK_SIZE);
    if (retstat <= 0) {
		memset (buf, 0, BLOCK_SIZE);
		if (retstat < 0)
			perror("block_read failed");
    }

    return retstat;
}

//Write a block to the disk
int bio_write(const int block_num, const void *buf) {
    int retstat = 0;
    retstat = pwrite(diskfile, buf, BLOCK_SIZE, block_num*BLOCK_SIZE);
    if (retstat < 0) {
		    perror("block_write failed");
    }
    return retstat;
}

