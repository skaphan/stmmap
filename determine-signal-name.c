#include <stdio.h>
#include <signal.h>
#include <sys/mman.h> 
#include <fcntl.h>
#include <sys/stat.h> 
#include <sys/errno.h>
#include <stdlib.h>
#include <string.h>


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
	exit(-1);
    }
    *fdp = fd;
    return status;
    
}


char *seg1;

static void sigbus_handler(int sig, siginfo_t *si, void *foo) {
    printf("-DPAGE_ACCESS_SIGNAL=SIGBUS\n");
    mprotect(seg1, 0x1000, PROT_READ|PROT_WRITE);
    
}

static void sigsegv_handler(int sig, siginfo_t *si, void *foo) {
    printf("-DPAGE_ACCESS_SIGNAL=SIGSEGV\n");
    mprotect(seg1, 0x1000, PROT_READ|PROT_WRITE);
}


    
    
int main (int argc, const char * argv[]) {
    
    int status;

    char *filename = "/tmp/test_mmap";
    int fd1;

    int prot = PROT_READ|PROT_WRITE;

    int page_size = getpagesize();
    size_t length = page_size;
    
    struct sigaction sa1;
    
    struct sigaction sa2;

    sa1.sa_flags = SA_SIGINFO;
    sigemptyset(&sa1.sa_mask);
    sa1.sa_sigaction = sigbus_handler;
    
    if ((status = sigaction(SIGBUS, &sa1, NULL)) != 0) {     
        perror("sigaction failed");
    }
    
    sa2.sa_flags = SA_SIGINFO;
    sigemptyset(&sa2.sa_mask);
    sa2.sa_sigaction = sigsegv_handler;
    
    if ((status = sigaction(SIGSEGV, &sa2, NULL)) != 0) {     
        perror("sigaction failed");
	exit(-1);
    }
    
    
    seg1 = open_and_map_file(filename, length, MAP_SHARED, PROT_NONE, &fd1);

    status = *seg1;
    
    close(fd1);

    return 0;
}
