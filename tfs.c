/*
 *  Copyright (C) 2019 CS416 Spring 2019
 *	
 *	Tiny File System
 *
 *	File:	tfs.c
 *  Author: Yujie REN
 *	Date:	April 2019
 *             
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>
#include <inttypes.h>
#include "block.h"
#include "tfs.h"

char diskfile_path[PATH_MAX];

// Declare your in-memory data structures here

bitmap_t ibmap = NULL;
bitmap_t dbmap = NULL;

struct superblock *superblock;
struct inode *inodes[MAX_INUM+2];
struct dirent *dirent;

int counter = 0;

/*
# of data blocks: 8125
# of inode blocks: 64
Super block, ibmap, dbmap: 3
*/

/* 
 * Get available inode number from bitmap
 */

/*
#################################################################################################################################
#																															    #	
#	int get_avail_ino() 																										#
#																															    #	
#	Step 1: Read inode bitmap from disk																							#
#	Step 2: Traverse inode bitmap to find an available slot																		#
#	Step 3: Update inode bitmap and write to disk 																				#
#																															    #	
#################################################################################################################################
*/
int get_avail_ino() {
	
	int i;
	bio_read(superblock->i_bitmap_blk,ibmap);
	for(i = 0; i < (MAX_INUM + 1); i++){

		if(get_bitmap(ibmap,i) == 0){

			set_bitmap(ibmap,i);
			bio_write(superblock->i_bitmap_blk,ibmap);
			return i;

		}

	}

	return -1;
}

/* 
#################################################################################################################################
#																															    #	
#   Get available data block number from bitmap																					#
#																															    #		
#	Step 1: Read data block bitmap from disk																					#
#	Step 2: Traverse data block bitmap to find an available slot																#
#	Step 3: Update data block bitmap and write to disk																			# 
#																															    #	
#################################################################################################################################
 */
int get_avail_blkno() {
	
	int i;
	bio_read(superblock->d_bitmap_blk,dbmap);
	for(i = 0; i < MAX_DNUM; i++){

		if(get_bitmap(dbmap,i) == 0){
			counter++;
			printf("\n\nCOUNTER: %d\n\n", counter);
			set_bitmap(dbmap,i);
			bio_write(superblock->d_bitmap_blk,dbmap);
			return i;

		}

	}

	return -1;
}

/* 
 * inode operations
 */

/*
#################################################################################################################################
#																															    #	
#	int readi(uint16_t ino, struct inode *inode)																				#
#																															    #	
#	Step 1: Get the inode's on-disk block number																				#
#	Step 2: Get offset of the inode in the inode on-disk block																	#
#	Step 3: Read the block from disk and then copy into inode structure															#
#																															    #	
#################################################################################################################################
*/
int readi(uint16_t ino, struct inode *inode) {

	int ino_blkno = superblock->i_start_blk + (ino/(BLOCK_SIZE/sizeof(struct inode))); //offset of inode within the iblock region.
	struct inode *ino_blk = malloc(BLOCK_SIZE);
	int status = bio_read(ino_blkno, ino_blk);

	int inoOffsetInBlk = (ino % (BLOCK_SIZE/sizeof(struct inode)));
		
	inode->ino = ino_blk[inoOffsetInBlk].ino;
	inode->valid = ino_blk[inoOffsetInBlk].valid;
	inode->size = ino_blk[inoOffsetInBlk].size;
	inode->type = ino_blk[inoOffsetInBlk].type;
	inode->link = ino_blk[inoOffsetInBlk].link;
	int i;
	for(i = 0; i < DIRECT_PTRS;i++){

		inode->direct_ptr[i] = ino_blk[inoOffsetInBlk].direct_ptr[i];

		if(i < INDIRECT_PTRS){

			inode->indirect_ptr[i] = ino_blk[inoOffsetInBlk].indirect_ptr[i];

		}

	}
	inode->vstat.st_dev = ino_blk[inoOffsetInBlk].vstat.st_dev;
	inode->vstat.st_ino = ino_blk[inoOffsetInBlk].vstat.st_ino;
	inode->vstat.st_mode = ino_blk[inoOffsetInBlk].vstat.st_mode;
	inode->vstat.st_nlink = ino_blk[inoOffsetInBlk].vstat.st_nlink;
	inode->vstat.st_uid = ino_blk[inoOffsetInBlk].vstat.st_uid;
	inode->vstat.st_gid = ino_blk[inoOffsetInBlk].vstat.st_gid;
	inode->vstat.st_atime = ino_blk[inoOffsetInBlk].vstat.st_atime;
	inode->vstat.st_mtime = ino_blk[inoOffsetInBlk].vstat.st_mtime;

	inodes[ino - 1]->ino = ino_blk[inoOffsetInBlk].ino;
	inodes[ino - 1]->valid = ino_blk[inoOffsetInBlk].valid;
	inodes[ino - 1]->size = ino_blk[inoOffsetInBlk].size;
	inodes[ino - 1]->type = ino_blk[inoOffsetInBlk].type;
	inodes[ino - 1]->link = ino_blk[inoOffsetInBlk].link;
	
	for(i = 0; i < DIRECT_PTRS;i++){

		inodes[ino - 1]->direct_ptr[i] = ino_blk[inoOffsetInBlk].direct_ptr[i];
		if(i < INDIRECT_PTRS){

			inodes[ino - 1]->indirect_ptr[i] = ino_blk[inoOffsetInBlk].indirect_ptr[i];
			
		}

	}
	inodes[ino - 1]->vstat.st_dev = ino_blk[inoOffsetInBlk].vstat.st_dev;
	inodes[ino - 1]->vstat.st_ino = ino_blk[inoOffsetInBlk].vstat.st_ino;
	inodes[ino - 1]->vstat.st_mode = ino_blk[inoOffsetInBlk].vstat.st_mode;
	inodes[ino - 1]->vstat.st_nlink = ino_blk[inoOffsetInBlk].vstat.st_nlink;
	inodes[ino - 1]->vstat.st_uid = ino_blk[inoOffsetInBlk].vstat.st_uid;
	inodes[ino - 1]->vstat.st_gid = ino_blk[inoOffsetInBlk].vstat.st_gid;
	inodes[ino - 1]->vstat.st_atime = ino_blk[inoOffsetInBlk].vstat.st_atime;
	inodes[ino - 1]->vstat.st_mtime = ino_blk[inoOffsetInBlk].vstat.st_mtime;

	free(ino_blk);
	ino_blk = NULL;
	
	return status;
}

