

#include <stdio.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h> 
#include <sys/stat.h> 


void *open_and_map_file(char *filename, size_t length, int flags, int prot, int *fdp) {
    int fd;
    void *status;
    struct stat sbuf;
    
    if ((fd = open(filename, O_RDWR|O_CREAT, 0777)) < 0) {
        fprintf(stderr, "could not open file %s: %s\n", filename, strerror(errno));
        exit(-1);
    }
    fstat(fd, &sbuf);
    if ((sbuf.st_mode & S_IFMT) != S_IFREG) {
        perror("bad filetype");
        exit(-1);
    }
    if (ftruncate(fd, length) == -1) {
        perror("ftruncate failed");   
        exit(-1);
    }
    
    status = mmap(0, length, prot, flags, fd, 0); 
    if (status == (void*)-1) {
        perror("mmap failed");
    }
    *fdp = fd;
    return status;
    
}



int main (int argc, const char * argv[]) {
    
    int status;
    void *statusp;

    char *filename = "/tmp/test_mmap";
    int fd1, fd2;
    char *seg1;
    char *seg2;

    int prot = PROT_READ|PROT_WRITE;

    int page_size = getpagesize();
    size_t length = page_size;

    seg1 = open_and_map_file(filename, length, MAP_SHARED, prot, &fd1);
    
    // set original values using the shared mapping.
    seg1[0] = 1;

    close (fd1);
    
    // now open one shared and one private mapping    
    seg1 = open_and_map_file(filename, length, MAP_PRIVATE, prot, &fd1);    
    // printf("fd1 = %d, seg1 = %lx\n", fd1, (unsigned long)seg1);

    seg2 = open_and_map_file(filename, length, MAP_SHARED, prot, &fd2);
    // printf("fd2 = %d, seg2 = %lx\n", fd2, (unsigned long)seg2);
    
    seg2[0] = 2;
    
    if (seg1[0] == 1)
      printf("-DPRIVATE_MAPPING_IS_PRIVATE\n");
        
    close(fd1);
    close(fd2);

    return 0;
}
