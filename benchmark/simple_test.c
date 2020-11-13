#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

/* You need to change this macro to your TFS mount point*/
#define TESTDIR "/tmp/mountdir"

#define N_FILES 100
#define BLOCKSIZE 4096
#define FSPATHLEN 256
#define ITERS 100
#define FILEPERM 0666
#define DIRPERM 0755

char buf[BLOCKSIZE];

int main(int argc, char **argv) {

	int i, fd = 0, ret = 0;
	struct stat st;

	if ((fd = creat(TESTDIR "/file", FILEPERM)) < 0) {
		perror("creat");
		printf("TEST 1: File create failure \n");
		exit(1);
	}
	printf("TEST 1: File create Success \n");


	/* Perform sequential writes */
	for (i = 0; i < ITERS; i++) {
		//memset with some random data
		memset(buf, 0x61 + i, BLOCKSIZE);

		if (write(fd, buf, BLOCKSIZE) != BLOCKSIZE) {
			printf("TEST 2: File write failure \n");
			exit(1);
		}
	}
	
	fstat(fd, &st);
	if (st.st_size != ITERS*BLOCKSIZE) {
		printf("TEST 2: File write failure \n");
		exit(1);
	}
	printf("TEST 2: File write Success \n");


	/*Close operation*/	
	if (close(fd) < 0) {
		printf("TEST 3: File close failure \n");
	}
	printf("TEST 3: File close Success \n");


	/* Open for reading */
	if ((fd = open(TESTDIR "/file", FILEPERM)) < 0) {
		perror("open");
		exit(1);
	}


	/* Perform sequential reading */
	for (i = 0; i < ITERS; i++) {
		//clear buffer
		memset(buf, 0, BLOCKSIZE);

		if (read(fd, buf, BLOCKSIZE) != BLOCKSIZE) {
			printf("TEST 4: File read failure \n");
			exit(1);
		}
		//printf("buf %s \n", buf);
	}
        
	if (pread(fd, buf, BLOCKSIZE, 2*BLOCKSIZE) != BLOCKSIZE) {
		perror("pread");
		printf("TEST 4: File read failure \n");
		exit(1);
	}
    
	printf("TEST 4: File read Success \n");
	close(fd);


	/* Unlink the file */
	if ((ret = unlink(TESTDIR "/file")) < 0) {
		perror("unlink");
		printf("TEST 5: File unlink failure \n");
		exit(1);
	}
	printf("TEST 5: File unlink success \n");


	/* Directory creation test */
	if ((ret = mkdir(TESTDIR "/files", DIRPERM)) < 0) {
		perror("mkdir");
		printf("TEST 6: failure. Check if dir %s already exists, and "
			"if it exists, manually remove and re-run \n", TESTDIR "/files");
		exit(1);
	}
	printf("TEST 6: Directory create success \n");

	
	/* Sub-directory creation test */
	for (i = 0; i < N_FILES; ++i) {
		char subdir_path[FSPATHLEN];
		memset(subdir_path, 0, FSPATHLEN);

		sprintf(subdir_path, "%s%d", TESTDIR "/files/dir", i);
		if ((ret = mkdir(subdir_path, DIRPERM)) < 0) {
			perror("mkdir");
			printf("TEST 7: Sub-directory create failure \n");
			exit(1);
		}
	}
	
	for (i = 0; i < N_FILES; ++i) {
		DIR *dir;
		char subdir_path[FSPATHLEN];
		memset(subdir_path, 0, FSPATHLEN);

		sprintf(subdir_path, "%s%d", TESTDIR "/files/dir", i);
		if ((dir = opendir(subdir_path)) == NULL) {
			perror("opendir");
			printf("TEST 7: Sub-directory create failure \n");
			exit(1);
		}
	}
	printf("TEST 7: Sub-directory create success \n");

	printf("Benchmark completed \n");
	return 0;
}