/*
#################################################################################################################################
#																																#																													
#	int writei(uint16_t ino, struct inode *inode)																				#
#																															    #
#	Step 1: Get the block number where this inode resides on disk																#
#	Step 2: Get the offset in the block where this inode resides on disk														#
#	Step 3: Write inode to disk 																								#
#																															    #
#################################################################################################################################
*/
int writei(uint16_t ino, struct inode *inode) {

	int ino_blkno = superblock->i_start_blk + (ino/(BLOCK_SIZE/sizeof(struct inode)));//offset of inode within the iblock region.
	struct inode *ino_blk = malloc(BLOCK_SIZE);
	bio_read(ino_blkno, ino_blk);


	int inoOffsetInBlk = (ino % (BLOCK_SIZE/sizeof(struct inode)));

	ino_blk[inoOffsetInBlk].ino = inode->ino;
	ino_blk[inoOffsetInBlk].valid = inode->valid;
	ino_blk[inoOffsetInBlk].size = inode->size;
	ino_blk[inoOffsetInBlk].type = inode->type;
	ino_blk[inoOffsetInBlk].link = inode->link;
	int i;
	for(i = 0; i < DIRECT_PTRS;i++){

		ino_blk[inoOffsetInBlk].direct_ptr[i] = inode->direct_ptr[i];

		if(i < INDIRECT_PTRS){

			ino_blk[inoOffsetInBlk].indirect_ptr[i] = inode->indirect_ptr[i];

		}

	}
	ino_blk[inoOffsetInBlk].vstat.st_dev = inode->vstat.st_dev;
	ino_blk[inoOffsetInBlk].vstat.st_ino = inode->vstat.st_ino;
	ino_blk[inoOffsetInBlk].vstat.st_mode = inode->vstat.st_mode;
	ino_blk[inoOffsetInBlk].vstat.st_nlink = inode->vstat.st_nlink;
	ino_blk[inoOffsetInBlk].vstat.st_uid = inode->vstat.st_uid;
	ino_blk[inoOffsetInBlk].vstat.st_gid  = inode->vstat.st_gid;
	ino_blk[inoOffsetInBlk].vstat.st_atime = inode->vstat.st_atime;
	ino_blk[inoOffsetInBlk].vstat.st_mtime = inode->vstat.st_mtime;

	inodes[ino - 1]->ino = ino_blk[inoOffsetInBlk].ino;
	inodes[ino - 1]->valid = ino_blk[inoOffsetInBlk].valid;
	inodes[ino - 1]->size = ino_blk[inoOffsetInBlk].size;
	inodes[ino - 1]->type = ino_blk[inoOffsetInBlk].type;
	inodes[ino - 1]->link = ino_blk[inoOffsetInBlk].link;
	
	for(i = 0; i < DIRECT_PTRS;i++){

		inodes[ino - 1]->direct_ptr[i] = ino_blk[inoOffsetInBlk].direct_ptr[i];
		if(i < INDIRECT_PTRS){

			inodes[ino - 1]->indirect_ptr[i] = ino_blk[inoOffsetInBlk].indirect_ptr[i];
			
		}

	}
	inodes[ino - 1]->vstat.st_dev = ino_blk[inoOffsetInBlk].vstat.st_dev;
	inodes[ino - 1]->vstat.st_ino = ino_blk[inoOffsetInBlk].vstat.st_ino;
	inodes[ino - 1]->vstat.st_mode = ino_blk[inoOffsetInBlk].vstat.st_mode;
	inodes[ino - 1]->vstat.st_nlink = ino_blk[inoOffsetInBlk].vstat.st_nlink;
	inodes[ino - 1]->vstat.st_uid = ino_blk[inoOffsetInBlk].vstat.st_uid;
	inodes[ino - 1]->vstat.st_gid = ino_blk[inoOffsetInBlk].vstat.st_gid;
	inodes[ino - 1]->vstat.st_atime = ino_blk[inoOffsetInBlk].vstat.st_atime;
	inodes[ino - 1]->vstat.st_mtime = ino_blk[inoOffsetInBlk].vstat.st_mtime;

	int status = bio_write(ino_blkno, ino_blk);
	free(ino_blk);
	ino_blk = NULL;
	return status;
}

/* 
 * directory operations
 */

/*
#################################################################################################################################
#																															    #	
#	int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) 										#
#																															    #	
#	Step 1: Call readi() to get the inode using ino (inode number of current directory)											#
#  	Step 2: Get data block of current directory from inode																		#
#  	Step 3: Read directory's data block and check each directory entry.															#
#																															    #	
#  	Note: If the name matches, then copy directory entry to dirent structure													#
#																															    #	
#################################################################################################################################
*/
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {
	
	char *full_path = strdup(fname);
	char *full_path_dup = strdup(fname);
	char *basepath = basename(full_path);
	
	uint16_t status = searchiFile(inodes[0], inodes[0]->ino, full_path_dup);
	if(status > 0){
		readi(status, inodes[ino-1]);
		dirent->ino = status;
		dirent->valid = 1;
		strcpy(dirent->name,basepath);

		return 0;
	}

	return -1;
}


