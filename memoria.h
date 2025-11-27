// Pablo Araújo Rodríguez   pablo.araujo@udc.es
// Uriel Liñares Vaamonde   uriel.linaresv@udc.es

#ifndef MEMORIA_H
#define MEMORIA_H

#include <stddef.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/wait.h>

#include "p3.h"
#include "lista.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifndef TAMANO
#define TAMANO 1024
#endif

typedef struct {
    void  *addr;
    size_t size;
} MallocBlock;

typedef struct {
    key_t   key;
    size_t  size;
    void   *addr;
    int     shmid;
} SharedBlock;

typedef struct {
    char    path[PATH_MAX];
    size_t  size;
    void   *addr;
    int     fd;
    int     protection;
    int     flags;
} MmapBlock;

void *shm_get(key_t clave, size_t tam);
void *map_file(char *fichero, int protection);
void fill_memory(void *p, size_t cont, unsigned char byte);

int cmd_malloc(int argc, char *argv[]);
int cmd_free(int argc, char *argv[]);
int cmd_memfill(int argc, char *argv[]);
int cmd_memdump(int argc, char *argv[]);
int cmd_mmap(int argc, char *argv[]);
int cmd_shared(int argc, char *argv[]);
int cmd_recurse(int argc, char *argv[]);
int cmd_read(int argc, char *argv[]);
int cmd_readfile(int argc, char *argv[]);
int cmd_write(int argc, char *argv[]);
int cmd_writefile(int argc, char *argv[]);
int cmd_mem(int argc, char *argv[]);
void mem_cleanup(void);

#endif
