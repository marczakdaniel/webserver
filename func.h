// Daniel Marczak - 324351

#ifndef FUNC_H
#define FUNC_H

#include <stdio.h>
#include <sys/stat.h>
#include <netinet/in.h>

#define ERROR(str) { fprintf(stderr, "%s: %s\n", str, strerror(errno)); exit(EXIT_FAILURE); }
#define BUFFER_SIZE 10000000
#define WORD_SIZE 200 

#define NO_TYPE 0
#define TXT_TYPE 1
#define HTML_TYPE 2
#define CSS_TYPE 3
#define JPEG_TYPE 4
#define PNG_TYPE 5
#define PDF_TYPE 6

#define RECEIVE_TIME_S 1
#define RECEIVE_TIME_US 0

struct Info {
    int port;
    char * catalogue;
    int sockfd;
};

struct HTTP_data {
    char request_method[WORD_SIZE];
    char requested_url[WORD_SIZE];
    char protocol_version[WORD_SIZE];
    char host_value[WORD_SIZE];
    char connection_value[WORD_SIZE];
    char domain_name[WORD_SIZE];
    char file_name[WORD_SIZE];
    char port[WORD_SIZE];
    char file_extension[WORD_SIZE];
    struct stat file_stat;
    FILE *fptr;
};

void read_configuration(int argc, char* argv[], struct Info * info);
void create_socket(struct Info * info);
void connection(struct Info * info);
int extract_data(char * buffer, ssize_t bytes_read, struct HTTP_data * http_data, struct Info * info);
int analyse_data(struct HTTP_data * http_data);
void response_404(char * response);
void response_501(char * response);
void response_403(char * response);
void response_301(char * response, struct HTTP_data * http_data);
int response_200(char * response, struct HTTP_data * http_data);
int get_file_extension(char * dst, char * src);
#endif // FUNC_H