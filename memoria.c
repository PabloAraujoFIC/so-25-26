// Pablo Araújo Rodríguez   pablo.araujo@udc.es
// Uriel Liñares Vaamonde   uriel.linaresv@udc.es

#define _POSIX_C_SOURCE 200809L

#include "memoria.h"

#include "lista.h"

#include <ctype.h>
#include <stdint.h>
#include <sys/wait.h>

int ext_uninit_a;
int ext_uninit_b;
int ext_uninit_c;
int ext_init_a = 101;
int ext_init_b = 202;
int ext_init_c = 303;

static int static_uninit_a;
static int static_uninit_b;
static int static_uninit_c;
static int static_init_a = -1;
static int static_init_b = -2;
static int static_init_c = -3;

static List *get_malloc_list(void) {
    static List list = { .head = NULL };
    return &list;
}

static List *get_shared_list(void) {
    static List list = { .head = NULL };
    return &list;
}

static List *get_mmap_list(void) {
    static List list = { .head = NULL };
    return &list;
}

static MmapBlock *find_mmap(void *addr);
static int perm_to_prot(const char *perm, int *protection);
static int unmap_mmap_by_path(const char *path);
static int text_to_key(const char *str, key_t *key);
static int detach_shared_by_key(key_t key);
static int delete_system_shared(key_t key);
static int free_malloc_by_addr(void *addr);
static int detach_shared_by_addr(void *addr);
static int unmap_mmap_by_addr(void *addr);
static int run_process_map(void);
static int ensure_valid_region(void *addr, size_t len);
static int read_ulong(const char *str, int base, unsigned long *value);
static int read_size(const char *str, size_t *value);
static int read_int(const char *str, int min, int max, int *value);
static int read_fd(const char *str, int *fd);
static int read_byte(const char *str, unsigned char *value);
static bool range_within_block(const void *addr, size_t len,
                                const void *block_addr, size_t block_size);
static bool is_region_from_malloc(const void *addr, size_t len);
static bool is_region_from_shared(const void *addr, size_t len);
static bool is_region_from_mmap(const void *addr, size_t len);
static void format_byte_repr(unsigned char byte, char *out, size_t out_len);
static void recurse_steps(int n);
static void show_shared(void);
static void show_mmap(void);
static void show_malloc(void);
static void print_function_addresses(void);
static void print_variable_addresses(void);
static void print_block_lists(void);
static void destroy_malloc_block(void *data);
static void destroy_shared_block(void *data);
static void destroy_mmap_block(void *data);

static void *parse_pointer(const char *s) {
    if (!s) { errno = EINVAL; return NULL; }
    void *p = NULL;
    if (sscanf(s, "%p", &p) != 1) { errno = EINVAL; return NULL; }
    if (!p) { errno = EFAULT; }
    return p;
}

static int perm_to_prot(const char *perm, int *protection) {
    if (!perm || !protection) return -1;
    size_t n = strlen(perm);
    if (n == 0 || n > 3) return -1;

    int prot = 0;
    for (size_t i = 0; i < n; ++i) {
        switch (tolower((unsigned char)perm[i])) {
            case 'r': prot |= PROT_READ;  break;
            case 'w': prot |= PROT_WRITE; break;
            case 'x': prot |= PROT_EXEC;  break;
            case '-': /* placeholder, ignorar */ break;
            default:  return -1;
        }
    }
    *protection = prot;
    return 0;
}

static int text_to_key(const char *str, key_t *key) {
    if (!str || !key) return -1;
    unsigned long val = 0;
    if (read_ulong(str, 10, &val) != 0) return -1;
    *key = (key_t)val;
    return 0;
}

static int read_ulong(const char *str, int base, unsigned long *value) {
    if (!str || !value) { errno = EINVAL; return -1; }
    errno = 0;
    char *endp = NULL;
    unsigned long tmp = strtoul(str, &endp, base);
    if (errno != 0) return -1;
    if (!endp || *endp != '\0') { errno = EINVAL; return -1; }
    *value = tmp;
    return 0;
}

static int read_size(const char *str, size_t *value) {
    unsigned long tmp = 0;
    if (read_ulong(str, 10, &tmp) != 0) return -1;
    if (tmp > SIZE_MAX) { errno = ERANGE; return -1; }
    *value = (size_t)tmp;
    return 0;
}

static int read_int(const char *str, int min, int max, int *value) {
    if (!str || !value) { errno = EINVAL; return -1; }
    errno = 0;
    char *endp = NULL;
    long tmp = strtol(str, &endp, 10);
    if (errno != 0) return -1;
    if (!endp || *endp != '\0') { errno = EINVAL; return -1; }
    if (tmp < min || tmp > max) { errno = ERANGE; return -1; }
    *value = (int)tmp;
    return 0;
}

