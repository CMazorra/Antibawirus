#include <stdio.h>
#include <stdlib.h>

#define EXTRA_SIZE_MB 12
#define BUFFER_SIZE 1024 // 1 KB
#define FILE_PATH "/media/adrianux/ADRI/formulario.txt"

int main()
{
    FILE *archivo = fopen(FILE_PATH, "ab"); // modo "ab" para añadir al final
    if (archivo == NULL)
    {
        perror("No se pudo abrir el archivo");
        return 1;
    }

    char buffer[BUFFER_SIZE] = {0}; // Lleno de ceros
    size_t total_bytes = EXTRA_SIZE_MB * 1024 * 1024;
    size_t escritos = 0;

    while (escritos < total_bytes)
    {
        size_t bytes_a_escribir = (total_bytes - escritos) > BUFFER_SIZE ? BUFFER_SIZE : (total_bytes - escritos);
        fwrite(buffer, 1, bytes_a_escribir, archivo);
        escritos += bytes_a_escribir;
    }

    fclose(archivo);
    printf("Se aumentó el tamaño del archivo en %d MB.\n", EXTRA_SIZE_MB);
    return 0;
}
