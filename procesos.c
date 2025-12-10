#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "procesos.h"

static List *background_processes = NULL;

static void ensure_process_list(void) {
    if (!background_processes) {
        background_processes = createList();
    }
}

int cmd_fork(int argc, char *argv[]) {
    (void)argv;
    if (argc != 1) {
        fprintf(stderr, "Usage: fork\n");
        return 1;
    }
    pid_t pid;
    if ((pid = fork()) == 0){
        printf ("ejecutando proceso %d\n", getpid());
        exit(0); // Run atexit handlers to release allocations in the child
    } else if (pid > 0) {
        waitpid (pid,NULL,0);
        return 0;
    }
    perror("fork");
    return 1;
}

int buscar_variable(char *var, char *e[]){
    int pos=0;
    char aux[MAXVAR];
    strcpy (aux,var);
    strcat (aux,"=");
    while (e[pos] != NULL){
        if (!strncmp(e[pos], aux, strlen(aux))) return (pos);
        else pos++;
    }
    errno = ENOENT;
    return(-1);
}

int cambiar_variable(char *var, char *valor, char *e[]){
    int pos;
    char *aux;
    if ((pos = buscar_variable(var, e)) == -1) return -1;
    if ((aux = (char *)malloc(strlen(var) + strlen(valor) + 2)) == NULL)
        return -1;
    strcpy(aux, var);
    strcat(aux, "=");
    strcat(aux, valor);
    e[pos] = aux;
    return pos;
}

static void record_status_from_wait(ProcessInfo *info, int status) {
    if (!info) return;
    if (WIFEXITED(status)) {
        info->status = FINISHED;
        info->exit_status = WEXITSTATUS(status);
        info->signal_number = -1;
    } else if (WIFSIGNALED(status)) {
        info->status = SIGNALED;
        info->signal_number = WTERMSIG(status);
        info->exit_status = -1;
    } else if (WIFSTOPPED(status)) {
        info->status = STOPPED;
        info->signal_number = WSTOPSIG(status);
        info->exit_status = -1;
    } else if (WIFCONTINUED(status)) {
        info->status = ACTIVE;
        info->signal_number = -1;
        info->exit_status = -1;
    }
}

static void refresh_background_processes(void) {
    if (!background_processes) return;
    int status;
    for (Node *node = background_processes->head; node != NULL;
        node = node->next) {
        ProcessInfo *info = (ProcessInfo *)node->data;
        if (!info) continue;
        pid_t res = waitpid(info->pid, &status,
                            WNOHANG | WUNTRACED | WCONTINUED);
        if (res == info->pid) {
            record_status_from_wait(info, status);
        }
    }
}

static ProcessInfo *create_process_info(pid_t pid, const char *cmdline,
                                        const ExecSpec *spec) {
    ProcessInfo *info = calloc(1, sizeof *info);
    if (!info) {
        perror("calloc");
        return NULL;
    }
    info->pid = pid;
    info->launch_time = time(NULL);
    info->status = ACTIVE;
    info->exit_status = -1;
    info->signal_number = -1;
    if (cmdline) {
        strncpy(info->command_line, cmdline, sizeof(info->command_line) - 1);
        info->command_line[sizeof(info->command_line) - 1] = '\0';
    }
    if (spec && spec->has_priority) {
        info->priority = spec->priority;
    }
    errno = 0;
    int prio = getpriority(PRIO_PROCESS, (id_t)pid);
    if (errno == 0) info->priority = prio;
    return info;
}

static int add_background_process(pid_t pid, const char *cmdline,
                                  const ExecSpec *spec) {
    ensure_process_list();
    ProcessInfo *info = create_process_info(pid, cmdline, spec);
    if (!info) return -1;
    if (insertItem(background_processes, info) != 0) {
        free(info);
        fprintf(stderr, "Unable to track process %d\n", pid);
        return -1;
    }
    return 0;
}

static void destroy_process_info(void *data) {
    if (!data) return;
    ProcessInfo *info = (ProcessInfo *)data;
    free(info);
}

void procesos_init(void) {
    ensure_process_list();
    clearList(background_processes, destroy_process_info);
}

void procesos_destroy(void) {
    if (!background_processes) return;
    deleteList(background_processes, destroy_process_info);
    background_processes = NULL;
}

void procesos_cleanup(void) {
    if (!background_processes) return;
    clearList(background_processes, destroy_process_info);
}

void procesos_refresh(void) {
    refresh_background_processes();
}

