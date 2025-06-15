#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>

int main()
{
    const char *ruta = "/media/adrianux/ADRI/archivo.txt";

    // Obtener UID y GID del usuario nobody
    struct passwd *pw = getpwnam("nobody");
    if (pw == NULL)
    {
        perror("No se encontró el usuario 'nobody'");
        return 1;
    }

    uid_t uid = pw->pw_uid;
    gid_t gid = pw->pw_gid;

    // Cambiar propietario del archivo
    if (chown(ruta, uid, gid) != 0)
    {
        perror("Error al cambiar propietario");
        return 1;
    }

    printf("Propietario cambiado a 'nobody'\n");
    return 0;
}
