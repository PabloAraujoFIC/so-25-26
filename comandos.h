// Pablo Araújo Rodríguez   pablo.araujo@udc.es
// Uriel Liñares Vaamonde   uriel.linaresv@udc.es

#ifndef COMANDOS_H
#define COMANDOS_H

#include <stdbool.h>
#include <string.h>
#include "lista.h"
#include "p3.h"
#include "memoria.h"
#include "ficheros.h"
#include "procesos.h"

typedef int (*command_fn)(int argc, char *argv[]);

typedef struct {
    const char *name;
    command_fn  func;
    const char *help;
} command_entry;

typedef struct tItemH{
    int id;
    char *name;
}tItemH;

void commands_init(void);
void commands_shutdown(void);

// Lee un comando y lo añade al historial
char *readCommand(char *command, bool read);

// Trocea la entrada por teclado en argumentos
int chopString(char * argc, char * argv[]);

// Procesa el comando y ejecuta
int processCommand(int argc, char *argv[]);

// Procesa el historial de comandos
int cmd_historic(int argc, char *argv[]);

// Vacía el historial de comandos
void historic_clear(void);

// Borra un comando del historial de comandos
void delHist(void *item);

// Proporciona información sobre los comandos disponibles
int cmd_help(int argc, char *argv[]);
#endif //COMANDOS_H