static int read_fd(const char *str, int *fd) {
    return read_int(str, 0, INT_MAX, fd);
}

static int read_byte(const char *str, unsigned char *value) {
    if (!str || !value) { errno = EINVAL; return -1; }
    if (str[0] != '\0' && str[1] == '\0') {
        *value = (unsigned char)str[0];
        return 0;
    }
    unsigned long tmp = 0;
    if (read_ulong(str, 0, &tmp) != 0) return -1;
    if (tmp > 0xFFUL) { errno = ERANGE; return -1; }
    *value = (unsigned char)tmp;
    return 0;
}

static void format_byte_repr(unsigned char byte, char *out, size_t out_len) {
    if (!out || out_len < 2) return;
    int written = 0;
    if (byte == '\n') written = snprintf(out, out_len, "\\n");
    else if (byte == '\t') written = snprintf(out, out_len, "\\t");
    else if (byte == '\r') written = snprintf(out, out_len, "\\r");
    else if (isprint(byte))
        written = snprintf(out, out_len, "%c", (unsigned char)byte);
    else {
        out[0] = ' ';
        out[1] = '\0';
        return;
    }
    if (written < 0 || (size_t)written >= out_len) out[out_len - 1] = '\0';
}

static bool range_within_block(const void *addr, size_t len,
        const void *block_addr, size_t block_size) {
    if (!addr || !block_addr) return false;
    uintptr_t start = (uintptr_t)block_addr;
    uintptr_t ptr   = (uintptr_t)addr;
    uintptr_t size  = (uintptr_t)block_size;
    uintptr_t end   = start + size;
    if (end < start) return false; /* overflow check */
    if (len == 0) return ptr >= start && ptr <= end;
    if (ptr < start || ptr >= end) return false;
    uintptr_t len_u = (uintptr_t)len;
    if (len_u > end - ptr) return false;
    return true;
}

static bool is_region_from_malloc(const void *addr, size_t len) {
    List *list = get_malloc_list();
    if (!list || isEmptyList(list)) return false;
    for (Node *node = list->head; node; node = node->next) {
        const MallocBlock *block = (const MallocBlock *)node->data;
        if (!block) continue;
        if (range_within_block(addr, len, block->addr, block->size))
            return true;
    }
    return false;
}

static bool is_region_from_shared(const void *addr, size_t len) {
    List *list = get_shared_list();
    if (!list || isEmptyList(list)) return false;
    for (Node *node = list->head; node; node = node->next) {
        const SharedBlock *block = (const SharedBlock *)node->data;
        if (!block) continue;
        if (range_within_block(addr, len, block->addr, block->size))
            return true;
    }
    return false;
}

static bool is_region_from_mmap(const void *addr, size_t len) {
    List *list = get_mmap_list();
    if (!list || isEmptyList(list)) return false;
    for (Node *node = list->head; node; node = node->next) {
        const MmapBlock *block = (const MmapBlock *)node->data;
        if (!block) continue;
        if (range_within_block(addr, len, block->addr, block->size))
            return true;
    }
    return false;
}

static int ensure_valid_region(void *addr, size_t len) {
    if (!addr && len == 0) return 0;
    if (is_region_from_malloc(addr, len)) return 0;
    if (is_region_from_shared(addr, len)) return 0;
    if (is_region_from_mmap(addr, len)) return 0;
    errno = EFAULT;
    return -1;
}

static void print_function_addresses(void) {
    puts("Program functions:");
    printf("  cmd_malloc : %p\n", (void *)(uintptr_t)&cmd_malloc);
    printf("  cmd_memdump: %p\n", (void *)(uintptr_t)&cmd_memdump);
    printf("  cmd_memfill: %p\n", (void *)(uintptr_t)&cmd_memfill);

    puts("Library functions:");
    printf("  printf : %p\n", (void *)(uintptr_t)&printf);
    printf("  malloc : %p\n", (void *)(uintptr_t)&malloc);
    printf("  strerror: %p\n", (void *)(uintptr_t)&strerror);
}

