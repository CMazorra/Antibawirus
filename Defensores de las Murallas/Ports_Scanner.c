#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>

#define NUMBER_THREADS 64
#define NUMBER_PORTS 65535

// Struct to pass data to threads
typedef struct {
    int start_port;
    int end_port;
    struct sockaddr_in target;
} ScanArgs;

const char* get_service_name(int port) {
    switch (port) {
        case 20: return "FTP Data";
        case 21: return "FTP Control";
        case 22: return "SSH";
        case 23: return "Telnet";
        case 25: return "SMTP";
        case 53: return "DNS";
        case 80: return "HTTP";
        case 110: return "POP3";
        case 143: return "IMAP";
        case 443: return "HTTPS";
        case 3306: return "MySQL";
        case 3389: return "RDP";
        default: return "Desconocido";
    }
}

int is_expected_port(int port) {
    int expected_ports[] = {20, 21, 22, 23, 25, 53, 80, 110, 143, 443, 3306, 3389};
    int n = sizeof(expected_ports) / sizeof(expected_ports[0]);

    for(int i = 0; i < n; i++) {
        if (port == expected_ports[i]) return 1;
    }
    return 0;
}

void *scanner(void *args_void) {
    ScanArgs *args = (ScanArgs *)args_void;
    int sock;

    for (int port = args->start_port; port <= args->end_port; port++) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            continue;
        }

        args->target.sin_port = htons(port);

        // 1 second timeout
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        if (connect(sock, (struct sockaddr *)&args->target, sizeof(args->target)) == 0) {
            if (is_expected_port(port)) {
                printf("Puerto %d abierto (%s)\n", port, get_service_name(port));
            }
            else{
                printf("Puerto %d abierto (%s) -- ¡Anomalía detectada!\n", port, get_service_name(port));
            }
        }

        close(sock);
    }

    free(args);
    pthread_exit(NULL);
}

int main() {
    struct sockaddr_in target;
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = inet_addr("127.0.0.1"); // Local scan

    pthread_t hilos[NUMBER_THREADS];
    int ports_per_thread = NUMBER_PORTS / NUMBER_THREADS;
    int start = 1;

    printf("Escaneando puertos del %d al %d usando %d hilos...\n", 1, NUMBER_PORTS, NUMBER_THREADS);

    for (int i = 0; i < NUMBER_THREADS; i++) {
        int end = start + ports_per_thread - 1;
        if (i == NUMBER_THREADS - 1) end = NUMBER_PORTS;

        ScanArgs *args = malloc(sizeof(ScanArgs));
        args->start_port = start;
        args->end_port = end;
        args->target = target;

        if (pthread_create(&hilos[i], NULL, scanner, args) != 0) {
            perror("No se pudo crear el hilo");
            free(args);
        }

        start = end + 1;
    }

    // Wait for the threads to finish
    for (int i = 0; i < NUMBER_THREADS; i++) {
        pthread_join(hilos[i], NULL);
    }

    printf("Escaneo completo.\n");
    return 0;
}