/*
#################################################################################################################################
#																																#
#	int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) 									#
#																																#
#	Step 1: Read dir_inode's data block and check each directory entry of dir_inode												#
#	Step 2: Check if fname (directory name) is already used in other entries													#
#	Step 3: Add directory entry in dir_inode's data block and write to disk														#
#																																#
#	Allocate a new data block for this directory if it does not exist															#
#	Update directory inode																										#
#	Write directory entry																										#
#																																#
#################################################################################################################################
*/
int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {
	
	int numberOfEntries = (int)(BLOCK_SIZE/(sizeof(struct dirent)));
	int i, start, directPtrIndex;
	dir_inode.link = dir_inode.link + 1;
	if(dir_inode.link < 15){

		start = 3;
		directPtrIndex = 0;

	}else{

		start = 0;	
		if(dir_inode.link == 15){

			directPtrIndex = 1;

		}else{

			directPtrIndex = (int)((dir_inode.link + 1)/ 16);

		}		

	}
	
	int workingDirOffset;
	struct dirent *workingDirBuf = (struct dirent *)malloc(BLOCK_SIZE);
	if(directPtrIndex > 0 && dir_inode.direct_ptr[directPtrIndex] == -1){

		dir_inode.direct_ptr[directPtrIndex] = get_avail_blkno();
		workingDirOffset = superblock->d_start_blk + dir_inode.direct_ptr[directPtrIndex];
		for(i = 0; i < numberOfEntries; i++){
				
			workingDirBuf[i].valid = 0;
		}		
		bio_write(workingDirOffset, workingDirBuf);
		start = 0;
	}else{
	
		workingDirOffset = superblock->d_start_blk + dir_inode.direct_ptr[directPtrIndex];
	}
	memset(workingDirBuf,0,BLOCK_SIZE);
	bio_read(workingDirOffset, workingDirBuf);


	char *basename_path = strdup(fname);
	char *base = basename(basename_path); 
	char addPathToDir[name_len];
	strcpy(addPathToDir,base);
	addPathToDir[name_len - 1] = '\0';
	for(i = start; i < numberOfEntries; i++){

		if(workingDirBuf[i].valid == 0){
			workingDirBuf[i].valid = 1;
			workingDirBuf[i].ino = f_ino;
			strcpy(workingDirBuf[i].name,base);
			break;
		}
	}	

	bio_write(workingDirOffset, workingDirBuf);	
	writei(dir_inode.ino,&dir_inode);

	free(workingDirBuf);
	workingDirBuf = NULL;	

	return 0;
}
/*
#################################################################################################################################
#																																#
#	int dir_remove(struct inode dir_inode, const char *fname, size_t name_len)													#
#																																#
#	Step 1: Read dir_inode's data block and checks each directory entry of dir_inode											#
#	Step 2: Check if fname exist																								#
#	Step 3: If exist, then remove it from dir_inode's data block and write to disk												#
#																																#
#################################################################################################################################
*/
int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	char *pathname = strdup(fname);
	char *full_path_base = strdup(fname);
	char *basename_path = basename(full_path_base);
	struct inode remove_ino;
	
	uint16_t ino = searchiFile(inodes[0], inodes[0]->ino, pathname);
	if(ino == 0){
		
		return -1;

	}
	if(inodes[ino-1]->link > 2){

		return -1;

	}
	dir_inode.link = dir_inode.link - 1;
	int workingDirOffset = superblock->d_start_blk + (dir_inode.direct_ptr[0]);
	struct dirent *workingDirBuf = (struct dirent *)malloc(BLOCK_SIZE);
	bio_read(workingDirOffset, workingDirBuf);
	
	int numberOfEntries = (int)(BLOCK_SIZE/(sizeof(struct dirent)));
	int i;
	for(i = 0; i < numberOfEntries; i++){

		if(strcmp(workingDirBuf[i].name,basename_path) == 0){
			workingDirBuf[i].valid = 0;
		}
	}	

	unset_bitmap(ibmap,remove_ino.ino);

	bio_write(workingDirOffset,workingDirBuf);
	writei(inodes[dir_inode.ino-1]->ino,inodes[dir_inode.ino-1]);
	writei(inodes[ino-1]->ino,inodes[ino-1]);
	bio_write(superblock->i_bitmap_blk,ibmap);
	free(workingDirBuf);
	workingDirBuf = NULL;

	return 0;
}

/* 
 * namei operation
 */
/*
#################################################################################################################################
#																																#
#	int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) 													#
#																																#
#	Step 1: Resolve the path name, walk through path, and finally, find its inode.												#
#	Note: You could either implement it in a iterative way or recursive way														#
#																																#
#################################################################################################################################
*/
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {

	const char *full_path_find = strdup(path);
	char *full_path = strdup(path);
	
	uint16_t node_ino = searchiFile(inodes[0], inodes[0]->ino,full_path);
	
	if(node_ino > 0){

		struct dirent *dirent = (struct dirent *)malloc(sizeof(dirent));

		int found = dir_find(1, full_path_find, (strlen(full_path_find) + 1), dirent);

		if(found == -1){
			
			free(dirent);
			dirent = NULL;
			return -1;

		}
		readi(inodes[(dirent->ino-1)]->ino,inodes[(dirent->ino-1)]);
		readi(inodes[(dirent->ino-1)]->ino,inode);
		inode->ino = inodes[(dirent->ino-1)]->ino;
		inode->valid = dirent->valid;
		inode->size = inodes[(dirent->ino-1)]->size;
		inode->type = inodes[(dirent->ino-1)]->type;
		inode->link = inodes[(dirent->ino-1)]->link;

		int i;		
		for(i = 0; i < 16; i++){

			inode->direct_ptr[i] = inodes[(dirent->ino-1)]->direct_ptr[i];
			if(i < 8){

				inode->indirect_ptr[i] = inodes[(dirent->ino-1)]->indirect_ptr[i];

			}
		}
		writei(inodes[(dirent->ino-1)]->ino,inodes[(dirent->ino-1)]);
		free(dirent);
		dirent = NULL;

		return 0;

	}else{

		return -1;

	}
		
}

/* 
#################################################################################################################################
#																																#
#  	Make file system																											#
#																																#
#	- Call dev_init() to initialize (Create) Diskfile																			#
#	- write superblock information																								#
#	- initialize inode bitmap																									#
#	- initialize data block bitmap																								#
#	- update bitmap information for root directory																				#
#	- update inode for root directory																							#
#																																#
#################################################################################################################################
 */
