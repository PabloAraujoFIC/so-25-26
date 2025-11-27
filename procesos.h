#ifndef PROCESOS_H
#define PROCESOS_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <limits.h>

#include "p3.h"
#include "memoria.h"
#include "lista.h"
#define MAXVAR 1024

void procesos_init(void);
void procesos_destroy(void);
void procesos_refresh(void);
void procesos_cleanup(void);

struct SEN {
    char *nombre;
    int senal;
};

enum Status {
    FINISHED, STOPPED, SIGNALED, ACTIVE
};

typedef struct {
    pid_t pid;                      // Process identifier
    time_t launch_time;             // Time the process started
    enum Status status;             // FINISHED, STOPPED, SIGNALED, ACTIVE
    int exit_status;                // Valid when FINISHED
    int signal_number;              // Valid when STOPPED/SIGNALED
    char command_line[MAX_COMMAND]; // Original command line
    int priority;                   // Scheduling priority
} ProcessInfo;

typedef struct {
    char *argv[MAX_TR];
    int argc;
    bool background;
    bool has_priority;
    int priority;
} ExecSpec;

int cmd_fork(int argc, char *argv[]);
int cmd_uid(int argc, char *argv[]);
int cmd_exec(int argc, char *argv[]);
int cmd_jobs(int argc, char *argv[]);
int cmd_deljobs(int argc, char *argv[]);
int execute_external_command(int argc, char *argv[]);

#endif //PROCESOS_H
