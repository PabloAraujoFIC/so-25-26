#ifndef P0_H
#define P0_H
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <features.h>
#include <stdio.h>

#include "lista.h"

#define MAX_TR 100
#define MAX_COMMAND 1024
#define MAX_FILES 1024

typedef struct Shell{
    List* LH;
    List* LF;
    //List* LM;
    //List* LP;
    //List* LD;
    int command_count;
    bool end;
}Shell;

// main
int main();

// Imprime $
void prompt();

// Función principal del shell
void loop(char *env[]);

// Da información acerca de los autores del proyecto
int cmd_authors(int argc, char *argv[], Shell *sh);

// Da información acerca de la fecha y hora del sistema
int cmd_date(int argc, char *argv[], Shell *sh);

// Da información acerca del sistema
int cmd_infosys(int argc, char *argv[], Shell *sh);

// Devuelve el pid del shell o del padre
int cmd_getpid(int argc, char *argv[], Shell *sh);

// Limpia la pantalla de la shell
int cmd_clear(int argc, char *argv[], Shell *sh);

// Devuelve el directorio actual
int cmd_cwd(int argc, char *argv[], Shell *sh);

// Devuelve el directorio actual o lo cambia
int cmd_cd(int argc, char *argv[], Shell *sh);

// Termina el programa
int cmd_exit(int argc, char *argv[], Shell *sh);

#endif //P0_H
