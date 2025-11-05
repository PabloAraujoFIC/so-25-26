// Pablo Araújo Rodríguez   pablo.araujo@udc.es
// Uriel Liñares Vaamonde   uriel.linaresv@udc.es

#define _POSIX_C_SOURCE 200809L
#include "comandos.h"
#include "p0.h"
#include "ficheros.h"

// Tabla de comandos
static command_entry commands[] = {
    {"authors", cmd_authors, "Prints the names and logins of the program authors.\n"
        "\tauthors -l\tPrints only the logins.\n\tauthors -n\tPrints only the "
        "names"},
    {"bye", cmd_exit, "Ends the shell"},
    {"chdir", cmd_cd, "Changes the current working directory of the shell to dir. "
        "When invoked without auguments it prints the current working directory"
        "."},
    {"close", cmd_close, "Closes the df file descriptor and eliminates the "
        "corresponding item from the list"},
    {"cls", cmd_clear, "Clears the shell screen."},
    {"create", cmd_create, "\n\tcreate -f 'name':\tCreates a file\n\tcreate 'name':"
        "\t\tCreates a directory"},
    {"date", cmd_date, "Prints the current date in the format DD/MM/YYYY and the "
        "current time in the format hh:mm:ss.\n\t\tdate -d\tPrints the current "
        "date in the format DD/MM/YYYY\n\t\tdate -t\tPrints and the current "
        "time in the format hh:mm:ss."},
    {"delrec", cmd_delrec, "Deletes a file or directory recursively"},
    {"dir", cmd_dir, "dir [-long|-short] [-link|-nolink] [-d] [hid|nohid] "
        "[reca|recb|norec] [n1 n2 ...] Shows info for files/dirs;\n\t-d\t\t"
        "lists directory contents;\n\thid/nohid\tincludes hidden; \n\t"
        "reca/recb/norec\tcontrols recursion."},
    {"dup", cmd_dup, "Duplicates the df file descriptor"},
    {"erase", cmd_erase, "erase 'name':\tErases the empty files or directories "
        "specified by 'name'"},
    {"exit", cmd_exit, "Ends the shell"},
    {"free", cmd_free, "free addr: releases the block associated with addr"},
    {"getcwd", cmd_cwd, "Prints the current working directory of the shell"},
    {"getdirparams", cmd_getdirparams, "Shows the value of the parameters for "
        "listing with dir"},
    {"getpid", cmd_getpid,"Prints the pid of the process executing the shell."},
    {"help", cmd_help, "help displays a list of available commands. help cmd gives "
        "a brief help on the usage of command cmd"},
    {"historic", cmd_historic, "Shows the history of commands executed by this "
        "shell.\n\t– historic\t\tPrints all the commands that have been input "
        "with their order number\n\t– historic N\t\tRepeats command number N"
        "\n\t– historic -N\t\tPrints only the last N commands\n  historic "
        "[-clear|-count]\tClears the history list or reports its number of "
        "elements\n\t– historic -count\tReports how many commands there are in "
        "the history list\n\t– historic -clear\tClears the history list"},
    {"hour", cmd_date, "Prints and the current time in the format hh:mm:ss." },
    {"infosys", cmd_infosys, "Prints information on the machine running the shell"},
    {"listopen", cmd_listOpen,"Lists the shell open files"},
    {"lseek", cmd_lseek, "lseek df offset whence: Repositions the offset of the"
        " file descriptor df to the argument offset according to the directive "
        "whence (SEEK_SET, SEEK_CUR or SEEK_END)"},
    {"malloc", cmd_malloc, "malloc n: allocates n bytes; without arguments it lists tracked malloc blocks"},
    {"mem", cmd_mem, "mem -funcs | -vars | -blocks | -all | -pmap: prints memory information (addresses, tracked blocks, and process map)"},
    {"memdump", cmd_memdump, "memdump addr count: dumps count bytes starting at addr in hexadecimal and printable form"},
    {"memfill", cmd_memfill, "memfill addr count byte: fills count bytes at addr with the byte value"},
    {"mmap", cmd_mmap, "mmap file perms: maps the file; mmap -free file: unmaps an active mapping"},
    {"open", cmd_open, "Opens a file and adds it. Open without arguments lists "
        "the shell open files. For each file it lists its descriptor, the file "
        "name and the opening mode."},
    {"pid", cmd_getpid,"Prints the pid of the process executing the shell."},
    {"pwd", cmd_cwd, "Prints the current working directory of the shell"},
    {"quit", cmd_exit, "Ends the shell"},
    {"read", cmd_read, "read fd addr count: reads count bytes from descriptor fd into addr"},
    {"readfile", cmd_readfile, "readfile file addr [count]: reads bytes from file into addr"},
    {"recurse", cmd_recurse, "Executes the recursive function n times. The function allocates an automatic array of size 1024, a static array of size 1024, and prints the addresses of both arrays plus the parameter on each recursion level"},
    {"setdirparams", cmd_setdirparams, "setdirparams long|short | link|nolink | "
        "hid|nohid | reca|recb|norec: sets listing parameters for 'dir' "
        "(format, symlink target, hidden files, and recursion order/disable)."},
    {"shared", cmd_shared, "shared key: attaches; shared -create key size: creates and attaches; shared -free key: detaches; shared -delkey key: removes"},
    {"write", cmd_write, "write fd addr count: writes count bytes from addr to descriptor fd"},
    {"writefile", cmd_writefile, "writefile [-o] file addr count: writes bytes from memory into file"},
    {"writestr", cmd_writestr, "writestr fd str: writes the string str to the "
        "descriptor df"},
};

static const size_t n_commands = sizeof(commands) / sizeof(commands[0]);

