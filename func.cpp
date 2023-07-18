// Daniel Marczak - 324351

#include "func.h"
#include <stdlib.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

char recv_buffer[BUFFER_SIZE];
char reply_buffer[BUFFER_SIZE];

void read_configuration(int argc, char* argv[], struct Info * info) {
    if (argc != 3) {
        fprintf(stderr, "Invalid number of arguments\r\n");
        exit(EXIT_FAILURE);
    }
    info->port = atoi(argv[1]);
    if (info->port < 1024 || info->port > 65535) {
        fprintf(stderr, "Invalid port number\r\n");
        exit(EXIT_FAILURE);
    }
    DIR *dir = opendir(argv[2]);
    if (dir == NULL) {
        fprintf(stderr, "Invalid directory name\r\n");
        exit(EXIT_FAILURE);
    }
    closedir(dir);
    info->catalogue = argv[2];
}

void create_socket(struct Info * info) {
    info->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (info->sockfd < 0) {
        ERROR("socket error");
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(info->port);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(info->sockfd, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
        ERROR("bind error");
    }

    if (listen(info->sockfd, 64) < 0) {
        ERROR("listen error");
    }
}

void connection(struct Info * info) {
    for (;;) {
        int connected_sockfd = accept(info->sockfd, NULL, NULL);
        if (connected_sockfd < 0) {
            ERROR("accept error");
        }

        for (;;) {
            fd_set descriptors;
            FD_ZERO(&descriptors);
            FD_SET(connected_sockfd, &descriptors);

            struct timeval tv; tv.tv_sec = RECEIVE_TIME_S; tv.tv_usec = RECEIVE_TIME_US;

            int ready = select(connected_sockfd + 1, &descriptors, NULL, NULL, &tv);
            if (ready < 0) {
                ERROR("select error");
            }
            if (ready == 0) {
                break;
            }

            memset(recv_buffer, 0, sizeof(recv_buffer));

            ssize_t bytes_read = recv(connected_sockfd, recv_buffer, BUFFER_SIZE, MSG_DONTWAIT);
            if (bytes_read < 0) {
                if (errno == EWOULDBLOCK) {
                    break;
                }
                ERROR("recv error");
            }
            if (bytes_read == 0) {
                break;
            }

            recv_buffer[bytes_read] = '\0';

            struct HTTP_data http_data;
            memset(&http_data, 0, sizeof(http_data));

            memset(reply_buffer, 0, sizeof(reply_buffer));

            int data_size = 0;

            if (extract_data(recv_buffer, bytes_read, &http_data, info) == 0){
                response_501(reply_buffer);
                data_size = strlen(reply_buffer);
            }
            else {
                int status = analyse_data(&http_data);

                if (status == 200) {
                    data_size = response_200(reply_buffer, &http_data);
                }
                else if (status == 404) {
                    response_404(reply_buffer);
                }
                else if (status == 403) {
                    response_403(reply_buffer);
                }
                else if (status == 301) {
                    response_301(reply_buffer, &http_data);
                }
                else {
                    response_501(reply_buffer);
                }

                if (data_size == 0 || status != 200) {
                    data_size = strlen(reply_buffer);
                }
            }

            ssize_t bytes_sent = send(connected_sockfd, reply_buffer, data_size, 0);

            if (bytes_sent < 0) {
                ERROR("send error");
            }

            if (strcmp(http_data.connection_value, "close") == 0) {
                break;
            }
        }

        if (close(connected_sockfd) < 0)
            ERROR("close error");
    }
}


void get_domain_port(char * dst_d, char * dst_p, char * src) {
    for (int i = 0; i < WORD_SIZE; i++) {
        if (src[i] == ':') {
            dst_d[i] = '\0';
            strcpy(dst_p, src + i + 1);
            break;
        } 
        else if (src[i] == '\0') {
            fprintf(stderr, "ERROR get_catalague\r\n");
            break;
        }
        else {
            dst_d[i] = src[i];
        }
    }
} 

void get_file_name(struct HTTP_data * http_data, struct Info * info) {
    char slash[2] = "/";
    strcpy(http_data->file_name, "");
    strcat(http_data->file_name, info->catalogue);
    strcat(http_data->file_name, slash);
    strcat(http_data->file_name, http_data->domain_name);
    strcat(http_data->file_name, http_data->requested_url);
}

/* return 1 if packet is implemented (GET and has Host and Connection) */
int extract_data(char * buffer, ssize_t bytes_read, struct HTTP_data * http_data, struct Info * info) {
    if (sscanf(buffer, "%s %s %s", 
            http_data->request_method, http_data->requested_url, http_data->protocol_version) != 3) {
        return 0;
    }
    if (strcmp(http_data->request_method, "GET") != 0) {
        return 0;
    }


    for (int i = 0; i < bytes_read - 1; i++) {
        char field_name[WORD_SIZE] = {0};
        char field_value[WORD_SIZE] = {0};
        if (buffer[i] == '\n' && buffer[i + 1] == '\n') {
            break;
        }
        else if (buffer[i] == '\n') {
            if (sscanf(buffer + i + 1, "%s %s", field_name, field_value) != 2) {
                break;
            }
            if (strcmp(field_name, "Host:") == 0) {
                strcpy(http_data->host_value, field_value);
                get_domain_port(http_data->domain_name, http_data->port, field_value);
                get_file_name(http_data, info);
            }
            else if (strcmp(field_name, "Connection:") == 0) {
                strcpy(http_data->connection_value, field_value);
            }
        }
        if (http_data->host_value[0] != 0 && http_data->connection_value[0] != 0) {
            break;
        } 
    } 
    if (http_data->host_value[0] == 0) {
        return 0;
    }
    return 1;
}

int analyse_data(struct HTTP_data * http_data) {
    if (strstr(http_data->file_name, "/..") != NULL) {
        return 403;
    }
    if (stat(http_data->file_name, &http_data->file_stat) == -1) {
        if (errno == ENOENT) {
            return 404;
        }
        ERROR("stat error");
    }
    if (S_ISDIR(http_data->file_stat.st_mode)) {
        return 301;
    }
    return 200;
}

void response_404(char * response) {
    char content[] = "<html>\r\n<head>\r\n<title>404</title>\r\n</head>\r\n<body>\r\n<p>404 Not Found</p>\r\n</body>\r\n</html>\r\n";
    strcpy(response, "HTTP/1.1 404 Not Found\r\n");
    strcat(response, "Content-Type: text/html; charset=UTF-8\r\n");
    char content_length[6];
    sprintf(content_length, "%ld", strlen(content));
    strcat(response, "Content-Length: ");
    strcat(response, content_length);
    strcat(response, "\r\n\r\n");
    strcat(response, content);
}

void response_501(char * response) {
    char content[] = "<html>\r\n<head>\r\n<title>501</title>\r\n</head>\r\n<body>\r\n<p>404 Not Implemented</p>\r\n</body>\r\n</html>\r\n";
    strcpy(response, "HTTP/1.1 501 Note Implemented\r\n");
    strcat(response, "Content-Type: text/html; charset=UTF-8\r\n");
    char content_length[6];
    sprintf(content_length, "%ld", strlen(content));
    strcat(response, "Content-Length: ");
    strcat(response, content_length);
    strcat(response, "\r\n\r\n");
    strcat(response, content);
}

void response_403(char * response) {
    char content[] = "<html>\r\n<head>\r\n<title>403</title>\r\n</head>\r\n<body>\r\n<p>403 Forbidden</p>\r\n</body>\r\n</html>\r\n";
    strcpy(response, "HTTP/1.1 403 Forbidden\r\n");
    strcat(response, "Content-Type: text/html; charset=UTF-8\r\n");
    char content_length[6];
    sprintf(content_length, "%ld", strlen(content));
    strcat(response, "Content-Length: ");
    strcat(response, content_length);
    strcat(response, "\r\n\r\n");
    strcat(response, content);
}

void response_301(char * response, struct HTTP_data * http_data) {
    strcpy(response, "HTTP/1.1 301 Moved Permanently\r\n");
    strcat(response, "Location: http://");
    strcat(response, http_data->host_value);
    strcat(response, "/index.html");
}

int get_file_extension(char * dst, char * src) {
    char * ptr = strrchr(src, '.');
    if (ptr == NULL || ptr[0] == '\0') {
        strcpy(dst, "application/octet-stream");
        return NO_TYPE;
    }
    char extension[WORD_SIZE];
    strcpy(extension, ptr + 1); 
    if (strcmp(extension, "txt") == 0) {
        strcpy(dst, "text/plain; charset=UTF-8");
        return TXT_TYPE;
    }
    else if (strcmp(extension, "html") == 0) {
        strcpy(dst, "text/html; charset=UTF-8");
        return HTML_TYPE;
    }
    else if (strcmp(extension, "css") == 0) {
        strcpy(dst, "text/css; charset=UTF-8");
        return CSS_TYPE;
    }
    else if (strcmp(extension, "jpg") == 0 || strcmp(extension, "jpeg") == 0) {
        strcpy(dst, "image/jpeg");
        return JPEG_TYPE;
    }
    else if (strcmp(extension, "png") == 0) {
        strcpy(dst, "image/png");
        return PNG_TYPE;
    }
    else if (strcmp(extension, "pdf") == 0) {
        strcpy(dst, "application/pdf");
        return PDF_TYPE;
    }
    strcpy(dst, "application/octet-stream");
    return NO_TYPE;
}

int response_200(char * response, struct HTTP_data * http_data) {
    FILE *fptr;
    fptr = fopen(http_data->file_name, "rb");
    if (fptr == NULL) {
        response_404(response);
        return 0;
    } 
    char *content = (char *)malloc(http_data->file_stat.st_size);
    if (content == NULL) {
        ERROR("malloc error");
    }
    
    size_t blocks_read = fread(content, http_data->file_stat.st_size, 1, fptr);
    if (blocks_read != 1) {
        ERROR("fread error");
    }
    fclose(fptr);

    int http_size = 0;

    strcpy(response, "HTTP/1.1 200 OK\r\n");
    strcat(response, "Content-Type: ");
    int type = get_file_extension(http_data->file_extension, http_data->file_name);
    strcat(response, http_data->file_extension);
    strcat(response, "\r\n");
    strcat(response, "Content-Length: ");
    char content_length[6];
    sprintf(content_length, "%ld", http_data->file_stat.st_size);
    strcat(response, content_length);
    strcat(response, "\r\n\r\n");
    
    http_size = strlen(response);
    http_size += http_data->file_stat.st_size;

    memcpy(response + strlen(response), content, http_data->file_stat.st_size);
    return http_size;
}   



