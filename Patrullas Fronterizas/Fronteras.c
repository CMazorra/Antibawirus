#include <stdio.h>
#include <stdlib.h>
#include <libudev.h>
#include <string.h>
#include <dirent.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <mntent.h>
#include <libmount/libmount.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

typedef struct
{
    char *ruta_completa;
    char *nombre;
    char *extension;
    off_t tamano;               // tamaño en bytes
    mode_t permisos;            // permisos estilo chmod
    time_t ultima_modificacion; // timestamp
    uid_t owner_uid;            // dueño
    gid_t group_gid;            // grupo
} ArchivoInfo;

const char *extension(const char *nombre)
{
    const char *p = strrchr(nombre, '.');
    return (p && p != nombre) ? p + 1 : ""; // sin el punto
}

char *nombre_sin_extension(const char *filename)
{
    const char *punto = strrchr(filename, '.');
    if (punto)
    {
        size_t len = punto - filename;
        char *nombre = malloc(len + 1);
        if (!nombre)
            return NULL;
        strncpy(nombre, filename, len);
        nombre[len] = '\0';
        return nombre;
    }
    return strdup(filename);
}

void listar_archivos(const char *path, ArchivoInfo **lista, int *count)
{
    DIR *dir;
    struct dirent *entry;
    char fullpath[2048];

    if (!(dir = opendir(path)))
    {
        fprintf(stderr, "No se pudo abrir el directorio %s: %s\n", path, strerror(errno));
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        struct stat statbuf;
        if (stat(fullpath, &statbuf) != 0)
            continue;

        // Crear y llenar estructura
        ArchivoInfo info;
        info.ruta_completa = strdup(fullpath);
        info.nombre = nombre_sin_extension(entry->d_name);
        info.extension = strdup(extension(entry->d_name));
        info.tamano = statbuf.st_size;
        info.permisos = statbuf.st_mode;
        info.ultima_modificacion = statbuf.st_mtime;
        info.owner_uid = statbuf.st_uid;
        info.group_gid = statbuf.st_gid;

        // Agregar a la lista
        *lista = realloc(*lista, sizeof(ArchivoInfo) * (*count + 1));
        (*lista)[*count] = info;
        (*count)++;

        // Si es directorio, entrar recursivamente
        if (S_ISDIR(statbuf.st_mode))
        {
            listar_archivos(fullpath, lista, count);
        }
    }

    closedir(dir);
}

const char *obtener(const char *device)
{
    FILE *mtab = setmntent("/etc/mtab", "r");
    if (!mtab)
    {
        return NULL;
    }

    struct mntent *ent;
    while ((ent = getmntent(mtab)) != NULL)
    {
        if (strcmp(ent->mnt_fsname, device) == 0)
        {
            endmntent(mtab);
            return strdup(ent->mnt_dir);
        }
    }

    endmntent(mtab);
    return NULL;
}

struct udev_list_entry *actualizar(struct udev *context)
{
    struct udev_enumerate *enumerate = udev_enumerate_new(context);
    udev_enumerate_add_match_subsystem(enumerate, "block");
    udev_enumerate_add_match_property(enumerate, "ID_USB_DRIVER", "usb-storage");
    udev_enumerate_scan_devices(enumerate);
    return udev_enumerate_get_list_entry(enumerate);
}

void mostrar(struct udev_list_entry *lista, struct udev *context)
{
    int i = 0;
    struct udev_list_entry *entry;
    udev_list_entry_foreach(entry, lista)
    {
        const char *path = udev_list_entry_get_name(entry);
        struct udev_device *dev = udev_device_new_from_syspath(context, path);

        if (dev)
        {
            struct udev_device *parent = udev_device_get_parent_with_subsystem_devtype(
                dev, "usb", "usb_device");

            const char *vendor = parent ? udev_device_get_sysattr_value(parent, "manufacturer") : NULL;
            const char *product = parent ? udev_device_get_sysattr_value(parent, "product") : NULL;
            const char *devnode = udev_device_get_devnode(dev);

            printf("Dispositivo %d:  Fabricante: %s  Producto: %s\n", ++i, vendor ? vendor : "Desconocido", product ? product : "Desconocido");
            udev_device_unref(dev);
        }
    }
}

