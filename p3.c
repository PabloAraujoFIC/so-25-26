// Pablo Araújo Rodríguez   pablo.araujo@udc.es
// Uriel Liñares Vaamonde   uriel.linaresv@udc.es

#define _GNU_SOURCE
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include "p3.h"
#include "comandos.h"
#include "ficheros.h"

static char **g_envp = NULL;
static bool shell_should_exit = false;
static char *command_buffer = NULL;
static bool cleanup_done = false;
static volatile sig_atomic_t sigint_received = 0;
static void shell_cleanup(void);
static void handle_sigint(int sig);

void print_logo(void) {
    printf("\n");
    printf("==============================================================\n");
    printf("    _____ _          _  _     _____  ____                     \n");
    printf("   / ____| |        | || |   / ____|/ __ \\                   \n");
    printf("  | (___ | |__   ___| || |  | (___ | |  | |                   \n");
    printf("   \\___ \\| '_ \\ / _ \\ || |   \\___ \\| |  | |             \n");
    printf("   ____) | | | |  __/ || |   ____) | |__| |                   \n");
    printf("  |_____/|_| |_|\\___|_||_|  |_____/ \\____/                  \n");
    printf("==============================================================\n");
    printf("                        SHELL SO 25/26                        \n");
    printf("--------------------------------------------------------------\n");
    printf("   Authors:\n");
    printf("      > Pablo Araújo Rodríguez\n");
    printf("      > Uriel Linares Vaamonde\n");
    printf("==============================================================\n");
    printf("\n");
}

// Llamada UNA sola vez desde main
void shell_set_envp(char **envp) {
    g_envp = envp;
}

static void shell_cleanup(void) {
    if (cleanup_done) return;
    cleanup_done = true;
    commands_shutdown();
    ficheros_shutdown();
    procesos_destroy();
    mem_cleanup();
    free(command_buffer);
    command_buffer = NULL;
}

static void handle_sigint(int sig) {
    (void)sig;
    sigint_received = 1;
    ssize_t unused = write(STDOUT_FILENO, "\n", 1);
    (void)unused;
}

int main(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    atexit(shell_cleanup);
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    shell_set_envp(envp);
    print_logo();
    loop(envp);
    return 0;
}

void prompt()
{
    printf("\n$");
}

void loop(char *env[]) {
    (void)env;
    shell_should_exit = false;
    commands_init();
    ficheros_init();
    procesos_init();

    char *command = malloc(MAX_COMMAND * sizeof(char));
    if (command == NULL) {
        perror("Error allocating memory for command");
        shell_cleanup();
        return;
    }
    command_buffer = command;

    while (!shell_should_exit) {
        prompt();
        char *tempCommand = readCommand(command, true);
        if (tempCommand != NULL) {
            if (tempCommand != command) {
                free(command);
                command = tempCommand;
                command_buffer = command;
            }
            char *argv[MAX_TR] = {0};
            const int argc = chopString(command, argv);
            processCommand(argc, argv);
        } else {
            perror("Error reading command");
            shell_should_exit = true;
        }
    }
    printf("Goodbye :3\n");
    shell_cleanup();
}

int cmd_authors(int argc, char *argv[])
{
    (void)argc; (void)argv;
    if (argc == 1)
    {
        printf("Authors:\n\tPablo Araujo Rodriguez\tpablo.araujo@udc.es\n\t"
                "Uriel Liñares Vaamonde\turiel.linaresv@udc.es\n");
        return 0;
    }
    if (strcmp(argv[1], "-l")==0)
    {
        printf("Logins:\n\tpablo.araujo@udc.es\n\turiel.linaresv@udc.es");
        return 0;
    }
    if (strcmp(argv[1], "-n")==0)
    {
        printf("Author's names:\n\tPablo Araujo Rodriguez\n\tUriel Liñares"
                " Vaamonde\n");
        return 0;
    }
    perror("Invalid arguments for 'authors'");
    return 1;
}

