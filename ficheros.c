// Pablo Araújo Rodríguez   pablo.araujo@udc.es
// Uriel Liñares Vaamonde   uriel.linaresv@udc.es

#include "ficheros.h"

static List *open_files = NULL;

static void ensure_file_list(void) {
    if (!open_files) open_files = createList();
}

static bool file_list_ready(void) {
    if (!open_files) {
        fprintf(stderr, "Internal error: file list not initialized\n");
        return false;
    }
    return true;
}

void ficheros_init(void) {
    ensure_file_list();
    clearList(open_files, delFile);
    insertItem(open_files, make_itemF(STDIN_FILENO,  "stdin",  O_RDONLY));
    insertItem(open_files, make_itemF(STDOUT_FILENO, "stdout", O_WRONLY));
    insertItem(open_files, make_itemF(STDERR_FILENO, "stderr", O_WRONLY));
}

void ficheros_shutdown(void) {
    if (!open_files) return;
    deleteList(open_files, delFile);
    open_files = NULL;
}

static DirParams global_dir_params = { false, false, false, DIR_REC_NOREC };

const DirParams *dirparams_get(void) { return &global_dir_params; }

static bool is_hidden_name(const char *name) { return name[0] == '.'; }

int get_fd(char *argv[]){
    char *endp = NULL;
    long lfd = strtol(argv[1], &endp, 10);
    if (*argv[1] == '\0' || *endp != '\0' || lfd < 0) {
        fprintf(stderr, "Invalid fd '%s'\n", argv[1]);
        return 1;
    }
    return (int)lfd;
}

static void fmt_mtime(time_t t, char *buf, size_t n){
    struct tm lt;
    localtime_r(&t, &lt);
    if (strftime(buf, n, "%Y-%m-%d %H:%M", &lt) == 0) {
        strncpy(buf, "0000-00-00 00:00", n);
        if (n) buf[n-1] = '\0';
    }
}

static int print_one_with_params(const char *path, const DirParams *p){
    struct stat sb;
    if (lstat(path, &sb) == -1) { perror(path); return 1; }
    if (!p->longfmt) {
        printf("%s\t%lld", path, (long long)sb.st_size);
        if (p->showlink && S_ISLNK(sb.st_mode)) {
            char tgt[PATH_MAX];
            ssize_t n = readlink(path, tgt, sizeof(tgt)-1);
            if (n >= 0) { tgt[n] = '\0'; printf(" -> %s", tgt); }
        }
        putchar('\n');
        return 0;
    }
    char perms[12];
    (void)convertMode(sb.st_mode, perms);
    struct passwd *pw = getpwuid(sb.st_uid);
    struct group  *gr = getgrgid(sb.st_gid);
    char tbuf[64];
    fmt_mtime(sb.st_mtime, tbuf, sizeof tbuf);
    printf("%s %3ld %-8s %-8s %9lld %s %s", perms, (long)sb.st_nlink,
            pw ? pw->pw_name : "unknown", gr ? gr->gr_name : "unknown",
            (long long)sb.st_size, tbuf, path);
    if (p->showlink && S_ISLNK(sb.st_mode)) {
        char tgt[PATH_MAX];
        ssize_t n = readlink(path, tgt, sizeof(tgt)-1);
        if (n >= 0) { tgt[n] = '\0'; printf(" -> %s", tgt); }
    }
    putchar('\n');
    return 0;
}

static int list_dir_once(const char *dirpath, const DirParams *p){
    DIR *d = opendir(dirpath);
    if (!d) { perror(dirpath); return 1; }
    printf("%s:\n", dirpath);
    int n, status = 0;
    struct dirent *de;
    char full[PATH_MAX];
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        if (!p->showhid && is_hidden_name(de->d_name)) continue;
        n = snprintf(full, sizeof full, "%s/%s", dirpath, de->d_name);
        if (n < 0 || (size_t)n >= sizeof full) {
            fprintf(stderr, "Path too long: %s/%s\n", dirpath, de->d_name);
            status = 1; continue;
        }
        if (print_one_with_params(full, p) != 0) status = 1;
    }
    closedir(d);
    putchar('\n');
    return status;
}