int tfs_mkfs() { 

	ibmap = (bitmap_t)malloc(BLOCK_SIZE);
	dbmap = (bitmap_t)malloc(BLOCK_SIZE);
	int i;
	for(i = 0; i < MAX_INUM;i++){

		ibmap[i] = 0;

	}
	for(i = 0; i < MAX_DNUM; i++){

		dbmap[i] = 0;

	}
		
	superblock = (struct superblock *)malloc(BLOCK_SIZE);

	for(i = 0; i < MAX_INUM; i++){

		inodes[i] = (struct inode *)malloc(sizeof(struct inode));
		inodes[i]->link = 0;
		int j;
		for(j = 0; j < 16; j++){
			
			inodes[i]->direct_ptr[j] = -1;

			if(j < 8){

				inodes[i]->indirect_ptr[j] = -1;

			}

		}
	}
	dev_init(diskfile_path);
	superblock->magic_num = MAGIC_NUM;
	superblock->max_inum = MAX_INUM;
	superblock->max_dnum = MAX_DNUM;
	superblock->i_bitmap_blk = 1;
	superblock->d_bitmap_blk = superblock->i_bitmap_blk + 1;
	superblock->i_start_blk = superblock->d_bitmap_blk + 1;
	superblock->d_start_blk	= superblock->i_start_blk+NUM_INODE_BLOCKS+1;	

	get_avail_ino();
	
	inodes[0]->ino = get_avail_ino();
	inodes[0]->valid = 1;
	inodes[0]->size = BLOCK_SIZE;
	inodes[0]->type = DIR;
	inodes[0]->link = 2;
	inodes[0]->direct_ptr[0] = get_avail_blkno();
	(inodes[0]->vstat).st_atime = time(NULL); 
 	(inodes[0]->vstat).st_mtime = time(NULL); 
	
	int numberOfEntries = (int)(BLOCK_SIZE/(sizeof(struct dirent)));
	struct dirent *newDirEntries = (struct dirent *)malloc(BLOCK_SIZE);

	
	newDirEntries[0].ino = inodes[0]->ino;
	newDirEntries[0].valid = 1;
	strcpy(newDirEntries[0].name,"/");

	newDirEntries[1].ino = inodes[0]->ino;
	newDirEntries[1].valid = 1;
	strcpy(newDirEntries[1].name,".");

	newDirEntries[2].ino = inodes[0]->ino;
	newDirEntries[2].valid = 1;
	strcpy(newDirEntries[2].name,"..");

	for(i = 3; i < numberOfEntries; i++){
		
		newDirEntries[i].valid = 0;
			
	}

	int firstFreeDataBlock = inodes[0]->direct_ptr[0];
	int newDirOffset = superblock->d_start_blk + firstFreeDataBlock;

	bio_write(newDirOffset,newDirEntries);

	//insert(inodes[0]->ino, "/");
	
	writei(inodes[0]->ino,inodes[0]);
	for(i = 1; i < MAX_INUM; i++){

	
		inodes[i]->valid = 0;
		inodes[i]->size = BLOCK_SIZE;
		inodes[i]->link = 0;		
		
	}

	return 0;
}


/* 
 * FUSE file operations
 */

/*
#################################################################################################################################
#																																#
#	static void *tfs_init(struct fuse_conn_info *conn)																			#
#																																#
#	Step 1a: If disk file is not found, call mkfs																				#
#	Step 1b: If disk file is found, just initialize in-memory data structures													#
#  			 and read superblock from disk																						#
#																																#
#################################################################################################################################
*/
static void *tfs_init(struct fuse_conn_info *conn) {
		
	int status = dev_open(diskfile_path);
	if(status == -1){

		tfs_mkfs();

	}
	if(status != -1){	
	
		ibmap = (bitmap_t)malloc(BLOCK_SIZE);
		dbmap = (bitmap_t)malloc(BLOCK_SIZE);
		superblock = (struct superblock *)malloc(BLOCK_SIZE); //bio_read(0,superblock);

		int i;
		for(i = 0; i < MAX_INUM; i++){
		
			inodes[i] = (struct inode *)malloc(sizeof(struct inode));

		}
		bio_read(0,superblock);
		if(superblock->magic_num != MAGIC_NUM){
	
				fprintf(stderr,"FILE SYSTEM NOT FORMATTED WITH 0x5C3A\n");
				exit(1);

		}
		
		bio_read(superblock->i_bitmap_blk,ibmap);
		bio_read(superblock->d_bitmap_blk,dbmap);

		for(i = 0; i < MAX_INUM; i++){

			readi((i+1),inodes[i]);

		}

	}

	return NULL;
}
/*
#################################################################################################################################
#																																#
#	static void tfs_destroy(void *userdata)																						#
#																																#	
#	Step 1: De-allocate in-memory data structures																				#
#	Step 2: Close diskfile																										#
#																																#
#################################################################################################################################
*/
static void tfs_destroy(void *userdata) {

	
	bio_write(0,superblock);
	bio_write(superblock->i_bitmap_blk,ibmap);
	bio_write(superblock->d_bitmap_blk,dbmap);

	int i;
	for(i = 0; i < MAX_INUM; i++){

		writei(inodes[i]->ino,inodes[i]);
		free(inodes[i]);
	}

	free(superblock);
	free(ibmap);
	free(dbmap);
	//free(inode);
	dev_close();
}
/*
#################################################################################################################################
#																																#
#	static int tfs_getattr(const char *path, struct stat *stbuf)																#
#																																#
#	Step 1: call get_node_by_path() to get inode from path																		#
#	Step 2: fill attribute of file into stbuf from inode																		#
#																																#
#################################################################################################################################
*/
static int tfs_getattr(const char *path, struct stat *stbuf) {
	
	struct inode *fill_stbuf = (struct inode *)malloc(sizeof(struct inode)); 
	
	int retrieved = get_node_by_path(path,stbuf->st_ino,fill_stbuf);

	if(retrieved == 0){
		
		stbuf->st_uid = getuid();
		stbuf->st_gid = getgid();
	
		if(fill_stbuf->type == DIR){
			
			stbuf->st_mode   = S_IFDIR | 0755;
			stbuf->st_nlink  = inodes[(fill_stbuf->ino - 1)]->link;
			time(&stbuf->st_mtime);
			
		}else{

			stbuf->st_mode = S_IFREG | 0644;
			stbuf->st_nlink = 1;
			stbuf->st_size = inodes[(fill_stbuf->ino - 1)]->size;
			
		}

	
		free(fill_stbuf);
		fill_stbuf = NULL;
		return 0;
	}

	free(fill_stbuf);
	fill_stbuf = NULL;
	return -ENOENT; // May need to use -ENOENT
}

/*
#################################################################################################################################
#																																#
#	static int tfs_opendir(const char *path, struct fuse_file_info *fi)															#
#																																#
#	Step 1: Call get_node_by_path() to get inode from path																		#
#	Step 2: If not find, return -1																								#
#																																#
#################################################################################################################################
*/
static int tfs_opendir(const char *path, struct fuse_file_info *fi) {

	struct inode *inode = (struct inode *)malloc(sizeof(struct inode));
	int status = get_node_by_path(path,inodes[0]->ino,inode);
	if(status == 0){
		free(inode);
		inode = NULL;
		return 0;

	}else{

		return -1;

	}
}

