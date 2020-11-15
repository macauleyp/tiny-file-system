# A 'Tiny' File System (TFS)  with FUSE Library

This is a User-Level File System library that builds on top of the kernel module FUSE (version 26) to redirect system calls to the User-Level file system. Thus, the program initializes a virtual disk and provides file abstraction by logically fetching disk segments.

# Using the TFS Library

 1. Build Test Infrastructure
	- ```> mkdir /tmp/<username>/``` 
	- ```> mkdir /tmp/<username>/mountdir``` 
 2. Compile and Run TFS
	- ```> make``` 
	- ```> ./tfs -s /tmp/<username>/mountdir``` 
3. Check if TFS is mounted successfully (**optional**)
	- ```> findmnt``` 
		- If mounted successfully, you could see the following:
			- ```/tmp/<username>/mountdir tfs fuse.tfs rw,nosuid,nodev,relatime,...```
4. File system Library Commands
	- Most standard command-line file system commands are supported such as:
	- ```> cd```
	- ```> touch```
	- ```> mkdir```
	- ```> rmdir```
	- ```> ls```
	- Furthermore, the library supports the following functions:

| Function Name | Return Value | Description |
|--------------------------------------------|-------------------------------------------------------------|--------------------------------------------|
| ``` int creat(const char *path, mode_t mode);```| File descriptor | Creates a file |
| ```ssize_t write(int fd, const void *buf, size_t count);``` | Number of bytes written | Writes to the given file descriptor |
|```int open(const char *pathname, int flags);```| 0 on success, or -1 on failure | Open a file for reading |
|```ssize_t read(int fd, void *buf, size_t count);```| Number of Bytes Read | Read from the given file descriptor |
|```int unlink(const char *pathname);```| 0 on success, or -1 on failure | Deletes a name from the filesystem |
|```int mkdir(const char *pathname, mode_t mode);```| 0 on success, or -1 on failure | Creates a directory |
|```DIR *opendir(const char *name);``` | A pointer to the directory stream on success, or ```NULL``` on failure | Opens the given directory the stream |
| ```int rmdir(const char *pathname);``` | 0 on success, or -1 on failure | Deletes a directory which must be empty |

6. Exit and Unmount TFS
- ```> fusermount -u /tmp/<username>/mountdir```

# File System API 
The User-Level file system works in 5 parts which are defined below.

### Block I/O Layer
Read a block at ```block_num``` from the flat file (the virtual disk)  to ```buf```
```C 
int bio_read(const int block_num, void *buf);
```

Write a block at ```block_num``` from ```buf``` to the flat file (the virtual disk) 
```C
int bio_write(const int block_num, void *buf);
```

### Bitmap
Set the i-th bit of bitmap ```b```
```C
set_bitmap(bitmap_t b, int i);
```

Unset the i-th bit of bitmap ```b```
```C 
unset_bitmap(bitmap_t b, int i);
```

Get the value of the i-th bit of bitmap ```b```
```C 
uint8_t get_bitmap(bitmap_t b, int i);
```

Traverses the data block bitmap to find an available inode block, and sets this inode in the bitmap before returning the inode number (or -1 on failure).
```C 
int get_avail_ino();
```

Traverses the data block bitmap to find an available data block, and sets this data bock in the bitmap before returning the data block number (or -1 on failure).
```C 
int get_avail_blkno();
```

### iNode

Reads the given inode number from the respective on-disk inode which is mapped to an inode area of the flat file (the virtual disk) to an in-memory inode (```struct inode```)
```C 
int readi(uint16_t ino, struct inode *inode);
```

Writes the in-memory inode struct to a disk inode into the inode area of the flat file (the virtual disk) using the given inode number as input.
```C
int writei(uint16_t ino, struct inode *inode);
```

### Directory and namei

Using the given inode number along with the current directory, the file name or sub-directory and its length, the function reads all direct entries of the current directory to see if the intended file or sub-directory exists. If it does exist, the parameters are loaded into ```struct direct *dirent```
```C
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent);
```

Creates a new directory from the given inode number, current directory inode, and the name to write a new directory entry to the current directory.
```C 
int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len);
```

Removes a directory from the given current directory inode and the sub-directory to be deleted.
```C
int dir_remove(struct inode dir_inode, const char *fname, size_t name_len);
```

A namei function that follows a pathname till a terminal point is found from the given path and the root inode number of the path. The funvtion calls ```int dir_find()``` to look up each path component before finally reading the inode of the terminal point into ```struct inode *inode```.
```C
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode);
```

### FUSE-based File System Handlers

Initialization function for the 'Tiny' File System which will open a flat file (the virtual disk) and reads a super block into memory. If the flat file does not exist (the virtual disk is not formatted), then it will call ```tfs_mkfs()``` to format the virtual disk which partitions the flat file into a superblock, inode, bitmap, and data block region. Furthermore, memory is allocation for the file system data structures.  
```C 
static void *tfs_init(struct fuse_conn_info *conn);
```

This function is called when the TFS is unmounted where in-memory file system data structures are de-allocated before closing the flat file (the virtual disk).
```C 
static void tfs_destroy(void *userdata);
```

The following function is called when accessing a file or directory from the given path to provide stats such as inode permission, size, number of references, etc. If the path is valid, information is loaded into ```struct stat *stbuf```.
```C 
static int tfs_getattr(const char *path, struct stat *stbuf);
```

This function is called when accessing a directory (e.g. cd command) from the given path of a directory. The function returns 0 is the path is valid, or -1 if it is invalid.
```C 
static int tfs_opendir(const char *path, struct fuse_file_info *fi);
```

This function is called when reading a directory (e.g. ls command) from the given path of a directory. The function returns 0 is the path is valid, or -1 if it is invalid.
```C 
static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
```

The following function is called when creating a directory (e.g. mkdir command) from the given path and mode.
```C 
static int tfs_mkdir(const char *path, mode_t mode);
```

The following function is called when removing a directory (e.g. rmdir command) from the given path and mode.
```C 
static int tfs_rmdir(const char *path);
```

This function is called when creating a file (e.g. touch command) given the path and mode. If the base name is a valid file name, then a new directory entry is added is added, an inode is allocated, and the bitmaps are updated.
```C 
static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
```

This function is called when accessing a file from the given path. If the path is valid, then 0 is returned—otherwise, -1 is returned.
```C 
static int tfs_open(const char *path, struct fuse_file_info *fi);
```

The following function is the read call handler. Using the given path, size, and offset, the corresponding inode and data blocks are retrieved and loaded into the memory area pointed to by the ```char *buffer``` using direct and indirect file system pointers.
```C
static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi);
```

The following function is the write call handler. Using the given path, size, and offset, the inode and data blocks are retrieved and loaded into the memory area pointed to by the ```char *buffer``` and the data is written to the corresponding data blocks using direct and indirect file system pointers.
```C 
static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi);
```
This function is called when removing a file (e.g. rm command) using the given path as input. The function return -1 if the file does not exist.
```C 
static int tfs_unlink(const char *path);
```
