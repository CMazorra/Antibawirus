#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>

#define ORIGINAL "documento.pdf"
#define PREFIX "documento_copy_"
#define NUM_COPIES 5

int main() {
    int src_fd, dest_fd;
    struct stat st;
    char *buffer;
    char dest_path[256];
    
    // Crear archivo original si no existe
    src_fd = open(ORIGINAL, O_WRONLY | O_CREAT, 0644);
    if (src_fd < 0) {
        perror("Error al crear original");
        exit(1);
    }
    close(src_fd);
    
    // Obtener información del archivo
    if (stat(ORIGINAL, &st) < 0) {
        perror("stat");
        exit(1);
    }
    
    // Leer contenido del archivo
    src_fd = open(ORIGINAL, O_RDONLY);
    if (src_fd < 0) {
        perror("Error al abrir original");
        exit(1);
    }
    
    buffer = malloc(st.st_size);
    if (!buffer) {
        perror("malloc");
        close(src_fd);
        exit(1);
    }
    
    read(src_fd, buffer, st.st_size);
    close(src_fd);
    
    // Crear copias
    srand(time(NULL));
    for (int i = 0; i < NUM_COPIES; i++) {
        // Generar sufijo aleatorio
        char suffix[4];
        for (int j = 0; j < 3; j++) {
            suffix[j] = 'a' + (rand() % 26);
        }
        suffix[3] = '\0';
        
        snprintf(dest_path, sizeof(dest_path), "%s%s.pdf", PREFIX, suffix);
        
        dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (dest_fd < 0) {
            perror("Error al crear copia");
            continue;
        }
        
        write(dest_fd, buffer, st.st_size);
        close(dest_fd);
    }
    
    free(buffer);
    return 0;
}