int cmd_date(int argc, char *argv[])
{
    (void)argc; (void)argv;
    const time_t now = time(NULL);  // Obtenemos el tiempo actual

    // Verifica si time() falló
    if (now == (time_t)-1) {
        perror("Error getting the current time");
        return 1;
    }

    const struct tm *local = localtime(&now);  // Convierte a tiempo local

    // Verifica si localtime() falló
    if (local == NULL) {
        perror("Error converting time to local time");
        return 1;
    }

    const int hours = local->tm_hour;       // obtenemos las horas (0-23)
    const int min = local->tm_min;          // obtenemos los minutos (0-59)
    const int sec = local->tm_sec;          // obtenemos los segundos (0-59)

    const int day = local->tm_mday;         // obtenemos el día del mes (1 a 31)
    const int month = local->tm_mon + 1;    // obtenemos el mes del año (0 a 11)
    const int year = local->tm_year + 1900; // obtenemos el año desde 1900

    if (strcmp(argv[0], "hour") == 0) {
        if (argc != 1){ perror("Invalid arguments for hour"); return 1; }
        printf("%02d:%02d:%02d\n", hours, min, sec);
        return 0;
    }
    if (argc == 1) {
        printf("%02d:%02d:%02d\n", hours, min, sec);
        printf("%02d/%02d/%04d\n", day, month, year);
        return 0;
    }
    if (strcmp(argv[1], "-d") == 0) {
        printf("%02d/%02d/%04d\n", day, month, year);
        return 0;
    }
    if (strcmp(argv[1], "-t") == 0) {
        printf("%02d:%02d:%02d\n", hours, min, sec);
        return 0;
    }
    perror("Invalid arguments for 'date'");
    return 1;
}

int cmd_infosys(int argc, char *argv[])
{
    (void)argc; (void)argv;
    if(argc == 1)
    {
        struct utsname systemData;
        uname(&systemData);
        printf("%s (%s), OS: %s-%s-%s\n", systemData.nodename,
                systemData.machine, systemData.sysname, systemData.release,
                systemData.version);
        return 0;
    }

    perror("Invalid arguments for 'infosys'");
    return 1;
}

int cmd_getpid(int argc, char *argv[])
{
    (void)argc; (void)argv;
    if(argc == 1) printf("Shell pid: %d\n", getpid());
    else if (strcmp(argv[1], "-p")==0)
        printf("Pid of the shell's parent: %d\n", getppid());
    else perror("Invalid arguments for 'pid'");
    return 0;
}

int cmd_clear(int argc, char *argv[])
{
    (void)argv;
    if (argc != 1) {
        perror("Invalid arguments for 'cls'"); return 1;
    }
    printf("\033[H\033[J");
    fflush(stdout);
    return 0;
}

int cmd_cwd(int argc, char *argv[])
{
    (void)argc; (void)argv;
    if (argc == 1)
    {
        char currentDir[200];
        printf("%s\n", getcwd(currentDir, sizeof(currentDir)));
        return 0;
    }
    perror("Invalid arguments for 'cwd'");
    return 1;
}

int cmd_cd(int argc, char *argv[])
{
    (void)argc; (void)argv;
    if (argc == 1)
    {
        cmd_cwd(argc, argv);
        return 0;
    }
    if (chdir(argv[1]) == -1)
    {
        perror("Unable to change directory");
        return 1;
    }
    return 0;
}

int cmd_exit (int argc, char *argv[])
{
    (void)argv;
    if (argc == 1) {
        shell_should_exit = true;
        return 0;
    }
    fprintf(stderr, "Invalid arguments for 'exit/quit/bye'\n");
    return 1;
}

static char *find_in_env_array(char **env, const char *name) {
    if (!env || !name) return NULL;
    size_t len = strlen(name);
    for (int i = 0; env[i] != NULL; i++)
        if (strncmp(env[i], name, len) == 0 && env[i][len] == '=')
            return env[i] + len + 1;
    return NULL;
}

static char **find_entry_in_env_array(char **env, const char *name) {
    if (!env || !name) return NULL;
    size_t len = strlen(name);
    for (int i = 0; env[i] != NULL; i++)
        if (strncmp(env[i], name, len) == 0 && env[i][len] == '=')
            return &env[i];
    return NULL;
}

static int change_envvar_in_array(char **env, const char *varname, const
                                    char *newval, const char *who) {
    char **entry = find_entry_in_env_array(env, varname);
    if (!entry) {
        fprintf(stderr, "Variable %s not found in %s, not creating it\n", varname, who);
        return -1;
    }
    size_t len = strlen(varname) + 1 + strlen(newval) + 1;
    char *buf = malloc(len);
    if (!buf) {
        perror("malloc"); return -1;
    }
    snprintf(buf, len, "%s=%s", varname, newval);
    *entry = buf;
    return 0;
}

static int change_envvar_putenv(const char *varname, const char *newval) {
    size_t len = strlen(varname) + 1 + strlen(newval) + 1;
    char *buf = malloc(len);
    if (!buf) {
        perror("malloc");
        return -1;
    }
    snprintf(buf, len, "%s=%s", varname, newval);
    if (putenv(buf) != 0) {
        perror("putenv");
        return -1;
    }
    return 0;
}

