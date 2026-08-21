/* For signal library */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <confuse.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h> 
#include <sys/time.h>

#include "http.h"
#include "tcp.h"
#include "queue.h"
#include "server.h"
#include "logs.h"


#define DEF_CONF_FILE "server.conf"

#define DEFAULT_PORT 8000
#define DEFAULT_ROOT "www/"
#define DEFAULT_MAX_CLIENTS 32
#define DEFAULT_SIGN "Server"
#define DEFAULT_LOG_FILE "server_logs"
#define DEFAULT_TIMEOUT_MS 1000
#define DEFAULT_POOL_SIZE 8
#define DEFAULT_QUEUE_SIZE 32
#define DEFAULT_BACKLOG 10

volatile sig_atomic_t sigint = 0; /* sigint handling variable */

/* thread argument structure */
typedef struct{
    int id;                    /**< Thread id */
    int sockfd;                /**< Socket descriptor */
    queue *cq;                 /**< Client queue pointer */
    server_conf *sc;           /**< Server configuration pointer */
    log_file *lf;              /**< Log structure pointer */
}thread_args;

/* Sigint handler */
void sigint_handle(int sig){
    sigint = 1;
}

/* Function executed in each thread */
void *thread_function(void *arg);

/* Initializes server configuration */
server_conf *initialize_server_conf(char *conf_filename){
    server_conf *sc;
    cfg_t *cfg;
    char *tmp;
    cfg_opt_t opts[] = {
        CFG_INT("listen_port", DEFAULT_PORT, CFGF_NONE),
        CFG_STR("server_root", DEFAULT_ROOT, CFGF_NONE),
        CFG_STR("server_signature", DEFAULT_SIGN, CFGF_NONE),
        CFG_STR("log_file", DEFAULT_LOG_FILE, CFGF_NONE),
        CFG_INT("timeout_ms", DEFAULT_TIMEOUT_MS, CFGF_NONE),
        CFG_INT("pool_size", DEFAULT_POOL_SIZE, CFGF_NONE),
        CFG_INT("queue_size", DEFAULT_QUEUE_SIZE, CFGF_NONE),
        CFG_INT("backlog", DEFAULT_BACKLOG, CFGF_NONE),
        CFG_END()
    };

    cfg = cfg_init(opts, 0);

    if(cfg_parse(cfg, conf_filename) == CFG_PARSE_ERROR){
        cfg_free(cfg);
        return NULL;
    }
    
    sc = malloc(sizeof(server_conf));
    if(!sc) return NULL;
    tmp = cfg_getstr(cfg, "server_root");
    sc->server_root = strdup(tmp);
    tmp = cfg_getstr(cfg, "server_signature");
    sc->server_signature = strdup(tmp);
    sc->listen_port = cfg_getint(cfg, "listen_port");
    sc->queue_size = cfg_getint(cfg, "queue_size");
    sc->pool_size = cfg_getint(cfg, "pool_size");
    sc->backlog = cfg_getint(cfg, "backlog");
    tmp = cfg_getstr(cfg, "log_file");
    sc->log_file = strdup(tmp);
    sc->timeout_ms = cfg_getint(cfg, "timeout_ms");

    cfg_free(cfg);

    sc->root_fd = open(sc->server_root, O_DIRECTORY | O_RDONLY | O_CLOEXEC);
    if(sc->root_fd < 0) return NULL;
     
    return sc;
}

/* Frees server configuration structure */
void free_server_conf(server_conf *sc){
    free(sc->server_signature);
    free(sc->server_root);
    free(sc->log_file);
    free(sc);
}