static void print_variable_addresses(void) {
    int auto_a = 0;
    int auto_b = 0;
    int auto_c = 0;

    puts("External variables (uninitialised):");
    printf("  ext_uninit_a: %p\n", (void *)(uintptr_t)&ext_uninit_a);
    printf("  ext_uninit_b: %p\n", (void *)(uintptr_t)&ext_uninit_b);
    printf("  ext_uninit_c: %p\n", (void *)(uintptr_t)&ext_uninit_c);

    puts("External variables (initialised):");
    printf("  ext_init_a: %p\n", (void *)(uintptr_t)&ext_init_a);
    printf("  ext_init_b: %p\n", (void *)(uintptr_t)&ext_init_b);
    printf("  ext_init_c: %p\n", (void *)(uintptr_t)&ext_init_c);

    puts("Static variables (uninitialised):");
    printf("  static_uninit_a: %p\n", (void *)(uintptr_t)&static_uninit_a);
    printf("  static_uninit_b: %p\n", (void *)(uintptr_t)&static_uninit_b);
    printf("  static_uninit_c: %p\n", (void *)(uintptr_t)&static_uninit_c);

    puts("Static variables (initialised):");
    printf("  static_init_a: %p\n", (void *)(uintptr_t)&static_init_a);
    printf("  static_init_b: %p\n", (void *)(uintptr_t)&static_init_b);
    printf("  static_init_c: %p\n", (void *)(uintptr_t)&static_init_c);

    puts("Automatic variables:");
    printf("  auto_a: %p\n", (void *)(uintptr_t)&auto_a);
    printf("  auto_b: %p\n", (void *)(uintptr_t)&auto_b);
    printf("  auto_c: %p\n", (void *)(uintptr_t)&auto_c);
}

static void print_block_lists(void) {
    puts("Malloc blocks:");
    show_malloc();
    puts("Shared memory blocks:");
    show_shared();
    puts("Memory-mapped files:");
    show_mmap();
}

static int run_process_map(void) {
    pid_t pid = getpid();
#ifdef __APPLE__
    const char *tool = "vmmap";
#else
    const char *tool = "pmap";
#endif
    char command[64];
    int written = snprintf(command, sizeof command, "%s %d", tool, (int)pid);
    if (written < 0 || (size_t)written >= sizeof command) {
        errno = ENOSPC;
        perror(tool);
        return 1;
    }
    FILE *fp = popen(command, "r");
    if (!fp) { perror(tool); return 1; }
    char line[512];
    while (fgets(line, sizeof line, fp) != NULL) { fputs(line, stdout); }
    int status = pclose(fp);
    if (status == -1) { perror("pclose"); return 1; }
#ifdef __APPLE__
    if (status != 0) {
        fprintf(stderr, "%s command failed (exit status %d)\n", tool, status);
        return 1;
    }
#else
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) != 0) {
            fprintf(stderr, "%s command failed (exit status %d)\n",
                    tool, WEXITSTATUS(status));
            return 1;
        }
    } else {
        fprintf(stderr, "%s command terminated abnormally\n", tool);
        return 1;
    }
#endif
    return 0;
}

static SharedBlock *find_shared(key_t key) {
    List *list = get_shared_list();
    for (Node *node = list->head; node; node = node->next) {
        SharedBlock *block = (SharedBlock *)node->data;
        if (block && block->key == key) return block;
    }
    return NULL;
}

static void show_shared(void) {
    List *list = get_shared_list();
    if (isEmptyList(list)) {
        puts("Shared memory list is empty");
        return;
    }
    for (Node *node = list->head; node; node = node->next) {
        const SharedBlock *block = (const SharedBlock *)node->data;
        if (!block) continue;
        printf("Key %lu -> %p (%zu bytes, shmid=%d)\n",
                (unsigned long)block->key, block->addr,
                block->size, block->shmid);
    }
}

static void add_shared(key_t key, size_t size, void *addr, int shmid) {
    SharedBlock *block = find_shared(key);
    if (block) {
        block->size  = size;
        block->addr  = addr;
        block->shmid = shmid;
        return;
    }
    block = (SharedBlock *)malloc(sizeof *block);
    if (!block) { perror("malloc"); return; }
    block->key   = key;
    block->size  = size;
    block->addr  = addr;
    block->shmid = shmid;
    if (insertItem(get_shared_list(), block) != 0) {
        fprintf(stderr, "Unable to register shared block\n");
        free(block);
    }
}

static MmapBlock *find_mmap(void *addr) {
    List *list = get_mmap_list();
    for (Node *node = list->head; node; node = node->next) {
        MmapBlock *block = (MmapBlock *)node->data;
        if (block && block->addr == addr) return block;
    }
    return NULL;
}

static void show_mmap(void) {
    List *list = get_mmap_list();
    if (isEmptyList(list)) {
        puts("Mapped file list is empty");
        return;
    }
    for (Node *node = list->head; node; node = node->next) {
        const MmapBlock *block = (const MmapBlock *)node->data;
        if (!block) continue;
        printf("%s -> %p (%zu bytes, prot=%d, flags=%d, fd=%d)\n",
                block->path, block->addr, block->size,
                block->protection, block->flags, block->fd);
    }
}

