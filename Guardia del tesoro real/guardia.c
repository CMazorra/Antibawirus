#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

void obtener_informacion_proceso(const char *pid) {
    char ruta[256], nombre[256], buffer[1024];
    FILE *archivo;

    // Obtener el nombre del proceso desde /proc/[PID]/status
    snprintf(ruta, sizeof(ruta), "/proc/%s/status", pid);
    archivo = fopen(ruta, "r");
    if (archivo == NULL) {
        perror("Error al abrir archivo de status");
        return;
    }

    while (fgets(buffer, sizeof(buffer), archivo)) {
        if (strncmp(buffer, "Name:", 5) == 0) {
            sscanf(buffer, "Name:\t%255s", nombre);
            break;
        }
    }
    fclose(archivo);

    // Obtener el uso de CPU y memoria desde /proc/[PID]/stat
    snprintf(ruta, sizeof(ruta), "/proc/%s/stat", pid);
    archivo = fopen(ruta, "r");
    if (archivo == NULL) {
        perror("Error al abrir archivo de stat");
        return;
    }

    long utime, stime, rss;
    int i = 0;
    while (fscanf(archivo, "%s", buffer) == 1) {
        i++;
        if (i == 14) utime = atol(buffer);  // Tiempo de CPU en modo usuario
        if (i == 15) stime = atol(buffer);  // Tiempo de CPU en modo sistema
        if (i == 24) rss = atol(buffer);    // Uso de memoria
    }
    fclose(archivo);

    printf("PID: %s, Nombre: %s, CPU: %ld ticks, Memoria: %ld KB\n", pid, nombre, utime + stime, rss * 4);
}

int main() {
    DIR *dir = opendir("/proc");
    if (dir == NULL) {
        perror("No se puede abrir /proc");
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Verificar si el nombre del directorio es un número (PID)
        if (isdigit(entry->d_name[0])) {
            obtener_informacion_proceso(entry->d_name);
        }
    }

    closedir(dir);
    return 0;
}