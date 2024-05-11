#ifndef GRACEFUL_EXIT_H
#define GRACEFUL_EXIT_H

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>

extern int exit_status;

void graceful_exit(int status);

#endif