static int list_dir_recursive(const char *dirpath, const DirParams *p) {
    int status = 0;
    if (p->rec == DIR_REC_RECA || p->rec == DIR_REC_NOREC) {
        if (list_dir_once(dirpath, p) != 0) status = 1;
    }
    if (p->rec != DIR_REC_NOREC) {
        DIR *d = opendir(dirpath);
        if (!d) { perror(dirpath); return 1; }
        struct dirent *de;
        char full[PATH_MAX];
        while ((de = readdir(d)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;
            if (!p->showhid && is_hidden_name(de->d_name)) continue;
            int n = snprintf(full, sizeof full, "%s/%s", dirpath, de->d_name);
            if (n < 0 || (size_t)n >= sizeof full) {
                fprintf(stderr, "Path too long: %s/%s\n", dirpath, de->d_name);
                status = 1; continue;
            }
            struct stat sb;
            if (lstat(full, &sb) != 0) { perror(full); status = 1; continue; }
            if (S_ISDIR(sb.st_mode)) {
                if (list_dir_recursive(full, p) != 0) status = 1;
            }
        }
        closedir(d);
    }
    if (p->rec == DIR_REC_RECB) {
        if (list_dir_once(dirpath, p) != 0) status = 1;
    }
    return status;
}


tItemF *make_itemF(int fd, const char *name, int mode) {
    tItemF *p = malloc(sizeof *p);
    if (!p) { perror("malloc"); exit(1); }
    p->fileDescriptor = fd;
    strncpy(p->filename, name, sizeof(p->filename) - 1);
    p->filename[sizeof(p->filename) - 1] = '\0';
    p->mode = mode;
    return p;
}

static void take_flags(int flags, char *out, size_t n)
{
    bool first = true;
    out[0] = '\0';
    #define ADD(tok) do{ \
        if (!first) strncat(out, ",", n > 0 ? n - strlen(out) - 1 : 0); \
        strncat(out, (tok), n > 0 ? n - strlen(out) - 1 : 0); \
        first = false; \
    }while(0)
    switch (flags & O_ACCMODE) {
        case O_RDONLY: ADD("ro"); break;
        case O_WRONLY: ADD("wo"); break;
        case O_RDWR:   ADD("rw"); break;
        default: break;
    }
    if (flags & O_CREAT)  ADD("cr");
    if (flags & O_EXCL)   ADD("ex");
    if (flags & O_APPEND) ADD("ap");
    if (flags & O_TRUNC)  ADD("tr");
    #undef ADD
}

int cmd_listOpen(int argc, char *argv[]) {
    (void)argc; (void)argv;
    if (!open_files || open_files->head == NULL) { printf("Empty list\n"); return 1; }
    for (Node *n = open_files->head; n; n = n->next) {
        tItemF *it = (tItemF*)n->data;
        char fl[128];
        take_flags(it->mode, fl, sizeof fl);
        off_t pos = lseek(it->fileDescriptor, 0, SEEK_CUR);
        if (pos == (off_t)-1) {
            printf("[%d] %s (mode=%s) pos=?\n",
                    it->fileDescriptor, it->filename, fl);
        } else {
            printf("[%d] %s (mode=%s) pos=%jd\n",
                    it->fileDescriptor, it->filename, fl, (intmax_t)pos);
        }
    }
    return 0;
}

static int comparar_itemF_fd(void *data, void *key) {
    const tItemF *it = (const tItemF*)data;
    const int fd = *(const int*)key;
    if (it->fileDescriptor == fd) return 0;
    return (it->fileDescriptor > fd) - (it->fileDescriptor < fd);
}

static int comparar_itemF_name(void *data, void *key) {
    const tItemF *it = (const tItemF*)data;
    const char *name = (const char*)key;
    if (strcmp(it->filename, name) == 0) return 0;
    return strcmp(it->filename, name);
}

