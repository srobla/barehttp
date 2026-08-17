/*
 * @file tcp.c
 * @brief Manages TCP sockets, connections and comunication.
 *
 * @author Samuel Robla y Ángela Horcajo
 */
#include "tcp.h"
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int tcp_start_connection(int *sockfd, int port, int max_conns){
    int ret;
    struct sockaddr_in sock_struct;

    if(!sockfd){
        fprintf(stderr, "Error on socket descriptor\n");
        return -1;
    }

    /* TCP socket initialization */
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(*sockfd < 0){
        perror("Error opening socket");
        return -1;
    }

    /* Contains port and address */
    memset(&sock_struct, 0, sizeof(sock_struct));
    sock_struct.sin_family = AF_INET;
    sock_struct.sin_port = htons(port);
    /* INADDR_ANY == 0 */
    sock_struct.sin_addr.s_addr = htonl(INADDR_ANY);

    ret = bind(*sockfd, (struct sockaddr*)&sock_struct, sizeof(sock_struct));
    if(ret) {
        perror("Error binding socket");
        return -1;
    }

    ret = listen(*sockfd, max_conns);
    if(ret == -1) {
        perror("Error listening");
        return -1;
    }

    return 0;
}

int tcp_accept_connection(int *connfd, int sockfd){
    
    if(!connfd){
        fprintf(stderr, "Error on connection descriptor\n");
        return -1;
    }

    *connfd = accept(sockfd, NULL, NULL);

    if(*connfd == -1){
        return -1;
    }

    return 0;
}

int tcp_send(int fd, char *buf, int len){
    int ret;
    ret = send(fd, buf, len, 0);
    if(ret < 0 && errno == EPIPE){
        tcp_shutdown(fd);
    }
    return ret;
}

int tcp_recv(int fd, char *buf, int len){
    int ret;

    ret = recv(fd, buf, len, 0);
    return ret;
}

int tcp_shutdown(int fd){
    int ret;

    ret = shutdown(fd, SHUT_RDWR);

    /* This close could return < 0 if fd is already closed.
     * Ignoring this path */
    close(fd);

    /* ENOTCONN throws when shutting down a connection already closed by the client
     * EBADF throws when connection is already close 
     * intentionaly ignoring both. */
    if(ret && errno != ENOTCONN && errno != EBADF){
        perror("Error shuting down server");
        return -1;
    }

    return 0;
}
