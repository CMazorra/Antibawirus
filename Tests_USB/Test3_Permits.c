#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

#define OLD_FILE "nota.txt"
#define NEW_FILE "nota.exe"

int main() {
    // Crear archivo original
    FILE *f = fopen(OLD_FILE, "w");
    if (!f) {
        perror("Error al crear archivo");
        exit(1);
    }
    fprintf(f, "Este es un archivo de texto inocente\n");
    fclose(f);
    
    // Cambiar nombre
    if (rename(OLD_FILE, NEW_FILE) < 0) {
        perror("rename");
        exit(1);
    }
    
    // Cambiar permisos a 777
    if (chmod(NEW_FILE, 0777) < 0) {
        perror("chmod");
        exit(1);
    }
    
    return 0;
}