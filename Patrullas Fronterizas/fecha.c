#include <stdio.h>
#include <stdlib.h>
#include <utime.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

int main()
{
    const char *ruta_archivo = "/media/adrianux/ADRI/archivo.txt";

    // Verifica si el archivo existe
    if (access(ruta_archivo, F_OK) != 0)
    {
        perror("El archivo no existe");
        return EXIT_FAILURE;
    }

    // Estructura para almacenar los nuevos tiempos
    struct utimbuf nuevo_tiempo;

    // Asignar tiempos personalizados (por ejemplo: 1 enero 2023 12:00:00)
    struct tm t;
    t.tm_year = 2023 - 1900; // Año desde 1900
    t.tm_mon = 0;            // Enero
    t.tm_mday = 1;           // Día
    t.tm_hour = 12;
    t.tm_min = 0;
    t.tm_sec = 0;
    t.tm_isdst = -1;

    time_t timestamp = mktime(&t);

    // Asignar mismo tiempo a acceso y modificación
    nuevo_tiempo.actime = timestamp;  // Último acceso
    nuevo_tiempo.modtime = timestamp; // Última modificación

    // Aplicar los cambios
    if (utime(ruta_archivo, &nuevo_tiempo) != 0)
    {
        perror("Error al modificar la fecha");
        return EXIT_FAILURE;
    }

    printf("Se modificó la fecha de acceso y modificación de %s a: %s", ruta_archivo, ctime(&timestamp));
    return EXIT_SUCCESS;
}
