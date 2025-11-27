#ifndef P2_H
#define P2_H
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <features.h>
#include <stdio.h>

#define MAX_TR 100
#define MAX_COMMAND 1024
#define MAX_FILES 1024

extern char **environ;

// main
int main();

// Imprime $
void prompt();

// Función principal del shell
void loop(char *env[]);

// Da información acerca de los autores del proyecto
int cmd_authors(int argc, char *argv[]);

// Da información acerca de la fecha y hora del sistema
int cmd_date(int argc, char *argv[]);

// Da información acerca del sistema
int cmd_infosys(int argc, char *argv[]);

// Devuelve el pid del shell o del padre
int cmd_getpid(int argc, char *argv[]);

// Limpia la pantalla de la shell
int cmd_clear(int argc, char *argv[]);

// Devuelve el directorio actual
int cmd_cwd(int argc, char *argv[]);

// Devuelve el directorio actual o lo cambia
int cmd_cd(int argc, char *argv[]);

// Termina el programa
int cmd_exit(int argc, char *argv[]);

// Gestiona variables de entorno
int cmd_envvar(int argc, char *argv[]);

// Muestra el entorno de ejecución
int cmd_showenv(int argc, char *argv[]);

#endif //P2_H
