// Daniel Marczak - 324351

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "func.h"

int main(int argc, char* argv[]) {
    struct Info info;
    memset(&info, 0, sizeof(info));
    
    read_configuration(argc, argv, &info);

    create_socket(&info);

    connection(&info);

    close(info.sockfd);
}