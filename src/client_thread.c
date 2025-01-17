#include "server.h"

pthread_mutex_t timestamp_tm_mutex = PTHREAD_MUTEX_INITIALIZER;

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

            // Builds the response that will be sent to the log file.

            time_t timestamp = time(NULL);
            pthread_mutex_lock(&timestamp_tm_mutex);
            struct tm *timestamp_tm = (struct tm*)malloc(sizeof(struct tm));
            if (timestamp_tm == NULL) {
                fprintf(stderr, "ERROR: couldn't allocate memory for timestamp structure.\n");
                pthread_mutex_unlock(&timestamp_tm_mutex);
                continue;
            }
            localtime_r(&timestamp, timestamp_tm);
            pthread_mutex_unlock(&timestamp_tm_mutex);

            char *timestamp_str = (char*)malloc(sizeof(char) * TIMESTAMP_MSG_SIZE);
            if (timestamp_str ==  NULL) {
                fprintf(stderr, "ERROR: couldn't allocate memory for timestamp.\n");
                free(timestamp_tm);
                continue;
            }

            strftime(timestamp_str, TIMESTAMP_MSG_SIZE, "%a, %d %b %Y %H:%M:%S %Z", timestamp_tm);
            if (timestamp_str == NULL) {
                free(timestamp_str);
                free(timestamp_tm);
                log_message_producer((void*)"[ERROR] Missing timestamp");
                continue;
            }

            char* file_path = get_file_path(data);
            if (file_path == NULL) {
                free(timestamp_str);
                free(timestamp_tm);
                log_message_producer((void*)"[ERROR] Malformed query");
                continue;
            }

            char* message = (char*)malloc(sizeof(char) * 
                (strlen("[MESSAGE] ") + strlen(timestamp_str) + strlen(file_path) + strlen(" - ") + 1));
            if (message == NULL) {
                free(timestamp_str);
                free(file_path);
                free(timestamp_tm);
                fprintf(stderr, "ERROR: Couldn't allocate memory for message.\n");
                continue;
            }

            strcpy(message, "[MESSAGE] ");
            strcat(message, file_path);
            strcat(message, " - ");
            strcat(message, timestamp_str);

            log_message_producer((void*)message);
        
            free(message);

            // Now will build the response (and 404 entries in log file, if necessary)

            if (strcmp("/", file_path) == 0) {
                free(file_path);

                file_path = (char*)malloc(sizeof(char) * (strlen("/index.html") + 1));
                if (file_path == NULL) {
                    fprintf(stderr,"ERROR: couldn't allocate memory for index.html file path.\n");

                    free(timestamp_str);
                    free(timestamp_tm);
                    continue;
                }
                
                strcpy(file_path, "/index.html");
            }

            timestamp = time(NULL);
            memset(timestamp_tm, 0, sizeof(struct tm));
            gmtime_r(&timestamp, timestamp_tm);

            strftime(timestamp_str, TIMESTAMP_MSG_SIZE, "%a, %d %b %Y %H:%M:%S %Z", timestamp_tm);
            if (timestamp_str == NULL) {
                free(timestamp_str);
                free(timestamp_tm);
                free(file_path);

                log_message_producer((void*)"[ERROR] Missing timestamp");
                continue;
            }

            char *file_extension = strrchr(file_path, (int)'.');
            if (file_extension == NULL) {
                size_t http_response_404_len = strlen(http_404_response_pt1) + strlen(http_404_response_pt2) + strlen(timestamp_str) + 1;
                char* http_response_404 = (char*)malloc(sizeof(char) * http_response_404_len);

                memset(http_response_404, 0, sizeof(char) * http_response_404_len);

                strcpy(http_response_404, http_404_response_pt1);
                strcat(http_response_404, timestamp_str);
                strcat(http_response_404, http_404_response_pt2);

                if (send(client_sockfd, http_response_404, sizeof(char) * http_response_404_len, 0) != sizeof(char) * http_response_404_len) {
                    log_message_producer((void*)"[ERROR] Couldn't send all data");

                    free(http_response_404);
                    free(file_path);
                    free(timestamp_str);
                    free(timestamp_tm);

                    continue;
                }

                free(http_response_404);

                char* message_404 = (char*)malloc(sizeof(char) * 
                    (strlen("[MESSAGE] 404 Not Found - ") + strlen(timestamp_str) + strlen(file_path) + strlen(" - ") + 1));
                if (message_404 == NULL) {
                    fprintf(stderr, "ERROR: couldn't allocate memory for 404 message. Error: %s\n", strerror(errno));

                    free(file_path);
                    free(timestamp_str);
                    free(timestamp_tm);

                    continue;
                }

                timestamp = time(NULL);
                memset(timestamp_tm, 0, sizeof(struct tm));
                localtime_r(&timestamp, timestamp_tm);

                strftime(timestamp_str, TIMESTAMP_MSG_SIZE, "%a, %d %b %Y %H:%M:%S %Z", timestamp_tm);
                if (timestamp_str == NULL) {
                    free(file_path);
                    free(timestamp_str);
                    free(timestamp_tm);

                    log_message_producer((void*)"[ERROR] Missing timestamp");

                    continue;
                }

                strcpy(message_404, "[MESSAGE] 404 Not Found - ");
                strcat(message_404, file_path);
                strcat(message_404, " - ");
                strcat(message_404, timestamp_str);
                
                log_message_producer(message_404);

                free(timestamp_str);
                free(timestamp_tm);
                free(file_path);
                free(message_404);

                remove_client(client_sockfd);
                
                return NULL; 
            }

            file_extension++; // get past dot

            FileType type = get_file_type(file_extension);

            struct file_response* file_response = get_file_content(file_path, application_context->root_path);

            if (file_response == NULL) {
                size_t http_response_404_len = strlen(http_404_response_pt1) + strlen(http_404_response_pt2) + strlen(timestamp_str) + 1;
                char* http_response_404 = (char*)malloc(sizeof(char) * http_response_404_len);

                memset(http_response_404, 0, sizeof(char) * http_response_404_len);

                strcpy(http_response_404, http_404_response_pt1);
                strcat(http_response_404, timestamp_str);
                strcat(http_response_404, http_404_response_pt2);

                if (send(client_sockfd, http_response_404, sizeof(char) * http_response_404_len, 0) != sizeof(char) * http_response_404_len) {
                    log_message_producer((void*)"[ERROR] Couldn't send all data");

                    free(http_response_404);
                    free(file_path);
                    free(timestamp_str);
                    free(timestamp_tm);

                    continue;
                }

                free(http_response_404);

                char* message_404 = (char*)malloc(sizeof(char) * 
                    (strlen("[MESSAGE] 404 Not Found - ") + strlen(timestamp_str) + strlen(file_path) + strlen(" - ") + 1));
                if (message_404 == NULL) {
                    fprintf(stderr, "ERROR: couldn't allocate memory for 404 message. Error: %s\n", strerror(errno));

                    free(file_path);
                    free(timestamp_str);
                    free(timestamp_tm);

                    continue;
                }
                
                timestamp = time(NULL);
                memset(timestamp_tm, 0, sizeof(struct tm));
                localtime_r(&timestamp, timestamp_tm);

                strftime(timestamp_str, TIMESTAMP_MSG_SIZE, "%a, %d %b %Y %H:%M:%S %Z", timestamp_tm);
                if (timestamp_str == NULL) {
                    free(timestamp_str);
                    free(file_path);
                    free(timestamp_tm);

                    log_message_producer((void*)"[ERROR] Missing timestamp");

                    continue;
                }

                strcpy(message_404, "[MESSAGE] 404 Not Found - ");
                strcat(message_404, file_path);
                strcat(message_404, " - ");
                strcat(message_404, timestamp_str);
                
                log_message_producer(message_404);

                free(timestamp_str);
                free(timestamp_tm);
                free(file_path);
                free(message_404);

                remove_client(client_sockfd);
                
                return NULL; 
            }

            free(file_path);

            char *content_type = content_type_array[type]; // this needs to be used to produce the response

            char content_length_str[10] = {0};

            sprintf(content_length_str, "%ld", file_response->file_size);

            size_t http_response_len = strlen(http_ok_response_pt1) + strlen(http_ok_response_pt2) + 
                strlen(http_ok_response_pt3)+ strlen(http_ok_response_pt4) + strlen(timestamp_str) + 
                strlen(content_length_str) + file_response->file_size + strlen(content_type) + 1;

            char *http_response = (char*)malloc(sizeof(char) * (http_response_len));
            if (http_response == NULL) {
                fprintf(stderr, "ERROR: couldn't allocate memory for http response.\n");
               
                free(file_response->file_content);
                free(file_response);
                free(timestamp_str);
                free(timestamp_tm);
                
                return NULL;
            }

            memset(http_response, 0, http_response_len);

            strcpy(http_response, "");
            strcat(http_response, http_ok_response_pt1);
            strcat(http_response, content_type);
            strcat(http_response, http_ok_response_pt2);
            strcat(http_response, content_length_str);
            strcat(http_response, http_ok_response_pt3);
            strcat(http_response, timestamp_str);
            strcat(http_response, http_ok_response_pt4);
            memcpy(http_response + (http_response_len - file_response->file_size - 1),
                 file_response->file_content, file_response->file_size);

            if (application_context->stats_filename != NULL) {
                produce_stats_message(type);
            }

            if (send(client_sockfd, http_response, sizeof(char) * http_response_len, 0) != sizeof(char) * http_response_len) {
                log_message_producer((void*)"[ERROR] Couldn't send all data");

                free(http_response);
                free(file_response->file_content);
                free(file_response);
                free(timestamp_str);
                free(timestamp_tm);

                continue;
            }

            free(http_response);
            free(file_response->file_content);
            free(file_response);
            free(timestamp_str);
            free(timestamp_tm);
        }

        pthread_testcancel();
    }

    remove_client(client_sockfd);

    return NULL;
}