static void add_mmap(const char *path, size_t size, void *addr,
                                int fd, int protection, int flags) {
    MmapBlock *block = find_mmap(addr);
    if (block) {
        block->size       = size;
        block->fd         = fd;
        block->protection = protection;
        block->flags      = flags;
        strncpy(block->path, path, sizeof block->path - 1);
        block->path[sizeof block->path - 1] = '\0';
        return;
    }
    block = (MmapBlock *)malloc(sizeof *block);
    if (!block) { perror("malloc"); return; }
    block->size       = size;
    block->addr       = addr;
    block->fd         = fd;
    block->protection = protection;
    block->flags      = flags;
    strncpy(block->path, path, sizeof block->path - 1);
    block->path[sizeof block->path - 1] = '\0';
    if (insertItem(get_mmap_list(), block) != 0) {
        fprintf(stderr, "Unable to record file mapping\n");
        free(block);
    }
}

static int remove_mmap_entry(MmapBlock *block, Node *node, List *list) {
    if (!block || !node || !list) { errno = EINVAL; return -1; }
    if (munmap(block->addr, block->size) == -1) { perror("munmap"); return -1; }
    if (node->prev) node->prev->next = node->next;
    else list->head = node->next;
    if (node->next) node->next->prev = node->prev;
    void *addr = block->addr;
    char path[PATH_MAX];
    strncpy(path, block->path, sizeof path - 1);
    path[sizeof path - 1] = '\0';

    free(block);
    free(node);
    printf("Unmapped file %s from %p\n", path, addr);
    return 0;
}

static int unmap_mmap_by_path(const char *path) {
    if (!path) { errno = EINVAL; return -1; }
    List *list = get_mmap_list();
    if (!list || isEmptyList(list)) { errno = ENOENT; return -1; }
    for (Node *node = list->head; node; node = node->next) {
        MmapBlock *block = (MmapBlock *)node->data;
        if (!block || strcmp(block->path, path) != 0) continue;
        remove_mmap_entry(block, node, list);
        return 0;
    }
    errno = ENOENT;
    return -1;
}

static int unmap_mmap_by_addr(void *addr) {
    if (!addr) { errno = EINVAL; return -1; }
    List *list = get_mmap_list();
    if (!list || isEmptyList(list)) { errno = ENOENT; return -1; }
    for (Node *node = list->head; node; node = node->next) {
        MmapBlock *block = (MmapBlock *)node->data;
        if (!block || block->addr != addr) continue;
        remove_mmap_entry(block, node, list);
        return 0;
    }
    errno = ENOENT;
    return -1;
}

static void detach_shared_entry(SharedBlock *block, Node *node, List *list){
    if (shmdt(block->addr) == -1) perror("shmdt");
    if (node->prev) node->prev->next = node->next;
    else list->head = node->next;
    if (node->next) node->next->prev = node->prev;
    printf("Detached shared memory key %lu at %p\n",
            (unsigned long)block->key, block->addr);
    free(block);
    free(node);
}

static int detach_shared_by_key(key_t key) {
    List *list = get_shared_list();
    if (!list || isEmptyList(list)) { errno = ENOENT; return -1; }

    for (Node *node = list->head; node; node = node->next) {
        SharedBlock *block = (SharedBlock *)node->data;
        if (!block || block->key != key) continue;
        detach_shared_entry(block, node, list);
        return 0;
    }

    errno = ENOENT;
    return -1;
}

static int detach_shared_by_addr(void *addr) {
    if (!addr) { errno = EINVAL; return -1; }

    List *list = get_shared_list();
    if (!list || isEmptyList(list)) { errno = ENOENT; return -1; }

    for (Node *node = list->head; node; node = node->next) {
        SharedBlock *block = (SharedBlock *)node->data;
        if (!block || block->addr != addr) continue;
        detach_shared_entry(block, node, list);
        return 0;
    }
    errno = ENOENT;
    return -1;
}

static int delete_system_shared(key_t key) {
    int id = shmget(key, 0, 0666);
    if (id == -1) return -1;
    if (shmctl(id, IPC_RMID, NULL) == -1) return -1;
    SharedBlock *block = find_shared(key);
    if (block) block->shmid = -1;
    return 0;
}

void fill_memory(void *p, size_t cont, unsigned char byte) {
    if (!p || cont == 0) { return; }
    unsigned char *arr = (unsigned char *)p;
    for (size_t i = 0; i < cont; ++i) arr[i] = byte;
}

void *shm_get(key_t clave, size_t tam) {
    void *p;
    int aux, id;
    int flags = 0777;
    struct shmid_ds s;

    if (tam) flags |= IPC_CREAT | IPC_EXCL;
    if (clave == IPC_PRIVATE) { errno = EINVAL; return NULL; }
    id = shmget(clave, tam, flags);
    if (id == -1) return NULL;
    p = shmat(id, NULL, 0);
    if (p == (void *)-1) {
        aux = errno;
        if (tam) shmctl(id, IPC_RMID, NULL);
        errno = aux;
        return NULL;
    }
    if (shmctl(id, IPC_STAT, &s) == -1) {
        aux = errno;
        shmdt(p);
        if (tam) shmctl(id, IPC_RMID, NULL);
        errno = aux;
        return NULL;
    }
    add_shared(clave, (size_t)s.shm_segsz, p, id);
    return p;
}

