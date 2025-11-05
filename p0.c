// Pablo Araújo Rodríguez   pablo.araujo@udc.es
// Uriel Liñares Vaamonde   uriel.linaresv@udc.es

#include "p0.h"
#include "comandos.h"
#include "ficheros.h"

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

int main()
{
    char *env[MAX_COMMAND];
    print_logo();
    loop(env);
    return 0;
}

void prompt()
{
    printf("\n$");
}

void loop(char *env[]) {
    (void)env;
    Shell sh;
    List *LF = createList();
    List *LH = createList();

    sh.LF = LF;
    sh.LH = LH;
    sh.command_count = 0;
    sh.end = false;

    insertItem(LF, make_itemF(STDIN_FILENO,  "stdin",  O_RDONLY));
    insertItem(LF, make_itemF(STDOUT_FILENO, "stdout", O_WRONLY));
    insertItem(LF, make_itemF(STDERR_FILENO, "stderr", O_WRONLY));

    char *command = malloc(MAX_COMMAND * sizeof(char));
    if (command == NULL) {
        perror("Error allocating memory for command");
        return;
    }
    while (!sh.end) {
        prompt();
        char *tempCommand = readCommand(LH, command, true, &sh);
        if (tempCommand != NULL) {
            if (tempCommand != command) {
                free(command);
                command = tempCommand;
            }
            char *argv[MAX_TR] = {0};
            const int argc = chopString(command, argv);
            processCommand(argc, argv, &sh);
        } else {
            perror("Error reading command");
            sh.end = true;
        }
    }
    printf("Goodbye :3\n");
    free(command);
    deleteList((List*)LH, delHist);
    deleteList((List*)LF, delFile);
    mem_cleanup();
}

int cmd_authors(int argc, char *argv[], Shell *sh)
{
    // Al principio de cada función:
    (void)argc; (void)argv; (void)sh;
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

int cmd_date(int argc, char *argv[], Shell *sh)
{
    (void)argc; (void)argv; (void)sh;
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

int cmd_infosys(int argc, char *argv[], Shell *sh)
{
    (void)argc; (void)argv; (void)sh;
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

int cmd_getpid(int argc, char *argv[], Shell *sh)
{
    (void)argc; (void)argv; (void)sh;
    if(argc == 1) printf("Shell pid: %d\n", getpid());
    else if (strcmp(argv[1], "-p")==0)
        printf("Pid of the shell's parent: %d\n", getppid());
    else perror("Invalid arguments for 'pid'");
    return 0;
}

int cmd_clear(int argc, char *argv[], Shell *sh)
{
    (void)argv; (void)sh;
    if (argc != 1) {
        perror("Invalid arguments for 'cls'"); return 1;
    }
    printf("\033[H\033[J");
    fflush(stdout);
    return 0;
}

int cmd_cwd(int argc, char *argv[], Shell *sh)
{
    (void)argc; (void)argv; (void)sh;
    if (argc == 1)
    {
        char currentDir[200];
        printf("%s\n", getcwd(currentDir, sizeof(currentDir)));
        return 0;
    }
    perror("Invalid arguments for 'cwd'");
    return 1;
}

int cmd_cd(int argc, char *argv[], Shell *sh)
{
    (void)argc; (void)argv; (void)sh;
    if (argc == 1)
    {
        cmd_cwd(argc, argv, sh);
        return 0;
    }
    if (chdir(argv[1]) == -1)
    {
        perror("Unable to change directory");
        return 1;
    }
    return 0;
}

int cmd_exit (int argc, char *argv[], Shell *sh)
{
    (void)argc; (void)argv; (void)sh;
    if (argv[1]==NULL)
    {
        sh->end = true;
        return 0;
    }
    perror("Invalid arguments for 'quit'");
    return 1;
}
