#include "graceful_exit.h"

int exit_status = EXIT_SUCCESS;

void graceful_exit(int status) {
    exit_status = status; 
    kill(getpid(), SIGUSR1);
}