void *map_file(char *fichero, int protection) {
    int df;
    int map = MAP_PRIVATE;
    int modo = O_RDONLY;
    struct stat s;
    if (!fichero) { errno = EINVAL; return NULL; }
    if (protection & PROT_WRITE) modo = O_RDWR;
    if (stat(fichero, &s) == -1) return NULL;
    df = open(fichero, modo);
    if (df == -1) return NULL;
    void *p = mmap(NULL, (size_t)s.st_size, protection, map, df, 0);
    
    if (p == MAP_FAILED) {
        int aux = errno;
        close(df);
        errno = aux;
        return NULL;
    }
    add_mmap(fichero, (size_t)s.st_size, p, -1, protection, map);
    close(df);
    return p;
}

int cmd_mmap(int argc, char *argv[], struct Shell *sh) {
    (void)sh;
    if (argc == 1) { show_mmap(); return 0; }
    if (strcmp(argv[1], "-free") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: mmap -free file\n"); return 1;
        }
        if (unmap_mmap_by_path(argv[2]) != 0) {
            fprintf(stderr, "No active mapping found for %s\n", argv[2]);
            return 1;
        }
        return 0;
    }
    if (argc != 3) {
        fprintf(stderr, "Usage: mmap file perms\n"); return 1;
    }
    int protection = 0;
    if (perm_to_prot(argv[2], &protection) != 0) {
        fprintf(stderr, "Invalid permissions: %s\n", argv[2]); return 1;
    }
    void *addr = map_file(argv[1], protection);
    if (!addr) {
        perror("Unable to map file"); return 1;
    }
    printf("Mapped file %s at %p\n", argv[1], addr);
    return 0;
}

int cmd_shared(int argc, char *argv[], struct Shell *sh) {
    (void)sh;
    if (argc == 1) { show_shared(); return 0; }
    if (strcmp(argv[1], "-create") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: shared -create key size\n"); return 1;
        }
        key_t clave;
        if (text_to_key(argv[2], &clave) != 0) {
            fprintf(stderr, "Invalid key: %s\n", argv[2]); return 1;
        }
        size_t tam = 0;
        if (read_size(argv[3], &tam) != 0) {
            fprintf(stderr, "Invalid size: %s\n", argv[3]); return 1;
        }
        if (tam == 0) {
            fprintf(stderr, "Blocks of 0 bytes are not allowed\n"); return 1;
        }
        void *addr = shm_get(clave, tam);
        if (!addr) { perror("Unable to create shared memory"); return 1;
        }
        printf("Allocated %lu bytes at %p\n", (unsigned long)tam, addr);
        return 0;
    }
    if (strcmp(argv[1], "-free") == 0 || strcmp(argv[1], "-delkey") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: shared %s key\n", argv[1]); return 1;
        }
        key_t clave;
        if (text_to_key(argv[2], &clave) != 0) {
            fprintf(stderr, "Invalid key: %s\n", argv[2]); return 1;
        }
        if (strcmp(argv[1], "-free") == 0) {
            if (detach_shared_by_key(clave) != 0) {
                if (errno == ENOENT) {
                    fprintf(stderr, "No shared memory block found for key %lu\n"
                        ,(unsigned long)clave);
                } else perror("Unable to detach shared memory");
                return 1;
            }
            return 0;
        }
        if (delete_system_shared(clave) != 0) {
            perror("Unable to remove shared memory"); return 1;
        }
        printf("Removed shared memory key %lu\n", (unsigned long)clave);
        return 0;
    }
    if (argc != 2) {
        fprintf(stderr, "Usage: shared [key | -create key size | -free key |"
                        " -delkey key]\n"); return 1;
    }
    key_t clave;
    if (text_to_key(argv[1], &clave) != 0) {
        fprintf(stderr, "Invalid key: %s\n", argv[1]); return 1;
    }
    void *addr = shm_get(clave, 0);
    if (!addr) {
        perror("Unable to attach shared memory"); return 1;
    }
    printf("Attached shared memory key %lu at %p\n",
            (unsigned long)clave, addr);
    return 0;
}

static void show_malloc(void) {
    List *list = get_malloc_list();
    if (isEmptyList(list)) {
        puts("Malloc block list is empty"); return;
    }
    for (Node *node = list->head; node; node = node->next) {
        const MallocBlock *block = (const MallocBlock *)node->data;
        if (block) printf("%p -> %zu bytes\n", block->addr, block->size);
    }
}

static int add_malloc(void *addr, size_t size) {
    MallocBlock *block = malloc(sizeof *block);
    if (!block) { perror("malloc"); return -1; }
    block->addr = addr;
    block->size = size;
    if (insertItem(get_malloc_list(), block) != 0) {
        fprintf(stderr, "Unable to store the malloc block\n");
        free(block);
        return -1;
    }
    return 0;
}

