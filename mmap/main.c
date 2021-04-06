#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define page_size sysconf(_SC_PAGESIZE)

/* Reference Counter Structure */
struct ref {
    void (*free)(const struct ref *);
    int count;
};

static inline void ref_inc (const struct ref *ref) {
    ((struct ref *)ref)->count++;
}
static inline void ref_dec (const struct ref *ref) {
    if (--((struct ref *)ref)->count == 0)
        ref->free(ref);
}
static inline void ref_inc_ts (const struct ref *ref) {
    __sync_add_and_fetch((int *)&ref->count, 1);
}
static inline void ref_dec_ts (const struct ref *ref) {
    if (__sync_sub_and_fetch((int *)&ref->count, 1) == 0)
        ref->free(ref);
}

int main () {
    char *src, *dst;
    int fin, fout;
    size_t size;
    
    fin = open ("foo", O_RDWR);
    size = lseek(fin, 0, SEEK_END);

    src = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fin, 0);
    printf("src: %s\n", src);

    src[0] = '0';
    if (msync(src, size, MS_ASYNC) == -1)
        perror ("msync");

    exit(0);
}
