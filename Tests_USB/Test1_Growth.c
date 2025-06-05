#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

#define TARGET "/media/adrianux/ADRI/documento.doc"
#define INITIAL_SIZE 2048    // 2KB
#define FINAL_SIZE 524288000 // 500MB

int main()
{
    int fd;
    char buffer[1024];

    // Crear archivo inicial
    fd = open(TARGET, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
    {
        perror("Error al crear archivo");
        exit(1);
    }

    // Escribir 2KB de datos pseudoaleatorios
    srand(time(NULL));
    for (int i = 0; i < INITIAL_SIZE / sizeof(buffer); i++)
    {
        for (int j = 0; j < sizeof(buffer); j++)
        {
            buffer[j] = rand() % 256;
        }
        write(fd, buffer, sizeof(buffer));
    }
    close(fd);

    // Esperar 5 segundos para simular actividad normal
    sleep(5);

    // Expandir a 500MB
    fd = open(TARGET, O_WRONLY);
    if (fd < 0)
    {
        perror("Error al abrir archivo");
        exit(1);
    }

    // Mover el puntero al final y escribir 1 byte para establecer el tamaño
    if (lseek(fd, FINAL_SIZE - 1, SEEK_SET) < 0)
    {
        perror("Error al hacer lseek");
        close(fd);
        exit(1);
    }

    if (write(fd, "", 1) < 0)
    {
        perror("Error al escribir para expandir");
        close(fd);
        exit(1);
    }

    close(fd);

    return 0;
}