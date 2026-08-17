#include <time.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include "logs.h"

char *get_log_type_name(log_type type);

log_file *logs_init_file(char *log_name) {
    log_file *lf;
    char fecha_log[32], complete_log_name[256];
    time_t now = time(NULL);
    struct tm *time = localtime(&now);
    struct stat st = {0};

    lf = (log_file*)malloc(sizeof(log_file));
    if (!lf) {
        return NULL;
    }

    if (!log_name) {
        return NULL;
    }
    if (stat("logs", &st) == -1){
        if (mkdir("logs", 0777) == -1) {
            fprintf(stderr, "Error creating logs directory\n");
            return NULL;
        }
    }
    else {
        if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Error: logs is not a directory\n");
            return NULL;
        }
    }

    strftime(fecha_log, sizeof(fecha_log), "%d-%m-%Y_%H-%M-%S", time);
    sprintf(complete_log_name, "logs/%s_%s.log", log_name, fecha_log);
    lf->file = fopen(complete_log_name, "w");
    if(!lf->file) {
        fprintf(stderr, "Error opening log file\n");
        return NULL;
    }

    if (pthread_mutex_init(&lf->mutex, NULL) != 0) {
        fprintf(stderr, "Error creating mutex\n");
        fclose(lf->file);
        return NULL;
    }

    return lf;
}

int logs_print(log_type type, log_file *lf, char *message, ...) {
    time_t now = time(NULL);
    struct tm *time = localtime(&now);
    char date[32], *type_str;
    va_list args;
    int ret;

    /* starting variable argument list */
    va_start(args, message);

    type_str = get_log_type_name(type);
    strftime(date, sizeof(date), "%d-%m-%Y, %H-%M-%S", time);

    pthread_mutex_lock(&lf->mutex);
    ret = fprintf(lf->file, "[%s] [%s] ", date, type_str);
    if (ret < 0) {
        pthread_mutex_unlock(&lf->mutex);
        return -1;
    }
    ret = vfprintf(lf->file, message, args);
    if (ret < 0) {
        pthread_mutex_unlock(&lf->mutex);
        return -1;
    }
    ret = fprintf(lf->file, "\n");
    if (ret < 0) {
        pthread_mutex_unlock(&lf->mutex);
        return -1;
    }

    pthread_mutex_unlock(&lf->mutex);

    /* cleaning arg list */
    va_end(args);
    return 0;

}

void get_client_ip_port(int clientfd, char *ip_port_str, log_file *lf) {
    char ip[INET_ADDRSTRLEN];
    struct sockaddr_in addr;
    unsigned int len;

    len = sizeof(addr);
    if (getpeername(clientfd, (struct sockaddr *)&addr, &len) == -1) {
        logs_print(LOG_ERROR, lf, "Error obteniendo la ip y el puerto del cliente");
        perror("getpeername");
        return;
    }
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));

    sprintf(ip_port_str, "%s:%d", ip, ntohs(addr.sin_port));
}

void logs_destroy(log_file *lf) {
    fclose(lf->file);
    pthread_mutex_destroy(&lf->mutex);
    free(lf);
}

/* ========== private function implementation =========== */

char *get_log_type_name(log_type type) {
    switch (type) {
        case LOG_ERROR:
            return "ERRR";
        case LOG_WARNING:
            return "WARN";
        case LOG_INFO:
            return "INFO";
        default:
            return "UNKN";
    }
}