int main(){
    int sockfd, ret, i, j, clientfd;
    pthread_t *threads;
    thread_args *args;
    struct sigaction sa;
    queue *cq;
    server_conf *sc;
    log_file *lf;
    char *error_string, ip_port_str[64];
    struct timeval timeout;

    /* Load server conf */
    sc = initialize_server_conf(DEF_CONF_FILE);
    if(!sc){
        fprintf(stderr, "Error parsing configuration file\n");
        return -1;
    }
    timeout.tv_sec = sc->timeout_ms/1000;
    timeout.tv_usec = (sc->timeout_ms % 1000) * 1000;

    if((lf = logs_init_file(sc->log_file)) == NULL) {
        free_server_conf(sc);
        return -1;
    }

    /* Mask sigaction handler */
    sa.sa_handler = sigint_handle;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    signal(SIGPIPE, SIG_IGN);

    /* Client queue initialization */
    cq = queue_create(sc->pool_size, sc->queue_size);
    if(!cq){
        logs_print(LOG_ERROR, lf, "Error iniziating client queue");
        free_server_conf(sc);
        logs_destroy(lf);
        return -1;
    }

    /* TCP socket initialization */
    ret = tcp_start_connection(&sockfd, sc->listen_port, sc->backlog);
    if(ret == -1){
        logs_print(LOG_ERROR, lf, "Error opening TCP socket");
        queue_destroy(cq);
        free_server_conf(sc);
        logs_destroy(lf);
        return -1;
    }

    logs_print(LOG_INFO, lf, "Server initialized in port %d", sc->listen_port);

    /* Thread memory allocation */
    threads = malloc(sizeof(pthread_t) * sc->pool_size);
    if(!threads){
        queue_destroy(cq);
        free_server_conf(sc);
        logs_destroy(lf);
        return -1;
    }
    args = malloc(sizeof(thread_args) * sc->pool_size);
    if(!args){
        queue_destroy(cq);
        free_server_conf(sc);
        logs_destroy(lf);
        return -1;
    }
   
    /* Pool loop */
    for ( i = 0; i < sc->pool_size; i++) {
        args[i].id = i;
        args[i].cq = cq;
        args[i].sc = sc;
        args[i].sockfd = sockfd;
        args[i].lf = lf;
        ret = pthread_create(&threads[i], NULL, thread_function, &args[i]);
        if (ret) {
            logs_print(LOG_ERROR, lf, "Error spawning thread");
            for(j=0; j<i; j++){
                pthread_detach(threads[j]);
            }
            queue_destroy(cq);
            free_server_conf(sc);
            logs_destroy(lf);
            free(args);
            free(threads);
            return -1;
        }
    }

    while(!sigint){
        ret = tcp_accept_connection(&clientfd, sockfd);
        if(ret == -1 || clientfd < 0){
            /* ctrl-c */
            if(errno == EINTR) break;
            error_string = strerror(errno);
            logs_print(LOG_ERROR, lf, "Error accepting connection: %s", error_string);
            break;
        }
        /* Socket timeout configuration */
        if(timeout.tv_sec != 0 || timeout.tv_usec != 0)
            setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        get_client_ip_port(clientfd, ip_port_str, lf);
        queue_push(cq, clientfd, &sigint);
        logs_print(LOG_INFO, lf, "New connection added to queue: descriptor %d", clientfd);
    }

    logs_print(LOG_INFO, lf, "SIGINT received, shutting down server", sc->listen_port);

    /* Wake up threads */
    queue_shutdown(cq);

    for(i = 0; i < sc->pool_size; i++){
        clientfd = queue_get_active_fd(cq, i);
        if(clientfd) {
            /* If active, call to close connection */
            tcp_shutdown(clientfd);
        }
        pthread_join(threads[i], NULL);
    }

    ret = tcp_shutdown(sockfd);
    if(ret < 0){
        for(j=0; j<i; j++){
            pthread_detach(threads[j]);
        }
        free_server_conf(sc);
        queue_destroy(cq);
        logs_destroy(lf);
        free(args);
        free(threads);
        return -1;
    }

    queue_destroy(cq);
    free_server_conf(sc);
    logs_destroy(lf); 
    free(args);
    free(threads);
    return 0;
}