/*
#################################################################################################################################
#																																#	
#	static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)		# 
#																																#	
#	Step 1: Call get_node_by_path() to get inode from path																		#
#	Step 2: Read directory entries from its data blocks, and copy them to filler												#
#																																#									
#################################################################################################################################
*/
static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	struct inode *inode = (struct inode *)malloc(sizeof(struct inode));
	
	char *full_path = strdup(path);
	int retrieved = get_node_by_path(full_path, 1, inode);

	struct dirent *subFilesAndDirs = (struct dirent *)malloc(BLOCK_SIZE);
	if(retrieved == 0){

		int i;
		for(i = 0; i < 16; i++){
			if(inodes[inode->ino - 1]->direct_ptr[i] != -1){

				int numberOfEntries = (int)(BLOCK_SIZE/(sizeof(struct dirent)));
				int workingDirOffset = superblock->d_start_blk + (inodes[inode->ino - 1]->direct_ptr[i]);

				bio_read(workingDirOffset, subFilesAndDirs);

				int j = 1;
				while(j < numberOfEntries){

					if(subFilesAndDirs[j].valid == 1){
					
						int isBuffFull = filler(buffer,subFilesAndDirs[j].name,NULL,0);

						if(isBuffFull == -1){
				
							return 0;

						}

					}
					j++;
					
				}
				memset(subFilesAndDirs,0,BLOCK_SIZE);
			}else{

				continue;

			}

		}

		free(subFilesAndDirs);
		subFilesAndDirs = NULL;
		free(inode);		
		inode = NULL;
		return 0;
	}
	free(subFilesAndDirs);
	subFilesAndDirs = NULL;
	free(inode);		
	inode = NULL;
	return -1;
}

/*
#################################################################################################################################
#																																#
#	static int tfs_mkdir(const char *path, mode_t mode)																			#
#																																#	
#	Step 1: Use dirname() and basename() to separate parent directory path and target directory name							#
#	Step 2: Call get_node_by_path() to get inode of parent directory															#
#	Step 3: Call get_avail_ino() to get an available inode number																#
#	Step 4: Call dir_add() to add directory entry of target directory to parent directory										#
#	Step 5: Update inode for target directory																					#
#	Step 6: Call writei() to write inode to disk																				#
#																																#
#################################################################################################################################
*/
static int tfs_mkdir(const char *path, mode_t mode) {

	char *full_path = strdup(path);
	char *reg_char_path = strdup(path);
	char *reg_char_path_dup = strdup(path);
	char *parentpath = dirname(reg_char_path);
	char *childpath = basename(reg_char_path_dup);

	struct inode addDirectory;

	int retrieved = get_node_by_path(parentpath,1,&addDirectory); 
	
	if(retrieved == 0){

		int child_ino = get_avail_ino();
		
		dir_add(addDirectory, child_ino, full_path, (strlen(full_path)+1));

		inodes[child_ino-1]->ino = child_ino;
		inodes[child_ino-1]->valid = 1;
		inodes[child_ino-1]->size = BLOCK_SIZE;
		inodes[child_ino-1]->type = DIR;
		inodes[child_ino-1]->link = 2;
		(inodes[child_ino-1]->vstat).st_atime = time(NULL); 
		(inodes[child_ino-1]->vstat).st_mtime = time(NULL); 
		inodes[child_ino-1]->vstat.st_mode = mode;

		int numberOfEntries = (int)(BLOCK_SIZE/(sizeof(struct dirent)));
		struct dirent *newDirEntries = (struct dirent *)malloc(BLOCK_SIZE);
		
		int firstFreeDataBlock = get_avail_blkno();
		inodes[child_ino-1]->direct_ptr[0] = firstFreeDataBlock;
		int newDirOffset = superblock->d_start_blk + firstFreeDataBlock;
		
		newDirEntries[0].valid = 1;
		newDirEntries[0].ino = child_ino;
		strcpy(newDirEntries[0].name,childpath);

		newDirEntries[1].valid = 1;
		newDirEntries[1].ino = child_ino;
		strcpy(newDirEntries[1].name,".");

		newDirEntries[2].ino = addDirectory.ino;
		newDirEntries[2].valid = 1;
		strcpy(newDirEntries[2].name,"..");
		
		int i;
		for(i = 3; i < numberOfEntries; i++){
		
			newDirEntries[i].valid = 0;
		}	

		bio_write(newDirOffset,newDirEntries);	
		writei(child_ino, inodes[child_ino-1]);
		writei(addDirectory.ino, inodes[addDirectory.ino-1]);
		
		free(newDirEntries);
		newDirEntries = NULL;
		return 0;
	}
	return -1;
}

/*
#################################################################################################################################
#																																#
#	static int tfs_rmdir(const char *path)																						#
#																																#
#	Step 1: Use dirname() and basename() to separate parent directory path and target directory name							#
#	Step 2: Call get_node_by_path() to get inode of target directory															#
#	Step 3: Clear data block bitmap of target directory																			#
#	Step 4: Clear inode bitmap and its data block																				#
#	Step 5: Call get_node_by_path() to get inode of parent directory															#
#	Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory								#
#																																#
#################################################################################################################################
*/
static int tfs_rmdir(const char *path) {
	
	char *reg_char_path = strdup(path);
	char *reg_char_path_parent = strdup(path);
	char *parentpath = dirname(reg_char_path_parent);
	

	struct inode removeThisDir;

	int retrieved = get_node_by_path(parentpath,0,&removeThisDir);
	if(retrieved == 0){

		int status = dir_remove(removeThisDir, reg_char_path, (strlen(reg_char_path)+1));	

		if(status != 0){
			fprintf(stderr,"{TFS RMDIR} DIR REMOVE ERROR\n");
			return -1;		
	
		}

		return 0;

	}else{

		return -1;
			
	}
	
	return -1;
}
/*
	static int tfs_releasedir(const char *path, struct fuse_file_info *fi)
*/
static int tfs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}
/*
#################################################################################################################################
#																																#
#	static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)												#
#																																#
#	Step 1: Use dirname() and basename() to separate parent directory path and target file name									#
#	Step 2: Call get_node_by_path() to get inode of parent directory															#
#	Step 3: Call get_avail_ino() to get an available inode number																#
#	Step 4: Call dir_add() to add directory entry of target file to parent directory											#
#	Step 5: Update inode for target file																						#
#	Step 6: Call writei() to write inode to disk																				#
#																																#
#################################################################################################################################
*/

