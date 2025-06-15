#include <stdio.h>
#include <sys/stat.h>

int main()
{
    const char *filename = "/media/adrianux/ADRI/archivo.txt";

    // Cambiar permisos a 0644: rw-r--r--
    if (chmod(filename, 0644) == 0)
    {
        printf("Permisos cambiados exitosamente para %s\n", filename);
    }
    else
    {
        perror("Error al cambiar permisos");
        return 1;
    }

    return 0;
}