char *readCommand(List *LH, char *command, bool read, Shell *sh)
{
    size_t len = 0;

    // Leemos el comando
    if (read) {
        if (getline(&command, &len, stdin) == -1) {
            perror("Error reading command");
            return NULL;
        }
        command[strcspn(command, "\n")] = '\0';
    }

    // Reserva en heap el item del historial
    tItemH *item = malloc(sizeof *item);
    if (!item) { perror("malloc"); return NULL; }

    // Incrementa el contador de comandos
    item->id   = sh->command_count++;
    item->name = strdup(command);
    if (!item->name) { perror("strdup"); free(item); return NULL; }

    // Insertamos el comando en el historial
    if (insertItem(LH, item) != 0) {
        free(item->name);
        free(item);
        perror("Error inserting command into history");
        return NULL;
    }
    return command;
}

int chopString(char * argc, char * argv[])
{
    int i=1;
    if ((argv[0]=strtok(argc," \n\t"))==NULL)
        return 0;
    while ((argv[i]=strtok(NULL," \n\t"))!=NULL)
        i++;
    return i;
}

static int command_cmp(const void *keyp, const void *elem) {
    const char *key = *(const char * const *)keyp;
    const command_entry *entry = (const command_entry *)elem;
    return strcmp(key, entry->name);
}


int processCommand(int argc, char *argv[], Shell *sh)
{
    (void)argc; (void)argv; (void)sh;
    if (argc < 1) return 0;
    command_entry *e = bsearch(&argv[0], commands, n_commands,
                                sizeof(commands[0]), command_cmp);
    if (e != NULL) {
        return e->func(argc, argv, sh); // Ejecuta el handler
    }
    fprintf(stderr, "Unknown command: '%s'. Use 'help'.\n", argv[0]);
    return 0;
}

int cmd_historic(int argc, char *argv[], Shell *sh) {
    (void)argc; (void)argv; (void)sh;
    if (sh->LH == NULL) {
        printf("Empty List\n");
        return 1;
    }

    // Si no se especifica parámetro, listamos el historial completo
    if (argc == 1)
    {
        listarHistorialDeComandos(sh->LH);
        return 1;
    }
    if (strcmp(argv[1], "-clear")==0)
    {
        historic_clear(sh);
        return 0;
    }
    if (strcmp(argv[1], "-count")==0)
    {
        printf("%d", sh->command_count);
        return 0;
    }

    // Convertimos el argumento a entero
    char *endptr;
    long i = strtol(argv[1], &endptr, 10);

    // Si strtol no pudo convertir el valor, mostramos un error
    if (*endptr != '\0') {
        perror("Invalid argument for historic");
        return 1;
    }

    // Si se proporciona un número negativo, mostramos los últimos n comandos
    if (i < 0) {
        int count = 0;
        for (Node *x = sh->LH->head; x != NULL && count < -i; x = x->next) {
            tItemH *y = (tItemH*)x->data;
            printf("%d\t%s\n", y->id, y->name);
            count++;
        }
        return 0;
    }

    // Si el número es positivo, buscamos el comando por su ID y lo ejecutamos
    int count = 0;
    Node *x = sh->LH->head;
    while (x != NULL && count < sh->command_count-i-1) {  // avanzar i pasos
        x = x->next;
        count++;
    }
    if (x == NULL || i > sh->command_count) {
        printf("Position %ld does not exist in historic\n", i);
        return 1;
    }
    tItemH *y = (tItemH*)x->data;

    // Evitamos el bucle infinito
    if (strcmp(y->name, "historic") == 0) {
        printf("Error: infinite loop\n");
        return 1;
    }
    printf("%d -> %s\n", y->id, y->name);
    char *argv2[MAX_TR] = {0};
    const int argc2 = chopString(y->name, argv2);
    processCommand(argc2, argv2, sh);
    return 0;
}

void listarHistorialDeComandos(const List *list) {
    if (!list) {
        puts("Empty list or null pointer.");
        return;
    }
    const Node *current = list->head;
    while (current) {
        const tItemH *it = (const tItemH*)current->data;
        if (it) {
            printf("id: %d, name: %s\n", it->id, it->name);
        }
        current = current->next;
    }
}

void delHist(void *item)
{
    if (!item) return;
    tItemH *h = (tItemH*)item;
    free(h->name);   // <- liberar el strdup
    free(h);
}

void historic_clear(Shell *sh)
{
    clearList(sh->LH, delHist);
    sh->command_count = 0;
}

static void print_help_entry(const command_entry *e) {
    printf("%s\n  %s\n", e->name, e->help);
}

static void print_all_help(void) {
    // Calcular ancho máximo del nombre para alinear
    size_t maxw = 0;
    for (size_t i = 0; i < n_commands; ++i) {
        size_t w = strlen(commands[i].name);
        if (w > maxw) maxw = w;
    }
    // Listado
    puts("Available commands:\n");
    for (size_t i = 0; i < n_commands; ++i) {
        printf("  %-*s  %s\n", (int)maxw, commands[i].name, commands[i].help);
    }
}

int cmd_help(int argc, char *argv[], Shell *sh)
{
    (void)argc; (void)argv; (void)sh;
    // Sin parámetros imprime el help entero
    if (argc == 1) {
        print_all_help();
        return 0;
    }

    // Buscamos el comando en la tabla e imprimimos su help
    int status = 0;
    for (int i = 1; i < argc; ++i) {
        command_entry *e = bsearch(&argv[i], commands, n_commands,
                                    sizeof(commands[0]), command_cmp);
        if (e) {
            print_help_entry(e);
        } else {
            fprintf(stderr, "help: commmand not found: %s\n", argv[i]);
            status = 1;
        }
    }
    return status;
}