static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	char *reg_char_path = strdup(path);
	char *reg_char_path_dup = strdup(path);
	char *parentpath = dirname(reg_char_path);

	struct inode parent_inode;

	int retrieved = get_node_by_path(parentpath, 1, &parent_inode);

	if(retrieved == 0){
	
		size_t name_len = (size_t)(strlen(reg_char_path_dup) + 1);
		uint16_t f_ino = get_avail_ino();

		dir_add(parent_inode, f_ino, reg_char_path_dup, name_len);

		inodes[f_ino-1]->ino = f_ino;
		inodes[f_ino-1]->valid = 1;
		inodes[f_ino-1]->size = 0;
		inodes[f_ino-1]->type = FILE;
		inodes[f_ino-1]->link = 1;
		(inodes[f_ino-1]->vstat).st_atime = time(NULL); 
		(inodes[f_ino-1]->vstat).st_mtime = time(NULL); 
		inodes[f_ino-1]->vstat.st_mode = mode;
		int i;
		for(i = 0; i < DIRECT_PTRS;i++){

			inodes[f_ino-1]->direct_ptr[i] = -1;
			if(i < INDIRECT_PTRS){
			
				inodes[f_ino-1]->indirect_ptr[i] = -1;

			}

		}
		int numberOfEntries = (int)(BLOCK_SIZE/(sizeof(struct dirent)));
		struct dirent *newDirEntries = (struct dirent *)malloc(BLOCK_SIZE);
	
		for(i = 0; i < numberOfEntries; i++){
		
			newDirEntries[i].valid = 0;
			
		}
	
		int firstFreeDataBlock = get_avail_blkno();
		int newDirOffset = superblock->d_start_blk + firstFreeDataBlock;

		inodes[f_ino-1]->direct_ptr[0] = firstFreeDataBlock;
		
		bio_write(newDirOffset,newDirEntries);
		writei(f_ino,inodes[f_ino-1]);

		free(newDirEntries);
		newDirEntries = NULL;

		return 0;
		
	}
	return -1;
}
/*
#################################################################################################################################
#																																#
#	static int tfs_open(const char *path, struct fuse_file_info *fi)															#
#																																#
#	Step 1: Call get_node_by_path() to get inode from path																		#
#	Step 2: If not find, return -1																								#
#																																#
#################################################################################################################################
*/
static int tfs_open(const char *path, struct fuse_file_info *fi) {

	struct inode *inode = (struct inode *)malloc(sizeof(struct inode));
	int status = get_node_by_path(path,inodes[0]->ino,inode);
	if(status == 0){

		free(inode);
		return 0;

	}else{

		return -1;

	}
}


/*
#################################################################################################################################
#																																#
#	static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi)					#
#																																#
#	Step 1: You could call get_node_by_path() to get inode from path															#
#	Step 2: Based on size and offset, read its data blocks from disk															#
#	Step 3: copy the correct amount of data from offset to buffer																#
#																																#
#	Note: this function should return the amount of bytes you copied to buffer													#
#																																#
#################################################################################################################################
*/
static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	struct inode file_ino;
	int retrieved = get_node_by_path(path, 1, &file_ino);
	int offsetInBlock = offset % BLOCK_SIZE;
	
	int j=0;
	int indirectMap[BLOCK_SIZE];
	
	if(retrieved == 0){
		int i = 0;
		int usingDirectPtr = 1;
		int startingBlock = (int)(offset/BLOCK_SIZE);
		i = startingBlock;
		if(startingBlock >= DIRECT_PTRS){	
	
			usingDirectPtr = 0;
			i = ((int)(offset / MAX_FILE_SIZE_INDIRECT_BLK));
			j = (((int)(offset / BLOCK_SIZE)) - (DIRECT_PTRS + 1)) % MAX_INDIRECT_MAPPINGS;		
			
			int indirectMapping_blkno_offset = superblock->d_start_blk + file_ino.indirect_ptr[i];
			
			bio_read(indirectMapping_blkno_offset,indirectMap);	
		}

		size_t bytesRead = 0;
		size_t bytes = 0;
	
		while(bytesRead < size){
			
			if(usingDirectPtr == 1){

				int targetBlock = superblock->d_start_blk + file_ino.direct_ptr[i];
				
				if(bytesRead == 0){
				
					char dataBuffer[BLOCK_SIZE - offsetInBlock + 1];
					bio_read(targetBlock,dataBuffer);
					memcpy(buffer, &dataBuffer[offsetInBlock],((size_t)(BLOCK_SIZE - offsetInBlock)));
					bytes = BLOCK_SIZE - offsetInBlock;

				}else{
			
					char dataBuffer[(BLOCK_SIZE - (bytesRead % BLOCK_SIZE)) + 1];
					bio_read(targetBlock,dataBuffer);				
					memcpy(&buffer[bytesRead], dataBuffer, ((size_t)(BLOCK_SIZE - (bytesRead % BLOCK_SIZE))));
					bytes = BLOCK_SIZE - (bytesRead % BLOCK_SIZE);
				}

			}else{
								
				int indirectMapping_blkno_offset = superblock->d_start_blk + file_ino.indirect_ptr[i];
				bio_read(indirectMapping_blkno_offset,indirectMap);
	
				int targetBlock = superblock->d_start_blk + indirectMap[j];

				if(bytesRead == 0){
					
					char dataBuffer[BLOCK_SIZE - offsetInBlock + 1];
					bio_read(targetBlock,dataBuffer);
					memcpy(buffer, &dataBuffer[offsetInBlock],((size_t)(BLOCK_SIZE - offsetInBlock)));
					bytes = BLOCK_SIZE - offsetInBlock;

				}else{
				
					char dataBuffer[(BLOCK_SIZE - (bytesRead % BLOCK_SIZE)) + 1];;
					bio_read(targetBlock,dataBuffer);				
					memcpy(&buffer[bytesRead], dataBuffer, ((size_t)(BLOCK_SIZE - (bytesRead % BLOCK_SIZE))));
					bytes = BLOCK_SIZE - (bytesRead % BLOCK_SIZE);
				}
				
				j = (((int)((offset+bytesRead) / BLOCK_SIZE)) - (DIRECT_PTRS + 1));
			
				if(j >= MAX_INDIRECT_MAPPINGS){

					i++;
					j = 0;
				
					indirectMapping_blkno_offset = superblock->d_start_blk + file_ino.indirect_ptr[i];
					bio_read(indirectMapping_blkno_offset,indirectMap);
				}
			}	

			bytesRead = bytesRead + bytes;
			bytes = 0;

			if(usingDirectPtr == 1){

				i++;
			}
			
			if(usingDirectPtr == 1 && i == DIRECT_PTRS){

				usingDirectPtr = 0;
				i = 0;
				j = 0;
				offset = 0;
				offsetInBlock = 0;
				int indirectMapping_blkno_offset = superblock->d_start_blk + file_ino.indirect_ptr[i];
				bio_read(indirectMapping_blkno_offset,indirectMap);
			
			}
		} 
	
		return bytesRead;

	}else{

		return 0;

	}
}