static void release_malloc_entry(MallocBlock *block, Node *node, List *list){
    if (node->prev) node->prev->next = node->next;
    else list->head = node->next;
    if (node->next) node->next->prev = node->prev;
    printf("Liberado bloque malloc de %zu bytes en %p\n",
            block->size, block->addr);
    free(block->addr);
    free(block);
    free(node);
}

static int free_malloc_by_size(size_t size) {
    List *list = get_malloc_list();
    if (!list || isEmptyList(list)) { errno = ENOENT; return -1; }
    for (Node *node = list->head; node; node = node->next) {
        MallocBlock *block = (MallocBlock *)node->data;
        if (!block || block->size != size) continue;
        release_malloc_entry(block, node, list);
        return 0;
    }
    return -1;
}

static int free_malloc_by_addr(void *addr) {
    if (!addr) { errno = EINVAL; return -1; }
    List *list = get_malloc_list();
    if (!list || isEmptyList(list)) { errno = ENOENT; return -1; }
    for (Node *node = list->head; node; node = node->next) {
        MallocBlock *block = (MallocBlock *)node->data;
        if (!block || block->addr != addr) continue;
        release_malloc_entry(block, node, list);
        return 0;
    }
    errno = ENOENT;
    return -1;
}

int cmd_malloc(int argc, char *argv[], struct Shell *sh) {
    (void)sh;
    if (argc == 1) { show_malloc(); return 0; }
    bool free_flag = false;
    const char *size_arg = NULL;
    if (strcmp(argv[1], "-free") == 0) {
        free_flag = true;
        if (argc != 3) {
            fprintf(stderr, "Usage: malloc <bytes> [-free]\n"); return 1;
        }
        size_arg = argv[2];
    } else {
        size_arg = argv[1];
        if (argc >= 3) {
            if (strcmp(argv[2], "-free") != 0 || argc > 3) {
                fprintf(stderr, "Usage: malloc <bytes> [-free]\n"); return 1;
            }
            free_flag = true;
        }
    }
    size_t req = 0;
    if (read_size(size_arg, &req) != 0) {
        fprintf(stderr, "Invalid size: %s\n", size_arg); return 1;
    }
    if (req == 0) { puts("Cannot allocate 0 bytes"); return 1; }
    if (free_flag) {
        if (free_malloc_by_size(req) != 0) {
            fprintf(stderr, "No malloc block of %zu bytes found\n", req);
            return 1;
        }
        return 0;
    }
    void *addr = malloc(req);
    if (!addr) { perror("malloc"); return 1; }
    memset(addr, 0, req);
    if (add_malloc(addr, req) != 0) { free(addr); return 1; }
    printf("Allocated %zu bytes at %p\n", req, addr);
    return 0;
}

int cmd_free(int argc, char *argv[], struct Shell *sh) {
    (void)sh;
    if (argc != 2) {
        fprintf(stderr, "Usage: free addr\n"); return 1;
    }
    void *addr = parse_pointer(argv[1]);
    if (!addr) { perror("parse_pointer"); return 1; }
    if (free_malloc_by_addr(addr) == 0) { return 0; }
    if (detach_shared_by_addr(addr) == 0) { return 0; }
    if (unmap_mmap_by_addr(addr) == 0) { return 0; }
    fprintf(stderr, "No block found for %p\n", addr);
    return 1;
}

int cmd_memdump(int argc, char *argv[], struct Shell *sh) {
    (void)sh;
    if (argc != 3) { fprintf(stderr, "Usage: memdump addr count\n"); return 1; }
    void *addr = parse_pointer(argv[1]);
    if (!addr) { perror("parse_pointer"); return 1; }
    size_t cont = 0;
    if (read_size(argv[2], &cont) != 0) {
        fprintf(stderr, "Invalid count: %s\n", argv[2]); return 1;
    }
    if (ensure_valid_region(addr, cont) != 0) {
        fprintf(stderr, "memdump: invalid address %p (%zu bytes)\n",
                addr, cont);
        return 1;
    }
    const size_t bytes_per_line = 16;
    unsigned char *bytes = (unsigned char *)addr;
    for (size_t offset = 0; offset < cont; offset += bytes_per_line) {
        size_t line_count = cont - offset;
        if (line_count > bytes_per_line) line_count = bytes_per_line;
        printf("%p: ", (void *)(bytes + offset));
        for (size_t i = 0; i < bytes_per_line; ++i) {
            if (i < line_count) printf("%02x ", bytes[offset + i]);
            else printf("   ");
            if (i == 7) printf(" ");
        }
        const size_t ascii_max_chars = bytes_per_line * 2+(bytes_per_line - 1);
        char ascii_buf[ascii_max_chars + 1];
        size_t ascii_len = 0;
        ascii_buf[0] = '\0';
        for (size_t i = 0; i < line_count; ++i) {
            char repr[8];
            format_byte_repr(bytes[offset + i], repr, sizeof repr);
            const size_t repr_len = strlen(repr);
            if (ascii_len + repr_len >= sizeof ascii_buf) break;
            memcpy(ascii_buf + ascii_len, repr, repr_len);
            ascii_len += repr_len;
            if (i + 1 < line_count) {
                if (ascii_len + 1 >= sizeof ascii_buf) break;
                ascii_buf[ascii_len++] = ' ';
            }
        }
        ascii_buf[ascii_len] = '\0';
        printf("|%-*s|\n", (int)ascii_max_chars, ascii_buf);
    }
    return 0;
}

