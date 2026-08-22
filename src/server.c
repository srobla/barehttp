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
#include <sys/epoll.h>

#include "http.h"
#include "tcp.h"
#include "queue.h"
#include "server.h"
#include "logs.h"


#define DEF_CONF_FILE "server.conf"

#define DEFAULT_PORT 8000
#define DEFAULT_ROOT "www/"
#define DEFAULT_MAX_CLIENTS 16
#define DEFAULT_SIGN "Server"
#define DEFAULT_LOG_FILE "server_logs"
#define DEFAULT_TIMEOUT_MS 1000
#define DEFAULT_POOL_SIZE 8
#define DEFAULT_QUEUE_SIZE 32
#define DEFAULT_BACKLOG 10

#define USE_EPOLL

/* TODO: Store file descriptors and offset while sending file cos NONBLOCK */

volatile sig_atomic_t sigint = 0; /* sigint handling variable */

/* thread argument structure */
typedef struct{
    int id;                    /**< Thread id */
    int sockfd;                /**< Socket descriptor */
    queue *cq;                 /**< Client queue pointer */
    server_conf *sc;           /**< Server configuration pointer */
    log_file *lf;              /**< Log structure pointer */
}thread_args;

/* Client state */
typedef enum{
    CLIENT_READING_HEADERS,
    CLIENT_READING_BODY,
    CLIENT_SENDING_RESPONSE
}client_state;

/* struct storing client info */
struct _client_data{
    int fd;                    /**< Connection descriptor */
    client_state state;        /**< State */
    char rq[MAX_BUFFR];        /**< Request buffer */
    int rq_size;               /**< Request size */
    http_request http_rq;      /**< Request structure */
    int last_len;              /**< Request structure */
    int body_len;
    int header_len;
    client_data *next;
};

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

int set_non_blocking(int fd){
	int flags, ret;

	flags = fcntl(fd, F_GETFL, 0);

	if(flags == -1) {
		perror("fcntl");
		return -1;
	}

	ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	if(ret == -1) {
		perror("fcntl");
		return -1;
	}
	return 0;
}

int initialize_events(struct epoll_event **events, int size){

    /* Events allocation */
    *events = calloc(size, sizeof(struct epoll_event));
    if(!*events){
        perror("Error allocating memory for events array\n");
        return -1;
    }

    return 0;
}

void remove_client(client_data **clients, client_data *client){
    client_data **aux = clients;

    while(*aux != NULL){
        if(*aux == client){
            *aux = client->next;
            free(client);
            return;
        }

        aux = & (*aux)->next;
    }
}

void free_clients(client_data *clients){
    client_data *aux;

    while(clients != NULL){
        aux = clients->next;
        close(clients->fd);
        free(clients);
        clients = aux;
    }
}

void add_client(client_data **clients, client_data *client){
    client->next = *clients;
    *clients = client;
}