static void show_envvar(const char *varname){
    char *val_getenv = getenv(varname);
    printf("== Access with getenv() ==\n");
    if (val_getenv) {
        printf("Value    : %s\n", val_getenv);
        printf("Address  : %p\n\n", (void *)val_getenv);
    } else printf("Variable %s is undefined (getenv)\n\n", varname);

    printf("== Access with g_envp ==\n");
    if (g_envp) {
        char *val_envp = find_in_env_array(g_envp, varname);
        if (val_envp) {
            printf("Value    : %s\n", val_envp);
            printf("Address  : %p\n\n", (void *)val_envp);
        } else printf("Variable %s is not in g_envp\n\n", varname);
    } else printf("g_envp is not initialized\n\n");

    printf("== Access with extern char **environ ==\n");
    char *val_environ = find_in_env_array(environ, varname);
    if (val_environ) {
        printf("Value    : %s\n", val_environ);
        printf("Address  : %p\n\n", (void *)val_environ);
    } else printf("Variable %s is not in environ\n\n", varname);
}

static void print_env_array(char **env, const char *who) {
    if (!env) { printf("%s is NULL\n", who); return; }
    printf("=== Environment via %s ===\n", who);
    for (int i = 0; env[i] != NULL; i++)
        printf("[%3d] (%p) %s\n", i, (void *)env[i], env[i]);
    printf("\n");
}

static void show_env_pointers(void) {
    printf("=== Addresses of environment pointers ===\n\n");
    printf("g_envp (copy of main's third arg)\n");
    printf("  value   (char **): %p\n", (void *)g_envp);
    printf("  address(&g_envp): %p\n", (void *)&g_envp);
    if (g_envp && g_envp[0])
        printf("  first element g_envp[0]: %p -> %s\n\n",
               (void *)g_envp[0], g_envp[0]);
    else printf("  g_envp is NULL or empty\n\n");
    printf("environ (extern char **environ)\n");
    printf("  value    (char **): %p\n", (void *)environ);
    printf("  address (&environ): %p\n", (void *)&environ);
    if (environ && environ[0])
        printf("  first element environ[0]: %p -> %s\n\n",
               (void *)environ[0], environ[0]);
    else printf("  environ is NULL or empty\n\n");
}

static int change_envvar(int argc, char *argv[]){
    char mode = 'p';
        const char *varname = NULL;
        const char *newval  = NULL;
        if (argc == 5) {
            if      (strcmp(argv[2], "-a") == 0) mode = 'a';
            else if (strcmp(argv[2], "-e") == 0) mode = 'e';
            else if (strcmp(argv[2], "-p") == 0) mode = 'p';
            else {
                fprintf(stderr, "Unknown flag '%s'. Use -a, -e or -p.\n", argv[2]);
                return 1;
            }
            varname = argv[3];
            newval  = argv[4];
        } else if (argc == 4) {
            mode    = 'p';
            varname = argv[2];
            newval  = argv[3];
        } else {
            fprintf(stderr, "Usage: %s -change [-a|-e|-p] VARNAME VALUE\n", argv[0]);
            return 1;
        }
        int rc = 0;
        switch (mode) {
        case 'a':
            if (!g_envp) {
                fprintf(stderr, "g_envp is not initialized\n");
                return 1;
            }
            rc = change_envvar_in_array(g_envp, varname, newval, "g_envp");
            break;
        case 'e':
            rc = change_envvar_in_array(environ, varname, newval, "environ");
            break;
        case 'p':
            rc = change_envvar_putenv(varname, newval);
            break;
        }
        if (rc != 0) {
            perror("change envvar");
            return 1;
        }
        show_envvar(varname);
        return 0;
}

int cmd_envvar(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  %s -show VARNAME\n", argv[0]);
        fprintf(stderr, "  %s -change [-a|-e|-p] VARNAME VALUE\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "-show") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s -show VARNAME\n", argv[0]); return 1;
        }
        const char *varname = argv[2];
        show_envvar(varname);
        return 0;
    }
    if (strcmp(argv[1], "-change") == 0) {
        return change_envvar(argc, argv);
    }

    fprintf(stderr, "Unknown subcommand '%s'\n", argv[1]);
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s -show VARNAME\n", argv[0]);
    fprintf(stderr, "  %s -change [-a|-e|-p] VARNAME VALUE\n", argv[0]);
    return 1;
}

int cmd_showenv(int argc, char *argv[]) {
    if (argc == 1) {
        print_env_array(g_envp, "g_envp (main's third arg)"); return 0;
    }
    if (argc == 2) {
        if (strcmp(argv[1], "-environ") == 0) {
            print_env_array(environ, "environ (extern char **)");
            return 0;
        }
        if (strcmp(argv[1], "-addr") == 0) {
            show_env_pointers();
            return 0;
        }
        fprintf(stderr, "Usage: %s [-environ|-addr]\n", argv[0]);
        return 1;
    }
    fprintf(stderr, "Usage: %s [-environ|-addr]\n", argv[0]);
    return 1;
}