static void destroy_malloc_block(void *data) {
    if (!data) return;
    MallocBlock *block = (MallocBlock *)data;
    if (block->addr) free(block->addr);
    free(block);
}

static void destroy_shared_block(void *data) {
    if (!data) return;
    SharedBlock *block = (SharedBlock *)data;
    if (block->addr)
        if (shmdt(block->addr) == -1) perror("shmdt");
    free(block);
}

static void destroy_mmap_block(void *data) {
    if (!data) return;
    MmapBlock *block = (MmapBlock *)data;
    if (block->addr && block->size > 0)
        if (munmap(block->addr, block->size) == -1) perror("munmap");
    free(block);
}

void mem_cleanup(void) {
    clearList(get_mmap_list(), destroy_mmap_block);
    clearList(get_shared_list(), destroy_shared_block);
    clearList(get_malloc_list(), destroy_malloc_block);
}

int cmd_memfill(int argc, char *argv[], struct Shell *sh) {
    (void)sh;
    if (argc != 4) {
        fprintf(stderr, "Usage: memfill addr count byte\n"); return 1;
    }
    void *addr = parse_pointer(argv[1]);
    if (!addr) { perror("parse_pointer"); return 1; }
    size_t cont = 0;
    if (read_size(argv[2], &cont) != 0) {
        fprintf(stderr, "Invalid count: %s\n", argv[2]); return 1;
    }
    if (ensure_valid_region(addr, cont) != 0) {
        fprintf(stderr, "memfill: invalid address %p (%zu bytes)\n",
                addr, cont);
        return 1;
    }
    unsigned char byte = 0;
    if (read_byte(argv[3], &byte) != 0) {
        fprintf(stderr, "Invalid byte: %s\n", argv[3]); return 1;
    }
    fill_memory(addr, cont, byte);
    printf("Filled %zu bytes at %p with 0x%02x\n", cont, addr, (unsigned)byte);
    return 0;
}

int cmd_recurse(int argc, char *argv[], struct Shell *sh){
    (void)sh;
    if (argc != 2) {
        fprintf(stderr, "Usage: recurse n\n"); return 1;
    }
    int n = 0;
    if (read_int(argv[1], 0, INT_MAX, &n) != 0) {
        fprintf(stderr, "Invalid integer: %s\n", argv[1]); return 1;
    }
    recurse_steps(n);
    return 0;
}

static void recurse_steps(int n)
{
    char automatico[TAMANO];
    static char estatico[TAMANO];
    printf("argument:%3d(%p) automatic %p, static array %p\n",
           n, (void *)&n, (void *)automatico, (void *)estatico);
    if (n - 1 > 0) { recurse_steps(n - 1); }
}

static ssize_t read_file_chunk(char *f, void *p, size_t cont)
{
    struct stat s;
    ssize_t  n;
    int df,aux;
    if (stat (f,&s)==-1 || (df=open(f,O_RDONLY))==-1) return -1;
    if (cont == (size_t)-1) {  /* si pasamos -1 como bytes a leer lo leemos entero*/
        if (s.st_size < 0) {
            close(df);
            errno = EINVAL;
            return -1;
        }
        cont = (size_t)s.st_size;
    }
    if (ensure_valid_region(p, cont) != 0) {
        close(df);
        errno = EFAULT;
        return -1;
    }
    if ((n=read(df,p,cont))==-1){
        aux = errno;
        close(df);
        errno = aux;
        return -1;
    }
    close (df);
    return n;
}

int cmd_readfile(int argc, char *argv[], struct Shell *sh){
    (void)sh;
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Usage: readfile file addr [count]\n"); return 1;
    }
    void *addr = parse_pointer(argv[2]);
    if (!addr) { perror("parse_pointer"); return 1; }
    size_t cont = (size_t)-1;
    if (argc == 4) {
        if (read_size(argv[3], &cont) != 0) {
            fprintf(stderr, "Invalid count: %s\n", argv[3]); return 1;
        }
    }
    ssize_t n = read_file_chunk(argv[1], addr, cont);
    if (n == -1) { perror("Unable to read file"); return 1; }
    printf("Read %lld bytes from %s into %p\n", (long long)n, argv[1], addr);
    return 0;
}

