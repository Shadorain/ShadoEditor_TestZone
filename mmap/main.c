#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 

#define page_size sysconf(_SC_PAGESIZE)

int main () {
    char *src, *dst;
    int fin, fout;
    size_t size;
    
    fin = open ("foo", O_RDWR);
    size = lseek(fin, 0, SEEK_END);

    src = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fin, 0);
    printf("src: %s\n", src);

    /* src = "hi"; */
    src[0] = 'A';
    if (msync(src, size, MS_ASYNC) == -1)
        perror ("msync");

    /* msync(src, strlen(src), MS_SYNC); */

/*     fout = open("bar", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR); */
/*     if (ftruncate(fout, size) == -1) { perror("ftruncate"); exit(3); } */

/*     dst = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fout, 0); */
/*     printf("precpy dst: %s\n", dst); */

/*     memcpy(dst, src, size); */

/*     printf("postcpy dst: %s\n", dst); */

/*     printf("%lu", page_size); */

    exit(0);
}