void alerta(struct udev *context, struct udev_list_entry *lista_anterior)
{
    struct udev_list_entry *lista_actual = actualizar(context);
    struct udev_list_entry *entry_anterior;
    udev_list_entry_foreach(entry_anterior, lista_anterior)
    {
        int encontrado = 0;
        const char *path_anterior = udev_list_entry_get_name(entry_anterior);

        struct udev_list_entry *entry_actual;
        udev_list_entry_foreach(entry_actual, lista_actual)
        {
            if (strcmp(path_anterior, udev_list_entry_get_name(entry_actual)) == 0)
            {
                encontrado = 1;
                break;
            }
        }

        if (!encontrado)
        {
            printf("Dispositivo desconectado\n");
        }
    }

    struct udev_list_entry *entry_actual;
    udev_list_entry_foreach(entry_actual, lista_actual)
    {
        int encontrado = 0;
        const char *path_actual = udev_list_entry_get_name(entry_actual);
        struct udev_list_entry *entry_anterior;
        udev_list_entry_foreach(entry_anterior, lista_anterior)
        {
            if (strcmp(path_actual, udev_list_entry_get_name(entry_anterior)) == 0)
            {
                encontrado = 1;
                break;
            }
        }

        if (!encontrado)
        {
            struct udev_device *dev = udev_device_new_from_syspath(context, path_actual);
            if (dev)
            {
                struct udev_device *parent = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
                const char *vendor = parent ? udev_device_get_sysattr_value(parent, "manufacturer") : NULL;
                const char *product = parent ? udev_device_get_sysattr_value(parent, "product") : NULL;
                printf("Dispositivo conectado: Fabricante: %s - Producto: %s\n", vendor ? vendor : "Desconocido", product ? product : "Desconocido");
                udev_device_unref(dev);
            }
        }
    }
}
int ensenar(struct udev_list_entry *lista, struct udev *contexto, ArchivoInfo **archivos_out, int *cantidad_total)
{
    ArchivoInfo *archivos = NULL;
    int capacidad = 100;
    int total = 0;
    archivos = malloc(sizeof(ArchivoInfo) * capacidad);
    struct udev_list_entry *entrada;
    udev_list_entry_foreach(entrada, lista)
    {
        const char *ruta = udev_list_entry_get_name(entrada);
        struct udev_device *dispositivo = udev_device_new_from_syspath(contexto, ruta);
        if (!dispositivo)
            continue;
        const char *nodo = udev_device_get_devnode(dispositivo);
        if (nodo)
        {
            const char *punto_montaje = obtener(nodo);
            if (punto_montaje)
            {
                ArchivoInfo *archivos_temp = NULL;
                int cantidad_local = 0;
                listar_archivos(punto_montaje, &archivos_temp, &cantidad_local);
                if (total + cantidad_local > capacidad)
                {
                    capacidad = (total + cantidad_local) * 2;
                    archivos = realloc(archivos, sizeof(ArchivoInfo) * capacidad);
                }
                for (int i = 0; i < cantidad_local; i++)
                {
                    archivos[total++] = archivos_temp[i];
                }
                free(archivos_temp);
                free((void *)punto_montaje);
            }
        }
        udev_device_unref(dispositivo);
    }
    *archivos_out = archivos;
    *cantidad_total = total;
    return 0;
}
void tamano(ArchivoInfo *archivos_viejos, int cantidad_viejos, ArchivoInfo *archivos_nuevos, int cantidad_nuevos)
{
    for (int i = 0; i < cantidad_viejos; i++)
    {
        for (int j = 0; j < cantidad_nuevos; j++)
        {
            if (strcmp(archivos_viejos[i].nombre, archivos_nuevos[j].nombre) == 0 && strcmp(archivos_viejos[i].extension, archivos_nuevos[j].extension) == 0)
            {
                long diferencia = archivos_nuevos[j].tamano - archivos_viejos[i].tamano;
                if (diferencia >= 10 * 1024 * 1024) // 10 MB
                {
                    printf("El archivo %s.%s ha aumentado su tamaño en %ld bytes\n", archivos_nuevos[j].nombre, archivos_nuevos[j].extension, diferencia);
                }
            }
        }
    }
}