int addFile(List *lista, const char *path, int flags)
{
    int fd = (flags & O_CREAT) ? open(path, flags, 0666) : open(path, flags);
    if (fd == -1) { perror("open"); return 1; }
    tItemF *it = make_itemF(fd, path, flags);
    if (insertItem(lista, it) != 0) {
        free(it);
        close(fd);
        fprintf(stderr, "Could not insert on the list\n");
        return 1;
    }
    return 0;
}

// ficheros.c
void delFile(void *data) {
    if (!data) return;
    tItemF *it = (tItemF*)data;
    if (it->fileDescriptor > 2) close(it->fileDescriptor);
    free(it);
}

static void free_itemF(void *data) {
    if (!data) return;
    free(data);
}

int cmd_open (int argc, char *argv[])
{
    if (!file_list_ready()) return 1;
    if (argc == 1) { return cmd_listOpen(argc, argv); }
    int flags = 0;
    for (int i = 2; argv[i] != NULL; ++i) {
        if      (!strcmp(argv[i], "cr")) flags |= O_CREAT;
        else if (!strcmp(argv[i], "ex")) flags |= O_EXCL;
        else if (!strcmp(argv[i], "ro")) flags |= O_RDONLY;
        else if (!strcmp(argv[i], "wo")) flags |= O_WRONLY;
        else if (!strcmp(argv[i], "rw")) flags |= O_RDWR;
        else if (!strcmp(argv[i], "ap")) flags |= O_APPEND;
        else if (!strcmp(argv[i], "tr")) flags |= O_TRUNC;
        else break;
    }
    int fd = open(argv[1], flags, 0666);
    if (fd == -1) { perror("open"); return 1; }
    tItemF *item = make_itemF(fd, argv[1], flags);
    if (insertItem(open_files, item) != 0) {
        perror("insertItem");
        close(fd);
        free(item);
        return 1;
    }
    printf("Opened [%d] %s (mode=%d)\n", fd, argv[1], flags);
    return 0;
}

int cmd_close (int argc, char *argv[]){
    if (!file_list_ready()) {
        return 1;
    }
    if (argc == 1) { return cmd_listOpen(argc, argv); }
    char *endp = NULL;
    errno = 0;
    long lfd = strtol(argv[1], &endp, 10);
    tItemF *f = NULL;
    int df = -1;
    bool close_by_fd = false;
    if (errno == 0 && endp && *argv[1] != '\0' && *endp == '\0') {
        close_by_fd = true;
        df = (int)lfd;
        if (df < 0) {
            fprintf(stderr, "close: invalid fd '%s'\n", argv[1]);
            return 1;
        }
        f = (tItemF*)findItem(open_files, &df, comparar_itemF_fd);
        if (!f) {
            fprintf(stderr, "close: fd %d not found in open list\n", df);
            return 1;
        }
    } else {
        f = (tItemF*)findItem(open_files, argv[1], comparar_itemF_name);
        if (!f) {
            fprintf(stderr, "close: file '%s' not found in open list\n",
                argv[1]); return 1; }
        df = f->fileDescriptor;
    }
    char closed_name[MAX];
    int closed_fd = f->fileDescriptor;
    strncpy(closed_name, f->filename, sizeof(closed_name) - 1);
    closed_name[sizeof(closed_name) - 1] = '\0';
    if (closed_fd > 2)
        if (close(closed_fd) == -1) { perror("close"); return 1; }
    if (close_by_fd)
        deleteItem(open_files, &df, comparar_itemF_fd, free_itemF);
    else { deleteItem(open_files, argv[1], comparar_itemF_name, free_itemF); }
    printf("Closed [%d] %s\n", closed_fd, closed_name);
    return 0;
}

