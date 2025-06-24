#include <stdio.h>
#include <stdlib.h>

int main()
{
    FILE *archivo;
    char ruta_completa[] = "/media/adrianux/ADRI/documento.doc";

    archivo = fopen(ruta_completa, "w");

    if (archivo == NULL)
    {
        perror("Error al crear el archivo");
        printf("Verifica que:\n");
        printf("1. El USB está correctamente montado en '%s'\n", ruta_completa);
        printf("2. Tienes permisos de escritura\n");
        exit(1);
    }

    fprintf(archivo, "Este archivo fue creado desde un programa en C\n");
    fclose(archivo);

    printf("¡Éxito! Archivo creado en:\n%s\n", ruta_completa);
    return 0;
}