/*
#################################################################################################################################
#																																#
#	static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi)			#
#																																#
#	Step 1: You could call get_node_by_path() to get inode from path															#
#	Step 2: Based on size and offset, read its data blocks from disk															#
#	Step 3: Write the correct amount of data from offset to disk																#
#	Step 4: Update the inode info and write it to disk																			#
#																																#
#	Note: this function should return the amount of bytes you write to disk														#
#																																#
#################################################################################################################################
*/
static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	
	struct inode file_ino;
	int retrieved = get_node_by_path(path, 1, &file_ino);
	int offsetInBlock = offset % BLOCK_SIZE;
	size_t bytesWritten = 0;
	int j=0;
	int indirectMap[BLOCK_SIZE];
	if(retrieved == 0){
		int i = 0;
		int usingDirectPtr = 1;
		int startingBlock = (int)(offset/BLOCK_SIZE);
		i = startingBlock;
		if(startingBlock >= DIRECT_PTRS){	
	
			usingDirectPtr = 0;
			i = ((int)(offset / MAX_FILE_SIZE_INDIRECT_BLK));
			j = (((int)(offset / BLOCK_SIZE)) - (DIRECT_PTRS + 1)) % MAX_INDIRECT_MAPPINGS;
			if(file_ino.indirect_ptr[i] != -1){

				int indirectMapping_blkno_offset = superblock->d_start_blk + file_ino.indirect_ptr[i];
				bio_read(indirectMapping_blkno_offset,indirectMap);
					
			}
		}

		size_t bytes = 0;
		
		while(bytesWritten < size){
			
			if(usingDirectPtr == 1){

				int targetBlock = superblock->d_start_blk + file_ino.direct_ptr[i];
				
				if(bytesWritten == 0 ){
				
					char dataBuffer[BLOCK_SIZE - offsetInBlock + 1];
					bio_read(targetBlock,dataBuffer);
					memcpy(&dataBuffer[offsetInBlock], buffer, ((size_t)(BLOCK_SIZE - offsetInBlock)));
					bio_write(targetBlock,dataBuffer);
					bytes = BLOCK_SIZE - offsetInBlock;

				}else{
			
					char dataBuffer[(BLOCK_SIZE - (bytesWritten % BLOCK_SIZE)) + 1];
					bio_read(targetBlock,dataBuffer);				
					memcpy(dataBuffer,&buffer[bytesWritten], ((size_t)((BLOCK_SIZE - (bytesWritten % BLOCK_SIZE)))+1));
					bio_write(targetBlock, dataBuffer);
					bytes = BLOCK_SIZE - (bytesWritten % BLOCK_SIZE);

				}

			}else{
			
				if(file_ino.indirect_ptr[i] == -1){ 
		
					int indirectMapping_blkno = get_avail_blkno();
					j = 0;
					file_ino.indirect_ptr[i] = indirectMapping_blkno;
					indirectMap[j] = get_avail_blkno();
					int k;	
					for(k=1; k< MAX_INDIRECT_MAPPINGS; k++){
						indirectMap[k] = -1;
					}
					int indirectMapping_blkno_offset = superblock->d_start_blk + file_ino.indirect_ptr[i];
					bio_write(indirectMapping_blkno_offset,indirectMap);		
			
				}else{
					
					int indirectMapping_blkno_offset = superblock->d_start_blk + file_ino.indirect_ptr[i];
					bio_read(indirectMapping_blkno_offset,indirectMap);
				
				}
				
				if(indirectMap[j] == -1){

					int indirectMap_dblkno = get_avail_blkno();
					indirectMap[j] = indirectMap_dblkno;
					int indirectMapping_blkno_offset = superblock->d_start_blk + file_ino.indirect_ptr[i];
					bio_write(indirectMapping_blkno_offset,indirectMap);
				}

				int targetBlock = superblock->d_start_blk + indirectMap[j];

				if(bytesWritten == 0 ){
					
					char dataBuffer[BLOCK_SIZE - offsetInBlock + 1];
					bio_read(targetBlock,dataBuffer);
					memcpy(&dataBuffer[offsetInBlock], buffer, ((size_t)(BLOCK_SIZE - offsetInBlock)));
					bio_write(targetBlock,dataBuffer);
					bytes = BLOCK_SIZE - offsetInBlock;

				}else{
				
					char dataBuffer[(BLOCK_SIZE - (bytesWritten % BLOCK_SIZE)) + 1];
					bio_read(targetBlock,dataBuffer);				
					memcpy(dataBuffer,&buffer[bytesWritten], ((size_t)((BLOCK_SIZE - (bytesWritten % BLOCK_SIZE)))+1));
					bio_write(targetBlock, dataBuffer);
					bytes = BLOCK_SIZE - (bytesWritten % BLOCK_SIZE);
				}
				
				j = (((int)((offset+bytesWritten) / BLOCK_SIZE)) - (DIRECT_PTRS + 1));
				
				if(j >= MAX_INDIRECT_MAPPINGS){

					int indirectMapping_blkno_offset = superblock->d_start_blk + file_ino.indirect_ptr[i];
					bio_write(indirectMapping_blkno_offset,indirectMap);
					i++;
					j=0;
					indirectMapping_blkno_offset = superblock->d_start_blk + file_ino.indirect_ptr[i];
					bio_read(indirectMapping_blkno_offset,indirectMap);
				}
			}	

			bytesWritten = bytesWritten + bytes;
			bytes = 0;
			
			if(usingDirectPtr == 1){

				i++;
			}			
			
			if(usingDirectPtr == 1 && i == DIRECT_PTRS){

				usingDirectPtr = 0;
				i = 0;
				offset = 0;
				offsetInBlock = 0;
				if(file_ino.indirect_ptr[i] != -1){

					int indirectMapping_blkno_offset = superblock->d_start_blk + file_ino.indirect_ptr[i];
					bio_read(indirectMapping_blkno_offset,indirectMap);
					
				}
				
			}
			
		}
	

		file_ino.size = file_ino.size + bytesWritten;
		writei(file_ino.ino,&file_ino); 
		
		return bytesWritten;
	}else{
	
		return 0;

	}

}

