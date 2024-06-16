#include "server.h"

int server_sockfd = -1;

int create_tcp_socket(int domain) {
    // 0 on protocol as only choice is TCP, but could also be IPPROTO_TCP
    int sockfd = socket(domain, SOCK_STREAM, 0); 

    if (sockfd == -1) {
        fprintf(stderr, "FATAL ERROR: couldn't create socket. Error: %s\n", strerror(errno));
        return -1;
    }

    return sockfd;
}

void terminate(int sig) {
    close_clients();
    free_log_buffer();

    if (server_sockfd != -1) {
        if (close(server_sockfd) != 0) {
            fprintf(stderr, "ERROR: couldn't close server socket. Error: %s.\n", strerror(errno));
        }
    }

    if (application_context != NULL) {
        if (application_context->stats_filename != NULL) {
            show_stats(application_context->stats_filename);
        }   
    }

    free_application_context();

    terminate_log_file_writer();
    if (log_file != NULL) {
        fclose(log_file);
    }

    // using _exit since exit is not async signal safe, and 'terminate' is called on SIGUSR1
    if (sig == EXIT_FAILURE) {
        _exit(EXIT_FAILURE);
    }

    _exit(EXIT_SUCCESS);
}