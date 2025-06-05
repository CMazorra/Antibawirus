#include <stdio.h>

int main()
{
    const char *archivo_viejo = "/media/adrianux/ADRI/archivo.txt";
    const char *archivo_nuevo = "/media/adrianux/ADRI/archivo.exe";

    if (rename(archivo_viejo, archivo_nuevo) == 0)
    {
        printf("Archivo renombrado exitosamente a archivo.exe\n");
    }
    else
    {
        perror("Error al renombrar el archivo");
        return 1;
    }

    return 0;
}