static bool parse_priority_token(const char *token, int *priority) {
    if (!token || token[0] != '@') return false;
    char *endptr = NULL;
    long val = strtol(token + 1, &endptr, 10);
    if (token[1] == '\0' || (endptr && *endptr != '\0')) return false;
    if (priority) *priority = (int)val;
    return true;
}

static int parse_progspec_tokens(int argc, char *argv[], bool allow_background,
                                ExecSpec *spec) {
    if (!spec) return -1;
    if (argc <= 0) {
        fprintf(stderr, "Missing program to execute\n");
        return -1;
    }
    spec->argc = 0;
    spec->background = false;
    spec->has_priority = false;
    spec->priority = 0;
    int effective_argc = argc;
    if (allow_background && argc > 0 && strcmp(argv[argc - 1], "&") == 0) {
        spec->background = true;
        effective_argc--;
        if (effective_argc == 0) {
            fprintf(stderr, "Background execution requires a program\n");
            return -1;
        }
    } else if (!allow_background && argc > 0 &&
                strcmp(argv[argc - 1], "&") == 0) {
        fprintf(stderr, "Background execution not supported here\n");
        return -1;
    }
    for (int i = 0; i < effective_argc; ++i) {
        int pri = 0;
        if (parse_priority_token(argv[i], &pri)) {
            if (spec->has_priority) {
                fprintf(stderr, "Priority specified multiple times\n");
                return -1;
            }
            spec->has_priority = true;
            spec->priority = pri;
            continue;
        }
        if (spec->argc + 1 >= MAX_TR) {
            fprintf(stderr, "Too many arguments for execution\n");
            return -1;
        }
        spec->argv[spec->argc++] = argv[i];
    }
    spec->argv[spec->argc] = NULL;
    if (spec->argc == 0) {
        fprintf(stderr, "Missing program to execute\n");
        return -1;
    }
    return 0;
}

static void build_command_line(char *dest, size_t cap,
                               int argc, char *argv[]) {
    if (!dest || cap == 0) return;
    dest[0] = '\0';
    for (int i = 0; i < argc; ++i) {
        int dummy;
        if (strcmp(argv[i], "&") == 0) continue;
        if (parse_priority_token(argv[i], &dummy)) continue;
        if (dest[0] != '\0') strncat(dest, " ", cap - strlen(dest) - 1);
        strncat(dest, argv[i], cap - strlen(dest) - 1);
    }
}

static bool command_exists_in_path(const char *cmd) {
    if (!cmd || *cmd == '\0') return false;
    if (strchr(cmd, '/')) {
        return access(cmd, X_OK) == 0;
    }
    const char *path = getenv("PATH");
    if (!path || *path == '\0') path = "/bin:/usr/bin";
    char *mutable = strdup(path);
    if (!mutable) {
        perror("strdup");
        return true; // Avoid false negatives on allocation failure.
    }
    bool found = false;
    char *save = NULL;
    for (char *dir = strtok_r(mutable, ":", &save); dir != NULL;
        dir = strtok_r(NULL, ":", &save)) {
        if (*dir == '\0') dir = ".";
        char candidate[PATH_MAX];
        if (snprintf(candidate, sizeof candidate, "%s/%s", dir, cmd)
                        >= (int)sizeof candidate)
            continue;
        if (access(candidate, X_OK) == 0) {
            found = true;
            break;
        }
    }
    free(mutable);
    return found;
}

static int apply_priority_if_needed(const ExecSpec *spec) {
    if (!spec || !spec->has_priority) return 0;
    errno = 0;
    if (setpriority(PRIO_PROCESS, 0, spec->priority) == -1 && errno != 0) {
        if (errno == EPERM || errno == EACCES) {
            fprintf(stderr, "setpriority: Permission denied (continuing with current priority)\n");
            return 0;
        }
        perror("setpriority");
        return -1;
    }
    return 0;
}

static const char *signal_name(int sen);

static const char *status_to_string(enum Status status) {
    switch (status) {
        case FINISHED:  return "FINISHED";
        case STOPPED:   return "STOPPED";
        case SIGNALED:  return "SIGNALED";
        case ACTIVE:    return "ACTIVE";
    }
    return "UNKNOWN";
}

static void format_launch_time(time_t t, char *buf, size_t n) {
    if (!buf || n == 0) return;
    struct tm tmp;
    localtime_r(&t, &tmp);
    strftime(buf, n, "%Y-%m-%d %H:%M:%S", &tmp);
}

