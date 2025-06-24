#include <stdio.h>
#include <stdlib.h>

int main()
{
    FILE *origen, *destino;
    char buffer[1024];
    size_t bytes;

    origen = fopen("/media/adrianux/ADRI/documento.doc", "rb");
    if (origen == NULL)
    {
        perror("No se pudo abrir archivo origen");
        return 1;
    }

    destino = fopen("/media/adrianux/ADRI/documento_copy.doc", "wb");
    if (destino == NULL)
    {
        perror("No se pudo crear archivo copia");
        fclose(origen);
        return 1;
    }

    while ((bytes = fread(buffer, 1, sizeof(buffer), origen)) > 0)
    {
        fwrite(buffer, 1, bytes, destino);
    }

    fclose(origen);
    fclose(destino);

    printf("Copia realizada exitosamente.\n");
    return 0;
}