int cmd_read(int argc, char *argv[], struct Shell *sh){
    (void)sh;
    if (argc != 4) {
        fprintf(stderr, "Usage: read fd addr count\n"); return 1;
    }
    int fd = 0;
    if (read_fd(argv[1], &fd) != 0) {
        fprintf(stderr, "Invalid descriptor: %s\n", argv[1]); return 1;
    }
    void *addr = parse_pointer(argv[2]);
    if (!addr) { perror("parse_pointer"); return 1; }
    size_t count = 0;
    if (read_size(argv[3], &count) != 0) {
        fprintf(stderr, "Invalid count: %s\n", argv[3]); return 1;
    }
    if (ensure_valid_region(addr, count) != 0) {
        fprintf(stderr, "read: invalid address %p (%zu bytes)\n",
                addr, count);
        return 1;
    }
    ssize_t n = read(fd, addr, count);
    if (n == -1) { perror("read"); return 1; }

    printf("%lld bytes read from descriptor %d into %p\n",
            (long long)n, fd, addr);
    return 0;
}

int cmd_writefile(int argc, char *argv[], struct Shell *sh){
    (void)sh;
    bool overwrite = false;
    const char *path = NULL;
    const char *addr_arg = NULL;
    const char *count_arg = NULL;
    if (argc == 5 && strcmp(argv[1], "-o") == 0) {
        overwrite = true;
        path = argv[2];
        addr_arg = argv[3];
        count_arg = argv[4];
    } else if (argc == 4 && strcmp(argv[1], "-o") != 0) {
        path = argv[1];
        addr_arg = argv[2];
        count_arg = argv[3];
    } else {
        fprintf(stderr, "Usage: writefile [-o] file addr count\n"); return 1;
    }
    void *addr = parse_pointer(addr_arg);
    if (!addr) { perror("parse_pointer"); return 1; }

    size_t count = 0;
    if (read_size(count_arg, &count) != 0) {
        fprintf(stderr, "Invalid count: %s\n", count_arg); return 1;
    }
    if (ensure_valid_region(addr, count) != 0) {
        fprintf(stderr, "writefile: invalid address %p (%zu bytes)\n",
                addr, count);
        return 1;
    }
    int flags = overwrite ? (O_WRONLY | O_CREAT | O_TRUNC)
                            : (O_WRONLY | O_CREAT | O_EXCL);
    int fd = open(path, flags, 0666);
    if (fd == -1) { perror("open"); return 1; }
    ssize_t written = write(fd, addr, count);
    if (written == -1) { perror("write"); close(fd); return 1; }
    printf("%lld bytes written to %s from %p\n",
            (long long)written, path, addr);
    close(fd);
    return 0;
}

int cmd_write(int argc, char *argv[], struct Shell *sh){
    (void)sh;
    if (argc != 4) { fprintf(stderr, "Usage: write fd addr count\n"); return 1;}
    int fd = 0;
    if (read_fd(argv[1], &fd) != 0) {
        fprintf(stderr, "Invalid descriptor: %s\n", argv[1]); return 1;
    }
    void *addr = parse_pointer(argv[2]);
    if (!addr) { perror("parse_pointer"); return 1; }
    size_t count = 0;
    if (read_size(argv[3], &count) != 0) {
        fprintf(stderr, "Invalid count: %s\n", argv[3]); return 1;
    }
    if (ensure_valid_region(addr, count) != 0) {
        fprintf(stderr, "write: invalid address %p (%zu bytes)\n",
                addr, count);
        return 1;
    }
    ssize_t written = write(fd, addr, count);
    if (written == -1) { perror("write"); return 1; }
    printf("%lld bytes written to descriptor %d from %p\n",
            (long long)written, fd, addr);
    return 0;
}

int cmd_mem(int argc, char *argv[], struct Shell *sh) {
    (void)sh;
    if (argc != 2) {
        fprintf(stderr, "Usage: mem [-funcs|-vars|-blocks|-all|-pmap]\n");
        return 1;
    }
    if (strcmp(argv[1], "-funcs") == 0) {
        print_function_addresses(); return 0;
    }
    if (strcmp(argv[1], "-vars") == 0) {
        print_variable_addresses(); return 0;
    }
    if (strcmp(argv[1], "-blocks") == 0) {
        print_block_lists(); return 0;
    }
    if (strcmp(argv[1], "-all") == 0) {
        print_function_addresses();
        print_variable_addresses();
        print_block_lists();
        return 0;
    }
    if (strcmp(argv[1], "-pmap") == 0) return run_process_map();
    fprintf(stderr, "Usage: mem [-funcs|-vars|-blocks|-all|-pmap]\n");
    return 1;
}