int epoll_server(server_conf *sc, log_file *lf){
    int sockfd, clientfd;
    int epollfd;
    int nfds, i, ret, expected_len;
    int bytes_read;
    struct epoll_event ev, *events;
    char buf[MAX_BUFFR];
    client_data *cd;
    client_data *active_clients;

    /* TCP Socket initialization */
    tcp_start_connection(&sockfd, sc->listen_port, DEFAULT_MAX_CLIENTS);

    if(set_non_blocking(sockfd)){
        logs_destroy(lf);
        free_server_conf(sc);
        return -1;
    }

    if(initialize_events(&events, sc->queue_size) != 0){
        logs_destroy(lf);
        free_server_conf(sc);
        return -1;
    }

    printf("Epoll structures data allocated succesfully\n");

    epollfd = epoll_create1(0);

    printf("Epoll created succesfully\n");

    if(epollfd == -1){
        perror("Error creating epoll file descriptor");
        free(events);
        logs_destroy(lf);
        free_server_conf(sc);
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = sockfd;

    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) == -1){
        perror("Error registering descriptor to epoll list");
        close(epollfd);
        free(events);
        logs_destroy(lf);
        free_server_conf(sc);
        return -1;
    }

    while(1){
        /* Blocking, waiting for I/O events on some file descriptor */
        printf("Waiting for I/O...\n");
        nfds = epoll_wait(epollfd, events, sc->queue_size, -1);
        /* TODO: Check sigint for normal shutdown */
        if(nfds < 0){
            close(epollfd);
            free(events);
            logs_destroy(lf);
            free_server_conf(sc);
            if(sigint){
                printf("Normal shutdown \n");
                return 0;
            }
            perror("Error waiting epoll");
            return -1;
        }

        /* Iterate over ready descriptors */
        for(i = 0; i < nfds; i++){
            /* New client trying to connect */
            if(events[i].data.fd == sockfd){
                printf("Reading sockfd for new connection\n");
                if(tcp_accept_connection(&clientfd, sockfd)){
                    perror("Error accepting connection");
                    close(epollfd);
                    logs_destroy(lf);
                    free(events);
                    free_server_conf(sc);
                    return -1;
                }
                /* NEW CLIENT */
                set_non_blocking(clientfd);
                printf("New connection accepted\n");

                ev.events = EPOLLIN; /* upgrade to EPOLLET Edge epoll*/
                ev.data.fd = clientfd;

                /* Create client data struct */
                cd = calloc(1, sizeof(client_data));
                cd->fd = clientfd;
                cd->state = CLIENT_READING_HEADERS;
                ev.data.ptr = cd;

                add_client(&active_clients, cd);
                
                if(epoll_ctl(epollfd, EPOLL_CTL_ADD, clientfd, &ev) == -1){
                    perror("Error adding clientfd to epoll list");
                    close(epollfd);
                    logs_destroy(lf);
                    free(events);
                    free_clients(active_clients);
                    free_server_conf(sc);
                    return -1;
                }
            }else{
                /* Already added client has I/O */
                cd = (client_data *)events[i].data.ptr;
                clientfd = cd->fd;
                printf("New I/O activity on fd %d. Rq size = %d, actual buffer: %s\n", cd->fd, cd->rq_size, cd->rq);

                /* For now just read and print */
                bytes_read = tcp_recv(clientfd, cd->rq + cd->rq_size, MAX_BUFFR - cd->rq_size);

                if(bytes_read == 0){
                    close(clientfd);
                    remove_client(&active_clients, cd);
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, clientfd, NULL);
                    printf("Connection closed by client. Clientfd deleted from queue\n");
                }else if (bytes_read < 0){
                    if(errno != EAGAIN && errno != EWOULDBLOCK){
                        printf("Internal error. Clientfd deleted from queue\n");
                        close(clientfd);
                        epoll_ctl(epollfd, EPOLL_CTL_DEL, clientfd, NULL);
                        remove_client(&active_clients, cd);
                    }
                }else{
                    /* Correct read. Could be headers read or body read */
                    if(cd->state == CLIENT_READING_HEADERS){
                        printf("Client %d sent: %s\n", clientfd, cd->rq);
                        cd->last_len = cd->rq_size;
                        cd->rq_size += bytes_read;

                        ret = http_parse_request(cd->rq, &cd->http_rq, cd->rq_size, cd->last_len);
                        /* Invalid request */
                        if(ret == -1){
                            printf("Invalid request, clientfd deleted\n");
                            close(clientfd);
                            epoll_ctl(epollfd, EPOLL_CTL_DEL, clientfd, NULL);
                            remove_client(&active_clients, cd);
                        /* Inclomplete request */
                        }else if(ret == -2){
                            /* Too long */
                            if(cd->rq_size >= MAX_BUFFR){
                                printf("Too long request, clientfd deleted\n");
                                close(clientfd);
                                epoll_ctl(epollfd, EPOLL_CTL_DEL, clientfd, NULL);
                                remove_client(&active_clients, cd);
                            /* Incomplete and valid */
                            }else{
                                /* TODO: When doing EPOLLET, reinsert into epoll */
                                continue;
                            }
                        /* Full request header read */
                        }else{
                            printf("Request headers read correctly\n");
                            /* Check if it is complete or exceeded */
                            cd->body_len = http_get_content_length(&cd->http_rq);
                            /* Too long */
                            if(cd->body_len + cd->rq_size >= MAX_BUFFR){
                                printf("Request too long. Closing \n");
                                remove_client(&active_clients, cd);
                                close(clientfd);
                                epoll_ctl(epollfd, EPOLL_CTL_DEL, clientfd, NULL);
                                continue;
                            }

                            /* No body, response */
                            if(cd->body_len == 0){
                                printf("No body request\n");
                                http_process_request(&cd->http_rq, cd->fd, sc, lf);
                                printf("Request processed, closing connection\n");
                                remove_client(&active_clients, cd);
                                epoll_ctl(epollfd, EPOLL_CTL_DEL, clientfd, NULL);
                                close(clientfd);
                            /* Body, keep reading */
                            }else{
                                printf("Request has body, size: %d\n", cd->body_len);
                                cd->state = CLIENT_READING_BODY;
                            }
                        }
                    }else if(cd->state == CLIENT_READING_BODY){
                        printf("Reading request body\n");
                        cd->rq_size += bytes_read;
                        /* Body completely read, response */
                        if(cd->rq_size - cd->body_len >= cd->body_len){
                            printf("Body completed. Full request: \n%s\n", cd->rq);
                            http_add_body(&cd->http_rq, cd->rq + cd->rq_size, cd->rq_size - cd->header_len);
                            http_process_request(&cd->http_rq, cd->fd, sc, lf);
                            printf("Request processed, closing connection\n");
                            close(clientfd);
                            remove_client(&active_clients, cd);
                            epoll_ctl(epollfd, EPOLL_CTL_DEL, clientfd, NULL);
                        /* Add to body */
                        }else{
                            cd->rq_size += bytes_read;
                        }
                    }

                }
            }
        }
    }

    close(epollfd);
    free(events);
    logs_destroy(lf);
    free_server_conf(sc);
    return 0;
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

#ifdef USE_EPOLL
    return epoll_server(sc, lf);
#endif

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