void extension_permisos(ArchivoInfo *archivos_viejos, int cantidad_viejos, ArchivoInfo *archivos_nuevos, int cantidad_nuevos)
{
    if (cantidad_nuevos == cantidad_viejos)
    {
        for (int i = 0; i < cantidad_viejos; i++)
        {
            for (int j = 0; j < cantidad_nuevos; j++)
            {
                if (strcmp(archivos_viejos[i].nombre, archivos_nuevos[j].nombre) == 0 && archivos_viejos[i].tamano == archivos_nuevos[j].tamano)
                {
                    if (strcmp(archivos_viejos[i].extension, archivos_nuevos[j].extension) != 0)
                    {
                        printf("El archivo %s.%s ha cambiado su extencion a %s\n", archivos_viejos[i].nombre, archivos_viejos[i].extension, archivos_nuevos[j].extension);
                        printf("el archivo %s tiene permiso %o\n", archivos_viejos[i].nombre, archivos_viejos[j].permisos);
                    }
                }
            }
        }
    }
}

void anadir_eliminar(ArchivoInfo *archivos_viejos, int cantidad_viejos, ArchivoInfo *archivos_nuevos, int cantidad_nuevos)
{
    if (cantidad_viejos > cantidad_nuevos)
    {
        for (int i = 0; i < cantidad_viejos; i++)
        {
            int encontrado = 0;
            for (int j = 0; j < cantidad_nuevos; j++)
            {
                if (strcmp(archivos_viejos[i].nombre, archivos_nuevos[j].nombre) == 0 && strcmp(archivos_viejos[i].extension, archivos_nuevos[j].extension) == 0)
                {
                    encontrado += 1;
                }
            }
            if (encontrado == 0)
            {
                printf("El archivo %s.%s ha sido eliminado\n", archivos_viejos[i].nombre, archivos_viejos[i].extension);
            }
        }
    }
    else if (cantidad_viejos < cantidad_nuevos)
    {
        for (int i = 0; i < cantidad_nuevos; i++)
        {
            int encontrado = 0;
            for (int j = 0; j < cantidad_viejos; j++)
            {
                if (strcmp(archivos_viejos[j].nombre, archivos_nuevos[i].nombre) == 0 && strcmp(archivos_viejos[j].extension, archivos_nuevos[i].extension) == 0)
                {
                    encontrado += 1;
                }
            }
            if (encontrado == 0)
            {
                printf("El archivo %s.%s ha sido anadido\n", archivos_nuevos[i].nombre, archivos_nuevos[i].extension);
            }
        }
    }
}

int main()
{
    struct udev *context = udev_new();
    struct udev_list_entry *lista_dispositivos = actualizar(context);
    mostrar(lista_dispositivos, context);
    ArchivoInfo *lista_archivos = NULL;
    int cantidad_viejos = 0;
    ensenar(lista_dispositivos, context, &lista_archivos, &cantidad_viejos);
    while (1)
    {
        alerta(context, lista_dispositivos);
        ArchivoInfo *archivos_nuevos = NULL;
        int cantidad_nuevos = 0;
        ensenar(lista_dispositivos, context, &archivos_nuevos, &cantidad_nuevos);
        tamano(lista_archivos, cantidad_viejos, archivos_nuevos, cantidad_nuevos);
        anadir_eliminar(lista_archivos, cantidad_viejos, archivos_nuevos, cantidad_nuevos);
        cambia_extension(lista_archivos, cantidad_viejos, archivos_nuevos, cantidad_nuevos);
        for (int i = 0; i < cantidad_viejos; i++)
        {
            free(lista_archivos[i].nombre);
            free(lista_archivos[i].extension);
            free(lista_archivos[i].ruta_completa);
        }
        free(lista_archivos);
        lista_archivos = archivos_nuevos;
        cantidad_viejos = cantidad_nuevos;
        lista_dispositivos = actualizar(context);
        sleep(5);
    }
    udev_unref(context);
    return 0;
}