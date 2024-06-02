#include "server.h"
#include "clients.h"
#include "log_file_handler.h"
#include "stats_file_handler.h"
#include "response_file_handler.h"
#include "args.h"

void *client_thread(void *arg) {
    int client_sockfd = *((int*)arg);
    free(arg); // arg is heap allocated

    if (insert_client(client_sockfd) != 0) {
        fprintf(stderr, "FATAL ERROR: couldn't insert client in list, will close\n");
        close(client_sockfd);
        return NULL;
    }

    while (1) {
        fd_set readfds, writefds, exceptfds;
        FD_ZERO(&readfds);
        FD_SET(client_sockfd, &readfds);

        FD_ZERO(&writefds);
        FD_ZERO(&exceptfds);

        struct timeval timeout;
        timeout.tv_sec = 0; 
        timeout.tv_usec = 2000; // 2ms timeout

        int error = select(client_sockfd + 1, &readfds, &writefds, &exceptfds, &timeout);

        if (error == -1) {
            fprintf(stderr, "FATAL ERROR: select call failed. Error: %s\n", strerror(errno));
            return (void*)-1;
        } 

        if (FD_ISSET(client_sockfd, &readfds) != 0) {
            char data[MAX_MSG_SIZE] = {0};

            if (read(client_sockfd, data, sizeof(data)) == 0) {
                break;
            }

            time_t timestamp = time(NULL);
            struct tm *timestamp_tm = localtime(&timestamp);

            char *timestamp_str = (char*)malloc(sizeof(char) * TIMESTAMP_MSG_SIZE);
            if (timestamp_str ==  NULL) {
                fprintf(stderr, "ERROR: couldn't allocate memory for timestamp.\n");
                continue;
            }

            strftime(timestamp_str, TIMESTAMP_MSG_SIZE, "%a, %d %b %Y %T %Z", timestamp_tm);
            if (timestamp_str == NULL) {
                free(timestamp_str);
                log_message_producer((void*)"[ERROR] Missing timestamp");
                continue;
            }

            char* file_path = get_file_path(data);
            if (file_path == NULL) {
                free(timestamp_str);
                log_message_producer((void*)"[ERROR] Malformed query");
                continue;
            }

            char* message = (char*)malloc(sizeof(char) * 
                (strlen("[MESSAGE] ") + strlen(timestamp_str) + strlen(file_path) + strlen(" - ") + 1));
            if (message == NULL) {
                free(timestamp_str);
                free(file_path);
                fprintf(stderr, "ERROR: Couldn't allocate memory for message.\n");
                continue;
            }

            strcpy(message, "[MESSAGE] ");
            strcat(message, file_path);
            strcat(message, " - ");
            strcat(message, timestamp_str);

            free(timestamp_str);

            log_message_producer((void*)message);
        
            free(message);

            if (strcmp("/", file_path) == 0) {
                free(file_path);

                file_path = (char*)malloc(sizeof(char) * (strlen("/index.html") + 1));
                if (file_path == NULL) {
                    fprintf(stderr,"ERROR: couldn't allocate memory for index.html file path.\n");
                    continue;
                }
                
                strcpy(file_path, "/index.html");
            }

            // get file extension
            char *file_extension = strrchr(file_path, (int)'.');
            if (file_extension == NULL) {
                free(file_path);

                if (send(client_sockfd, http_404_response, sizeof(char) * strlen(http_404_response), 0) != sizeof(char) * strlen(http_404_response)) {
                    log_message_producer((void*)"[ERROR] Couldn't send all data");
                    continue;
                }
                
                log_message_producer("[MESSAGE] 404 Not Found");

                remove_client(client_sockfd);
                
                return NULL;    
            }
            file_extension++; // get past dot

            FileType type = get_file_type(file_extension);

            struct file_response* file_response = get_file_content(file_path);
            if (file_response == NULL) {
                free(file_path);

                if (send(client_sockfd, http_404_response, sizeof(char) * strlen(http_404_response), 0) != sizeof(char) * strlen(http_404_response)) {
                    log_message_producer((void*)"[ERROR] Couldn't send all data");
                    continue;
                }

                log_message_producer("[MESSAGE] 404 Not Found");

                remove_client(client_sockfd);
                
                return NULL; 
            }

            free(file_path);

            char *content_type = content_type_array[type]; // this needs to be used to produce the response

            char content_length_str[10] = {0};

            sprintf(content_length_str, "%ld", file_response->file_size);

            size_t http_response_len = strlen(http_ok_response_pt1) + strlen(http_ok_response_pt2) + strlen(http_ok_response_pt3)
                + strlen(content_length_str) + file_response->file_size + strlen(content_type) + 1;

            char *http_response = (char*)malloc(sizeof(char) * (http_response_len));
            if (http_response == NULL) {
                fprintf(stderr, "ERROR: couldn't allocate memory for http response.\n");
               
                free(file_response->file_content);
                free(file_response);
                
                return NULL;
            }

            memset(http_response, 0, http_response_len);

            strcpy(http_response, "");
            strcat(http_response, http_ok_response_pt1);
            strcat(http_response, content_type);
            strcat(http_response, http_ok_response_pt2);
            strcat(http_response, content_length_str);
            strcat(http_response, http_ok_response_pt3);
            memcpy(http_response + (http_response_len - file_response->file_size - 1),
                 file_response->file_content, file_response->file_size);

            if (application_context->stats_filename != NULL) {
                produce_stats_message(type);
            }

            if (send(client_sockfd, http_response, sizeof(char) * http_response_len, 0) != sizeof(char) * http_response_len) {
                log_message_producer((void*)"[ERROR] Couldn't send all data");
                continue;
            }

            free(http_response);
            free(file_response->file_content);
            free(file_response);
        }

        pthread_testcancel();
    }

    remove_client(client_sockfd);

    return NULL;
}
