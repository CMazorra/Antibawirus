#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <utime.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

#define TARGET "reporte.txt"

int main() {
    // Crear archivo
    FILE *f = fopen(TARGET, "w");
    if (!f) {
        perror("Error al crear archivo");
        exit(1);
    }
    fprintf(f, "Contenido confidencial\n");
    fclose(f);
    
    // Cambiar timestamp (1 de enero de 2020)
    struct utimbuf new_times;
    new_times.actime = 1577836800;  // 2020-01-01 00:00:00 UTC
    new_times.modtime = 1577836800;
    
    if (utime(TARGET, &new_times) < 0) {
        perror("utime");
        exit(1);
    }
    
    // Cambiar ownership a nobody
    struct passwd *pwd = getpwnam("nobody");
    if (!pwd) {
        perror("getpwnam");
        exit(1);
    }
    
    /*if (chown(TARGET, pwd->pw_uid, -1) < 0) {
        perror("chown");
        exit(1);
    }*/
    
    return 0;
}