static void print_process(const ProcessInfo *info) {
    if (!info) return;
    char launch[64];
    format_launch_time(info->launch_time, launch, sizeof launch);
    int priority = info->priority;
    errno = 0;
    int current = getpriority(PRIO_PROCESS, (id_t)info->pid);
    if (errno == 0) priority = current;
    printf("PID=%d | start=%s | status=%s",
            info->pid, launch, status_to_string(info->status));
    if (info->status == FINISHED) {
        printf(" (code=%d)", info->exit_status);
    } else if (info->status == SIGNALED || info->status == STOPPED) {
        const char *name = signal_name(info->signal_number);
        printf(" (signal=%s)", name);
    }
    printf(" | pri=%d | cmd=%s\n", priority, info->command_line);
}

static size_t remove_jobs_by_status(enum Status status) {
    if (!background_processes) return 0;
    size_t removed = 0;
    Node *node = background_processes->head;
    while (node) {
        Node *next = node->next;
        ProcessInfo *info = (ProcessInfo *)node->data;
        if (info && info->status == status) {
            if (node->prev) node->prev->next = node->next;
            else background_processes->head = node->next;
            if (node->next) node->next->prev = node->prev;
            destroy_process_info(info);
            free(node);
            removed++;
        }
        node = next;
    }
    return removed;
}

static struct SEN sigstrnum[]={
	{"HUP",    SIGHUP},
	{"INT",    SIGINT},
	{"QUIT",   SIGQUIT},
	{"ILL",    SIGILL},
	{"TRAP",   SIGTRAP},
	{"ABRT",   SIGABRT},
	{"IOT",    SIGIOT},
	{"BUS",    SIGBUS},
	{"FPE",    SIGFPE},
	{"KILL",   SIGKILL},
	{"USR1",   SIGUSR1},
	{"SEGV",   SIGSEGV},
	{"USR2",   SIGUSR2},
	{"PIPE",   SIGPIPE},
	{"ALRM",   SIGALRM},
	{"TERM",   SIGTERM},
	{"CHLD",   SIGCHLD},
	{"CONT",   SIGCONT},
	{"STOP",   SIGSTOP},
	{"TSTP",   SIGTSTP},
	{"TTIN",   SIGTTIN},
	{"TTOU",   SIGTTOU},
	{"URG",    SIGURG},
	{"XCPU",   SIGXCPU},
	{"XFSZ",   SIGXFSZ},
	{"VTALRM", SIGVTALRM},
	{"PROF",   SIGPROF},
	{"WINCH",  SIGWINCH},
	{"IO",     SIGIO},
	{"SYS",    SIGSYS},
#ifdef SIGPOLL
	{"POLL", SIGPOLL},
#endif
#ifdef SIGPWR
	{"PWR", SIGPWR},
#endif
#ifdef SIGEMT
	{"EMT", SIGEMT},
#endif
#ifdef SIGINFO
	{"INFO", SIGINFO},
#endif
#ifdef SIGSTKFLT
	{"STKFLT", SIGSTKFLT},
#endif
#ifdef SIGCLD
	{"CLD", SIGCLD},
#endif
#ifdef SIGLOST
	{"LOST", SIGLOST},
#endif
#ifdef SIGCANCEL
	{"CANCEL", SIGCANCEL},
#endif
#ifdef SIGTHAW
	{"THAW", SIGTHAW},
#endif
#ifdef SIGFREEZE
	{"FREEZE", SIGFREEZE},
#endif
#ifdef SIGLWP
	{"LWP", SIGLWP},
#endif
#ifdef SIGWAITING
	{"WAITING", SIGWAITING},
#endif
    {NULL,-1},
};

static const char *signal_name(int sen) {
    int i;
    for (i = 0; sigstrnum[i].nombre != NULL; i++)
        if (sen == sigstrnum[i].senal) return sigstrnum[i].nombre;
    return ("SIGUNKNOWN");
}

/* Parse a numeric uid safely; returns false on invalid input. */
static bool parse_uid_value(const char *s, uid_t *out_uid) {
    if (!s || *s == '\0') return false;
    errno = 0;
    char *end = NULL;
    long val = strtol(s, &end, 10);
    if (errno != 0 || *end != '\0' || val < 0) return false;
    uid_t uid = (uid_t)val;
    if ((long)uid != val) return false; // detect overflow
    if (out_uid) *out_uid = uid;
    return true;
}