int cmd_dup(int argc, char *argv[])
{
    if (!file_list_ready()) { return 1; }
    if (argc == 1) { fprintf(stderr, "dup: missing fd\n"); return 1; }
    int df = atoi(argv[1]);
    if (df < 0) { fprintf(stderr, "dup: invalid fd '%s'\n", argv[1]); return 1;}
    tItemF *f = (tItemF*)findItem(open_files, &df, comparar_itemF_fd);
    if (!f) {
        fprintf(stderr, "dup: fd %d not found in open list\n", df);
        return 1;
    }
    int duplicated = dup(df);
    if (duplicated == -1) { perror("dup"); return 1; }
    tItemF *newItem = make_itemF(duplicated, f->filename, f->mode);
    if (insertItem(open_files, newItem) != 0) {
        perror("insertItem");
        close(duplicated);
        free(newItem);
        return 1;
    }
    printf("Duplicated %d -> [%d] %s\n", df, duplicated, f->filename);
    return 0;
}

static char assignLetterToType(const mode_t mode){
    switch (mode & __S_IFMT)
    { // AND bit a bit con los bits de formato 0170000
    case __S_IFSOCK: return 's'; // socket
    case __S_IFLNK: return 'l'; // symbolic link
    case __S_IFREG: return '-'; // fichero normal
    case __S_IFBLK: return 'b'; // block device
    case __S_IFDIR: return 'd'; // directorio
    case __S_IFCHR: return 'c'; // char device
    case __S_IFIFO: return 'p'; // pipe
    default: return '?'; // desconocido
    }
}

char * convertMode (mode_t m, char *permisos){
    strcpy (permisos,"---------- ");
    permisos[0]=assignLetterToType(m);
    if (m&S_IRUSR) permisos[1]='r';    /*propietario*/
    if (m&S_IWUSR) permisos[2]='w';
    if (m&S_IXUSR) permisos[3]='x';
    if (m&S_IRGRP) permisos[4]='r';    /*grupo*/
    if (m&S_IWGRP) permisos[5]='w';
    if (m&S_IXGRP) permisos[6]='x';
    if (m&S_IROTH) permisos[7]='r';    /*resto*/
    if (m&S_IWOTH) permisos[8]='w';
    if (m&S_IXOTH) permisos[9]='x';
    if (m&S_ISUID) permisos[3]='s';    /*setuid, setgid y stickybit*/
    if (m&S_ISGID) permisos[6]='s';
    if (m&__S_ISVTX) permisos[9]='t';
    return permisos;
}

char * convertMode2 (mode_t m){
    static char permisos[12];
    strcpy (permisos,"---------- ");
    permisos[0]=assignLetterToType(m);
    if (m&S_IRUSR) permisos[1]='r';    /*propietario*/
    if (m&S_IWUSR) permisos[2]='w';
    if (m&S_IXUSR) permisos[3]='x';
    if (m&S_IRGRP) permisos[4]='r';    /*grupo*/
    if (m&S_IWGRP) permisos[5]='w';
    if (m&S_IXGRP) permisos[6]='x';
    if (m&S_IROTH) permisos[7]='r';    /*resto*/
    if (m&S_IWOTH) permisos[8]='w';
    if (m&S_IXOTH) permisos[9]='x';
    if (m&S_ISUID) permisos[3]='s';    /*setuid, setgid y stickybit*/
    if (m&S_ISGID) permisos[6]='s';
    if (m&__S_ISVTX) permisos[9]='t';
    return permisos;
}

char * convertMode3 (mode_t m){
    char *permisos;
    if ((permisos=(char *) malloc (12))==NULL)
        return NULL;
    strcpy (permisos,"---------- ");
    permisos[0]=assignLetterToType(m);
    if (m&S_IRUSR) permisos[1]='r';    /*propietario*/
    if (m&S_IWUSR) permisos[2]='w';
    if (m&S_IXUSR) permisos[3]='x';
    if (m&S_IRGRP) permisos[4]='r';    /*grupo*/
    if (m&S_IWGRP) permisos[5]='w';
    if (m&S_IXGRP) permisos[6]='x';
    if (m&S_IROTH) permisos[7]='r';    /*resto*/
    if (m&S_IWOTH) permisos[8]='w';
    if (m&S_IXOTH) permisos[9]='x';
    if (m&S_ISUID) permisos[3]='s';    /*setuid, setgid y stickybit*/
    if (m&S_ISGID) permisos[6]='s';
    if (m&__S_ISVTX) permisos[9]='t';
    return permisos;
}

