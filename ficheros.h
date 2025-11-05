// Pablo Araújo Rodríguez   pablo.araujo@udc.es
// Uriel Liñares Vaamonde   uriel.linaresv@udc.es

#ifndef PRACTICAS_FICHEROS_H
#define PRACTICAS_FICHEROS_H

#define MAX 1024
#define _POSIX_C_SOURCE 200809L

#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <pwd.h>
#include <inttypes.h>
#include <grp.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "lista.h"

struct Shell;

typedef struct tItemF{

    int fileDescriptor;
    char filename[MAX];
    int mode;

}tItemF;

typedef enum { DIR_REC_NOREC = 0, DIR_REC_RECA, DIR_REC_RECB } dir_rec_t;

typedef struct {
    bool longfmt;    /* long|short */
    bool showlink;   /* link|nolink */
    bool showhid;    /* hid|nohid */
    dir_rec_t rec;   /* norec|reca|recb */
} DirParams;

const DirParams *dirparams_get(void);

tItemF *make_itemF(int fd, const char *name, int mode);
int addFile(List *lista, const char *path, int flags);
void delFile(void *data);
char *convertMode(mode_t m, char *permisos);
char *convertMode2(mode_t m);
char *convertMode3(mode_t m);

int cmd_listOpen(int argc, char **argv, struct Shell *sh);
int cmd_open(int argc, char **argv, struct Shell *sh);
int cmd_close (int argc, char *argv[], struct Shell *sh);
int cmd_dup (int argc, char **argv, struct Shell *sh);
int cmd_create(int argc, char *argv[], struct Shell *sh);
int cmd_erase(int argc, char *argv[], struct Shell *sh);
int cmd_setdirparams(int argc, char *argv[], struct Shell *sh);
int cmd_getdirparams(int argc, char *argv[], struct Shell *sh);
int cmd_dir(int argc, char *argv[], struct Shell *sh);
int cmd_delrec(int argc, char *argv[], struct Shell *sh);
int cmd_lseek(int argc, char *argv[], struct Shell *sh);
int cmd_writestr(int argc, char *argv[], struct Shell *sh);
#endif //PRACTICAS_FICHEROS_H