int cmd_uid(int argc, char *argv[]){
    if (argc == 2 && strcmp(argv[1], "-get") == 0){
        uid_t ruid = getuid();
        uid_t euid = geteuid();
        gid_t rgid = getgid();
        gid_t egid = getegid();
        printf("UID real:      %d\n", ruid);
        printf("UID efectivo:  %d\n", euid);
        printf("GID real:      %d\n", rgid);
        printf("GID efectivo:  %d\n", egid);
        return 0;
    }
    if (argc == 4 && strcmp(argv[1], "-set") == 0 && strcmp(argv[2], "-l") == 0){
        const char *name = argv[3];
        struct passwd *pw = getpwnam(name);
        if (!pw) {
            fprintf(stderr, "User '%s' not found\n", name); return 1;
        }
        if (initgroups(pw->pw_name, pw->pw_gid) != 0) {
            perror("initgroups"); return 1;
        }
        if (setgid(pw->pw_gid) != 0) {
            perror("setgid"); return 1;
        }
        if (setuid(pw->pw_uid) != 0) {
            perror("setuid"); return 1;
        }
        printf("Login credentials set for user '%s':\n", pw->pw_name);
        printf("  UID = %d\n", pw->pw_uid);
        printf("  GID = %d\n", pw->pw_gid);
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "-set") == 0){
        uid_t new_euid = 0;
        if (!parse_uid_value(argv[2], &new_euid)) {
            fprintf(stderr, "Invalid UID '%s'\n", argv[2]);
            return 1;
        }
        if (seteuid(new_euid) != 0) {
            perror("seteuid"); return 1;
        }
        printf("EUID changed to %d\n", new_euid);
        return 0;
    }
    fprintf(stderr, "Usage: uid -get | uid -set [-l] id\n");
    return 1;
}

int cmd_exec(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: exec progspec\n");
        return 1;
    }
    ExecSpec spec;
    if (parse_progspec_tokens(argc - 1, &argv[1], false, &spec) != 0) {
        return 1;
    }
    if (!command_exists_in_path(spec.argv[0])) {
        fprintf(stderr, "%s: command not found\n", spec.argv[0]);
        return 127;
    }
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        if (apply_priority_if_needed(&spec) != 0) _exit(1);
        execvp(spec.argv[0], spec.argv);
        perror("execvp");
        _exit(1);
    }
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return 1;
    }
    if (WIFSIGNALED(status)) {
        fprintf(stderr, "%s terminated by signal %s\n",
                spec.argv[0], signal_name(WTERMSIG(status)));
        return 1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 0;
}

int cmd_jobs(int argc, char *argv[]) {
    (void)argv;
    if (argc != 1) {
        fprintf(stderr, "Usage: jobs\n");
        return 1;
    }
    refresh_background_processes();
    if (!background_processes || background_processes->head == NULL) {
        printf("No background jobs.\n");
        return 0;
    }
    for (Node *node = background_processes->head; node; node = node->next) {
        ProcessInfo *info = (ProcessInfo *)node->data;
        print_process(info);
    }
    return 0;
}

int cmd_deljobs(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: deljobs -term|-sig\n");
        return 1;
    }
    refresh_background_processes();
    size_t removed = 0;
    if (strcmp(argv[1], "-term") == 0) {
        removed = remove_jobs_by_status(FINISHED);
    } else if (strcmp(argv[1], "-sig") == 0) {
        removed = remove_jobs_by_status(SIGNALED);
    } else {
        fprintf(stderr, "Usage: deljobs -term|-sig\n");
        return 1;
    }
    printf("%zu job(s) removed\n", removed);
    return 0;
}

int execute_external_command(int argc, char *argv[]) {
    if (argc <= 0) return 0;
    ExecSpec spec;
    if (parse_progspec_tokens(argc, argv, true, &spec) != 0) return 1;
    refresh_background_processes();
    if (!command_exists_in_path(spec.argv[0])) {
        fprintf(stderr, "%s: command not found\n", spec.argv[0]);
        return 127;
    }
    char command_line[MAX_COMMAND];
    build_command_line(command_line, sizeof command_line, argc, argv);
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        if (apply_priority_if_needed(&spec) != 0) _exit(1);
        execvp(spec.argv[0], spec.argv);
        perror("execvp");
        _exit(1);
    }
    if (spec.background) {
        if (add_background_process(pid, command_line, &spec) == 0) {
            printf("Started background job PID %d: %s\n", pid, command_line);
        }
        return 0;
    }
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return 1;
    }
    if (WIFSIGNALED(status)) {
        fprintf(stderr, "%s terminated by signal %s\n",
                spec.argv[0], signal_name(WTERMSIG(status)));
        return 1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 0;
}