int cmd_create(int argc, char *argv[]){
    if (argc == 3 && strcmp(argv[1], "-f") == 0){
        const char* fileName = argv[2];
        const int df = open(fileName, O_CREAT | O_EXCL, S_IRWXU |
            S_IRWXG | S_IROTH | S_IXOTH);
        if (df == -1){ perror("Could not create file"); return 1; }
        close(df);
        printf("File created successfully\n");
        return 0;
    }else if(argc == 2){
        const char* dirName = argv[1];
        if (mkdir(dirName, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0){
            perror("Could not create directory");
            return 1;
        }
        printf("Directory created successfully\n");
        return 0;
    }else{ perror("Could not create file or directory"); return 1; }
}

int cmd_erase(int argc, char *argv[]){
    struct stat sb;
    if(argc > 1){
        for (int i = 1; i < argc; i++){
            if (lstat(argv[i], &sb) != 0) {
                perror("Impossible to erase");
                return 1;
            }
            else{
                const char type = assignLetterToType(sb.st_mode);
                if (type == 'd'){
                    if (rmdir(argv[i])){ perror("Could not erase"); return 1; }
                    else{
                        printf("Directory removed successfully\n");
                        return 0;
                    }
                }else if (type == '-'){
                    if (unlink(argv[i])){ perror("Could not erase"); return 1; }
                    else{ printf("File removed successfully\n"); return 0; }
                }
            }
        }
        return 0;
    }else{ perror("Invalid arguments for erase"); return 1; }
}

int cmd_setdirparams(int argc, char *argv[]){
    if (argc < 2) {
        fprintf(stderr,
            "Usage: setdirparams long|short | link|nolink | hid|nohid | "
            "reca|recb|norec\n");
        return 1;
    }
    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if      (strcmp(a,"long")  == 0) global_dir_params.longfmt = true;
        else if (strcmp(a,"short") == 0) global_dir_params.longfmt = false;
        else if (strcmp(a,"link")  == 0) global_dir_params.showlink = true;
        else if (strcmp(a,"nolink") == 0) global_dir_params.showlink = false;
        else if (strcmp(a,"hid")   == 0) global_dir_params.showhid = true;
        else if (strcmp(a,"nohid") == 0) global_dir_params.showhid = false;
        else if (strcmp(a,"reca")  == 0) global_dir_params.rec = DIR_REC_RECA;
        else if (strcmp(a,"recb")  == 0) global_dir_params.rec = DIR_REC_RECB;
        else if (strcmp(a,"norec") == 0) global_dir_params.rec = DIR_REC_NOREC;
        else {
            fprintf(stderr, "setdirparams: invalid parameter '%s'\n", a);
            return 1;
        }
    }
    return 0;
}

int cmd_getdirparams(int argc, char *argv[]){
    (void)argc; (void)argv;
    printf("format   : %s\n", global_dir_params.longfmt ? "long" : "short");
    printf("symlink  : %s\n", global_dir_params.showlink ? "link" : "nolink");
    printf("hidden   : %s\n", global_dir_params.showhid ? "hid" : "nohid");
    const char *r = (global_dir_params.rec == DIR_REC_NOREC) ? "norec" :
                    (global_dir_params.rec == DIR_REC_RECA)  ? "reca"  : "recb";
    printf("recursion: %s\n", r);
    return 0;
}

int cmd_dir(int argc, char *argv[]){
    int i = 1, status = 0; const DirParams *p = dirparams_get();
    bool list_contents = false; char *path; struct stat sb;
    if (i < argc && strcmp(argv[i], "-d") == 0) { list_contents = true; ++i; }
    if (argc <= 1) {
        if (list_contents) return list_dir_recursive(".", p);
        else return print_one_with_params(".", p);
    }
    for (; i < argc; ++i) {
        path = argv[i];
        if (lstat(path, &sb) != 0) {
            perror(path);
            status = 1;
            continue;
        }
        if (S_ISDIR(sb.st_mode) && list_contents) {
            if (list_dir_recursive(path, p) != 0) status = 1;
        } else { if (print_one_with_params(path, p) != 0) status = 1; }
    }
    return status;
}

