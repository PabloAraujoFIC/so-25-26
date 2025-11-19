// Pablo Araújo Rodríguez   pablo.araujo@udc.es
// Uriel Liñares Vaamonde   uriel.linaresv@udc.es

#ifndef COMANDOS_H
#define COMANDOS_H

#include <stdbool.h>
#include <string.h>
#include "lista.h"
#include "p2.h"
#include "memoria.h"

/* forward declaration: no incluye p0.h aquí */
struct Shell;

typedef int (*command_fn)(int argc, char *argv[], struct Shell *sh);

typedef struct {
    const char *name;
    command_fn  func;
    const char *help;
} command_entry;

typedef struct tItemH{
    int id;
    char *name;
}tItemH;

// Lee un comando
char *readCommand(List* LH, char *command, bool read, struct Shell *sh);

// Trocea la entrada por teclado en argumentos
int chopString(char * argc, char * argv[]);

// Procesa el comando y ejecuta
int processCommand(int argc, char *argv[], struct Shell *sh);

// Lista el historial de comandos
void listarHistorialDeComandos(const List *LH);

// Procesa el historial de comandos
int cmd_historic(int argc, char *argv[], struct Shell *sh);

// Vacía el historial de comandos
void historic_clear(Shell *sh);

// Borra un comando del historial de comandos
void delHist(void *item);

// Proporciona información sobre los comandos disponibles
int cmd_help(int argc, char *argv[], struct Shell *sh);
#endif //COMANDOS_H
