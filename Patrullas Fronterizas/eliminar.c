#include <stdio.h>
#include <stdlib.h>

int main()
{
    const char *ruta = "/media/adrianux/ADRI/formulario.exe";

    if (remove(ruta) == 0)
    {
        printf("El archivo %s fue eliminado exitosamente.\n", ruta);
    }
    else
    {
        perror("Error al eliminar el archivo");
    }

    return 0;
}