static int delrec_path(const char *path){
    struct stat sb;
    int n;
    if (lstat(path, &sb) == -1) { perror(path); return 1; }
    if (!S_ISDIR(sb.st_mode)) {
        if (unlink(path) == -1) { perror(path); return 1; }
        return 0;
    }
    DIR *d = opendir(path);
    if (!d) { perror(path); return 1; }
    int status = 0; struct dirent *de; char full[PATH_MAX];
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        n = snprintf(full, sizeof full, "%s/%s", path, de->d_name);
        if (n < 0 || (size_t)n >= sizeof full) {
            fprintf(stderr, "Path too long: %s/%s\n", path, de->d_name);
            status = 1; continue;
        }
        if (delrec_path(full) != 0) status = 1;
    }
    closedir(d);
    if (rmdir(path) == -1) { perror(path); status = 1; }
    return status;
}

int cmd_delrec(int argc, char *argv[]){
    if (argc < 2) { fprintf(stderr, "Usage: delrec n1 [n2 ...]\n"); return 1; }
    int status = 0;
    for (int i = 1; i < argc; ++i) {
        if (delrec_path(argv[i]) != 0) status = 1;
    }
    return status;
}

int cmd_lseek(int argc, char *argv[]){
    if (argc != 4) {
        fprintf(stderr, "Usage: lseek df off SEEK_SET|SEEK_CUR|SEEK_END\n");
        return 1;
    }
    int fd = get_fd(argv);
    if (fd < 0) return 1;
    char *endp = NULL; errno = 0;
    long long loff = strtoll(argv[2], &endp, 10);
    if (*argv[2] == '\0' || *endp != '\0' || errno != 0) {
        fprintf(stderr, "lseek: invalid offset '%s'\n", argv[2]);
        return 1;
    }
    off_t off = (off_t)loff;
    int whence;
    if      (strcmp(argv[3], "SEEK_SET") == 0) whence = SEEK_SET;
    else if (strcmp(argv[3], "SEEK_CUR") == 0) whence = SEEK_CUR;
    else if (strcmp(argv[3], "SEEK_END") == 0) whence = SEEK_END;
    else {
        fprintf(stderr, "lseek: invalid whence '%s'\n", argv[3]);
        return 1;
    }
    off_t pos = lseek(fd, off, whence);
    if (pos == (off_t)-1) { perror("lseek"); return 1; }
    printf("%jd\n", (intmax_t)pos);
    return 0;
}

static int write_all(int fd, const char *buf, size_t len){
    size_t done = 0;
    while (done < len) {
        ssize_t n = write(fd, buf + done, len - done);
        if (n == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        done += (size_t)n;
    }
    return 0;
}

int cmd_writestr(int argc, char *argv[]){
    if (argc < 3) { fprintf(stderr, "Usage: writestr df str\n"); return 1; }
    int fd = get_fd(argv);
    if (fd < 0) return 1;
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) { perror("fcntl"); return 1; }
    int acc = flags & O_ACCMODE;
    if (acc == O_RDONLY) {
        fprintf(stderr, "writestr: fd %d is O_RDONLY "
            "(open with 'wo' or 'rw')\n", fd);
        return 1;
    }
    char buf[8192]; size_t pos = 0;
    for (int i = 2; i < argc; ++i) {
        size_t w = strlen(argv[i]);
        if (pos + w + 1 >= sizeof buf) {
            fprintf(stderr, "writestr: string too long\n");
            return 1;
        }
        memcpy(buf + pos, argv[i], w);
        pos += w;
        if (i + 1 < argc) buf[pos++] = ' ';
    }
    if (write_all(fd, buf, pos) == -1) { perror("write"); return 1; }
    printf("%zu bytes written\n", pos);
    return 0;
}
