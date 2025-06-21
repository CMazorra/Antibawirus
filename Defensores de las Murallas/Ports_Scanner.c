#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>

#define MAX_THREADS 64
#define MAX_PORTS 65535

typedef struct {
    int port;
    const char *service;
} Service;

Service common_services[] = {
    {20, "FTP-DATA"}, {21, "FTP"}, {22, "SSH"}, {23, "TELNET"},
    {25, "SMTP"}, {53, "DNS"}, {80, "HTTP"}, {110, "POP3"},
    {143, "IMAP"}, {443, "HTTPS"}, {445, "SMB"}, {3306, "MySQL"},
    {3389, "RDP"}, {0, NULL}
};

Service risk_ports[] = {
    {4444, "Metasploit/Reverse Shell"},
    {31337, "Backdoor Elite"},
    {6667, "IRC (Usado por bots)"},
    {8080, "Proxy HTTP"},
    {9999, "Servicios no estándar"},
    {0, NULL}
};

typedef struct {
    int start_port;
    int end_port;
    struct sockaddr_in target;
} ScanArgs;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

const char* get_service_name(int port, int *is_risk) {
    *is_risk = 0;
    
    for (int i = 0; common_services[i].service != NULL; i++) {
        if (common_services[i].port == port) {
            return common_services[i].service;
        }
    }
    
    for (int i = 0; risk_ports[i].service != NULL; i++) {
        if (risk_ports[i].port == port) {
            *is_risk = 1;
            return risk_ports[i].service;
        }
    }
    
    return "Desconocido";
}

int is_common_port(int port) {
    for (int i = 0; common_services[i].service != NULL; i++) {
        if (common_services[i].port == port) {
            return 1;
        }
    }
    return 0;
}

void *port_scanner(void *args_void) {
    ScanArgs *args = (ScanArgs *)args_void;
    int sock;
    struct timeval timeout;
    
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    for (int port = args->start_port; port <= args->end_port; port++) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;

        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        args->target.sin_port = htons(port);

        if (connect(sock, (struct sockaddr *)&args->target, sizeof(args->target)) == 0) {
            int is_risk = 0;
            const char *service = get_service_name(port, &is_risk);
            
            pthread_mutex_lock(&mutex);
            if (is_risk) {
                printf("[ALERTA] Puerto %d/tcp abierto (%s) ▲\n", port, service);
            } else if (!is_common_port(port)) {
                printf("[ALERTA] Puerto %d/tcp abierto (%s) - servicio no estándar\n", port, service);
            } else {
                printf("[OK] Puerto %d/tcp (%s) abierto 😊\n", port, service);
            }
            pthread_mutex_unlock(&mutex);
        }
        close(sock);
    }

    free(args);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <rango-puertos>\n", argv[0]);
        printf("Ejemplo: %s 1-1024\n", argv[0]);
        return 1;
    }

    int start_port, end_port;
    if (sscanf(argv[1], "%d-%d", &start_port, &end_port) != 2) {
        fprintf(stderr, "Formato de rango inválido\n");
        return 1;
    }

    if (start_port < 1 || end_port > MAX_PORTS || start_port > end_port) {
        fprintf(stderr, "Rango inválido (1-%d)\n", MAX_PORTS);
        return 1;
    }

    struct sockaddr_in target;
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = inet_addr("127.0.0.1");

    printf("\nEscaneando puertos en localhost (%d-%d)...\n", start_port, end_port);
    
    int total_ports = end_port - start_port + 1;
    int thread_count = (total_ports < MAX_THREADS) ? total_ports : MAX_THREADS;
    int ports_per_thread = total_ports / thread_count;
    int extra_ports = total_ports % thread_count;

    pthread_t threads[MAX_THREADS];
    int current_start = start_port;

    for (int i = 0; i < thread_count; i++) {
        ScanArgs *args = malloc(sizeof(ScanArgs));
        args->start_port = current_start;
        args->end_port = current_start + ports_per_thread - 1 + (i < extra_ports ? 1 : 0);
        args->target = target;

        if (pthread_create(&threads[i], NULL, port_scanner, args) != 0) {
            perror("Error al crear hilo");
            free(args);
        }
        
        current_start = args->end_port + 1;
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\nEscaneo completado.\n");
    return 0;
}