void *thread_function(void *arg) {
    thread_args *args = (thread_args *) arg;
    int clientfd, ret, bytes_read, last_len, buff_len, expected_len, open_con, header_size;
    char buff[MAX_BUFFR], *error_sring, ip_port_str[64];
    queue *cq = args->cq;
    http_request request;
    int id = args->id;
    server_conf *sc = args->sc;
    log_file *lf = args->lf;

    buff_len = 0;
    expected_len = 0;
    last_len = 0;
    open_con = 1;
    ret = 0;
    while(!sigint){
        
        /* Blocking point. Waits for client and extracts fd*/
        clientfd = queue_pop(cq, &sigint);

        if(sigint) break;

        queue_add_active_fd(cq, clientfd, id);
        get_client_ip_port(clientfd, ip_port_str, lf);
        logs_print(LOG_INFO, lf, "Connection accepted by thread %d - descriptor %d - client %s", id, clientfd, ip_port_str);

        open_con = 1;
        /* Reads til full header is read */
        do {
            bytes_read = tcp_recv(clientfd, buff + buff_len, MAX_BUFFR - buff_len);
            if(bytes_read == 0){
                /* Connection closed by client if recv returns 0 */
                logs_print(LOG_INFO, lf, "Client %d closed connection", ip_port_str);
                open_con = 0;
                break;
            }
            if(bytes_read == -1){
                if(errno == EAGAIN){
                    /* timeout */
                    logs_print(LOG_INFO, lf, "Connection %d closed by timeout", clientfd);
                }else{
                    /* Internal error response */
                    error_sring = strerror(errno);
                    logs_print(LOG_ERROR, lf, "TCP error on client %d: %s", ip_port_str, error_sring);
                    http_send_error(clientfd, HTTP_INTERNAL_ERROR);
                }
                open_con = 0;
                break;
            }
                
            last_len = buff_len;
            buff_len += bytes_read;

            ret = http_parse_request(buff, &request, buff_len, last_len);
            /* If petition is invalid or too long */
            if(ret == -1 || (buff_len == MAX_BUFFR && ret == -2)){
                logs_print(LOG_WARNING, lf, "Invalid or too long petition. Client %d", ip_port_str);
                http_send_error(clientfd, HTTP_BAD_REQUEST);
                open_con = 0;
                break;
            }
        } while(ret == -2 && open_con);

        /* If connection is closed, go to next iteration */
        if(!open_con){
            queue_del_active_fd(cq, id);
            tcp_shutdown(clientfd);
            continue;
        }

        header_size = ret;

        expected_len = http_get_content_length(&request);
        /* Already exceeded */
        if(expected_len > MAX_BUFFR - header_size){
            http_send_error(clientfd, HTTP_BAD_REQUEST);
            open_con = 0;
        }

        while (buff_len-header_size < expected_len && open_con) {
            bytes_read = tcp_recv(clientfd, buff + buff_len, MAX_BUFFR - buff_len);
            if(bytes_read == 0){
                /* Connection closed by client if recv returns 0 */
                logs_print(LOG_INFO, lf, "Client %s closed connection", ip_port_str);
                open_con = 0;
                break;
            }
            if(bytes_read == -1){
                error_sring = strerror(errno);
                logs_print(LOG_ERROR, lf, "TCP error on client %d: %s", ip_port_str, error_sring);
                http_send_error(clientfd, HTTP_INTERNAL_ERROR);
                open_con = 0;
                break;
            }

            buff_len += bytes_read;

        }
        
        if(!open_con){
            tcp_shutdown(clientfd);
            queue_del_active_fd(cq, id);
            continue;
        }

        /* adds request body to structure */
        http_add_body(&request, buff + header_size, buff_len - header_size);
        
        ret = http_process_request(&request, clientfd, sc, lf);
        if(ret < 0 ){
            /* Error procesing response. Handling is done in http_process_request */
            logs_print(LOG_ERROR, lf, "Error processing HTTP request.", ip_port_str);
            http_send_error(clientfd, HTTP_INTERNAL_ERROR);
            tcp_shutdown(clientfd);
            queue_del_active_fd(cq, id);
            continue;
        }


        queue_del_active_fd(cq, id);
        tcp_shutdown(clientfd);

        buff_len = 0;
        last_len = 0;

        logs_print(LOG_INFO, lf, "Closing connection %d", clientfd);
    }

 
    return NULL;
}