/*
#################################################################################################################################
#																																#
#	static int tfs_unlink(const char *path)																						#
#																																#
#	Step 1: Use dirname() and basename() to separate parent directory path and target file name									#
#	Step 2: Call get_node_by_path() to get inode of target file																	#
#	Step 3: Clear data block bitmap of target file																				#
#	Step 4: Clear inode bitmap and its data block																				#
#	Step 5: Call get_node_by_path() to get inode of parent directory															#
#	Step 6: Call dir_remove() to remove directory entry of target file in its parent directory									#
#																																#
#################################################################################################################################

*/
static int tfs_unlink(const char *path) {

	char *reg_char_path = strdup(path);
	char *reg_char_path_parent = strdup(path);
	char *parentpath = dirname(reg_char_path_parent);

	struct inode removeThisDir;

	int retrieved = get_node_by_path(parentpath,0,&removeThisDir);
	if(retrieved == 0){

		int status = dir_remove(removeThisDir, reg_char_path, (strlen(reg_char_path)+1));	

		if(status != 0){
	
			return -1;		
	
		}

		return 0;
	}else{

		return -1;
			
	}

	
	return -1;
}

static int tfs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int tfs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations tfs_ope = {
	.init		= tfs_init,
	.destroy	= tfs_destroy,

	.getattr	= tfs_getattr,
	.readdir	= tfs_readdir,
	.opendir	= tfs_opendir,
	.releasedir	= tfs_releasedir,
	.mkdir		= tfs_mkdir,
	.rmdir		= tfs_rmdir,

	.create		= tfs_create,
	.open		= tfs_open,
	.read 		= tfs_read,
	.write		= tfs_write,
	.unlink		= tfs_unlink,

	.truncate   = tfs_truncate,
	.flush      = tfs_flush,
	.utimens    = tfs_utimens,
	.release	= tfs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;
	
	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &tfs_ope, NULL);
	
	return fuse_stat;
}
/*
#################################################################################
#																				#
#																				#
#								HELPER FUNCTIONS								#
#																				#
#																				#
#################################################################################
*/

/*
#################################################################################
#																				#
#							SPLITS PATH NAME BY SLASH							#
#																				#
#################################################################################
*/
char **splitPathNames(char pathname[]){

	
	int i,slash = 0, fileNameLength = strlen(pathname);
	for(i = 0; i <= fileNameLength; i++){

		if(pathname[i] == '/'){

			slash++;

		}	

	}
	
	int indexOfName = 1,start = -1;
	
	 char **splitPathName = (char **)malloc((slash+2) * sizeof(char *));

	if(strcmp(pathname,"/") == 0){

		splitPathName[indexOfName] = (char *)malloc(2 * sizeof(char));
		strcpy(splitPathName[indexOfName],"/");

	}else{

		for(i = 0; i <= fileNameLength; i++){

			if(pathname[i] == '/'){
				
				if(i == 0){
					
					splitPathName[indexOfName] = (char *)malloc(2 * sizeof(char));
					strcpy(splitPathName[indexOfName],"/");			

				}else{

					int subStringLength = i - start - 1;

					splitPathName[indexOfName] = (char *)malloc((subStringLength+1) * sizeof(char));
					memcpy(splitPathName[indexOfName],&pathname[start+1],subStringLength);
					splitPathName[indexOfName][subStringLength] = '\0';
	
				}

				start = i;
				indexOfName++;
			}
			
		}
	}
	splitPathName[0] = malloc(11 * sizeof(char));
	sprintf(splitPathName[0],"%d",indexOfName);
	int subStringLength = fileNameLength-start;
	splitPathName[indexOfName] = (char *)malloc((subStringLength+1) * sizeof(char));
	memcpy(splitPathName[indexOfName],&pathname[start+1],subStringLength);
	splitPathName[indexOfName][subStringLength] = '\0';

	return splitPathName;
}
/*
#################################################################################
#																				#
#							SEARCH BY PATH IN TREE								#
#																				#
#################################################################################
*/

uint16_t searchiFile(struct inode *inode,uint16_t ino, char pathname[]){

	double numberOfEntries = BLOCK_SIZE/(sizeof(struct dirent));	
	struct dirent *subFilesAndDirs = (struct dirent *)malloc(BLOCK_SIZE);

	char **splitPathName = splitPathNames(pathname);
	int length = strtod(splitPathName[0],NULL);
	int indexOfPath = 1;

	int foundInDisk = -1;
	struct inode found;
	readi(1,&found);
	int workingDirOffset = superblock->d_start_blk + found.direct_ptr[0];

	bio_read(workingDirOffset, subFilesAndDirs);
	if(strcmp(splitPathName[length],"/") == 0){

		return 1;

	}
	for(;;){
					
		int i, j;
		for(i = 0; i < DIRECT_PTRS; i++){

			if(indexOfPath == length && foundInDisk == 0){

				i = 0;
				workingDirOffset = superblock->d_start_blk + found.direct_ptr[i];
				bio_read(workingDirOffset, subFilesAndDirs);

			}
			if(found.direct_ptr[i] == -1){
				
				continue;

			}
	
			workingDirOffset = superblock->d_start_blk + found.direct_ptr[i];
			
			bio_read(workingDirOffset, subFilesAndDirs);
			foundInDisk = -1;
			for(j = 0; j < numberOfEntries; j++){
					
				if(subFilesAndDirs[j].valid == 1){
		
					if(strcmp(subFilesAndDirs[j].name,splitPathName[indexOfPath]) == 0){

						readi(subFilesAndDirs[j].ino,&found);
						foundInDisk = 0;
						found.ino = subFilesAndDirs[j].ino;
						
						indexOfPath++;
						break;

					}
				}
			}

			if(foundInDisk == -1){

				continue;

			}
			if(indexOfPath > length){

				free(subFilesAndDirs);
				subFilesAndDirs = NULL;
				return found.ino;

			}
			
		}
		
		if(foundInDisk == -1){
						
			break;

		}
			
	}
	free(subFilesAndDirs);
	subFilesAndDirs = NULL;

	return 0;
}
