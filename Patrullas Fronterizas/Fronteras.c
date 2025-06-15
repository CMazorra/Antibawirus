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
            const char *devnode = udev_device_get_devnode(dev);

            // Verificar que el dispositivo esté montado
            const char *punto_montaje = obtener(devnode);
            if (punto_montaje == NULL)
            {
                udev_device_unref(dev);
                continue; // Si no está montado, lo saltamos
            }
            struct udev_device *parent = udev_device_get_parent_with_subsystem_devtype(
                dev, "usb", "usb_device");

            const char *vendor = parent ? udev_device_get_sysattr_value(parent, "manufacturer") : NULL;
            const char *product = parent ? udev_device_get_sysattr_value(parent, "product") : NULL;

            printf("Dispositivo %d:  Fabricante: %s  Producto: %s\n", ++i, vendor ? vendor : "Desconocido", product ? product : "Desconocido");
            udev_device_unref(dev);
        }
    }
}
int contar_dispositivos(struct udev_list_entry *lista)
{
    int count = 0;
    struct udev_list_entry *entry;
    udev_list_entry_foreach(entry, lista)
    {
        const char *devnode = udev_list_entry_get_name(entry);
        const char *mnt = obtener(devnode); // Solo si está montado
        if (mnt != NULL)
        {
            count++;
            free((void *)mnt);
        }
    }
    return count;
}
int alerta(struct udev *context, struct udev_list_entry *lista_anterior, struct udev_list_entry *lista_actual)
{

    int actuales = 0;
    struct udev_list_entry *entry_anterior;
    udev_list_entry_foreach(entry_anterior, lista_anterior)
    {
        const char *path_anterior = udev_list_entry_get_name(entry_anterior);
        int encontrado = 0;

        struct udev_list_entry *entry_actual;
        udev_list_entry_foreach(entry_actual, lista_actual)
        {
            const char *path_actual = udev_list_entry_get_name(entry_actual);
            if (strcmp(path_anterior, path_actual) == 0)
            {
                encontrado = 1;
                break;
            }
        }
        if (!encontrado)
        {
            printf("Dispositivo desconectado\n");
            break;
        }
    }

    struct udev_list_entry *entry_actual;
    udev_list_entry_foreach(entry_actual, lista_actual)
    {
        const char *path_actual = udev_list_entry_get_name(entry_actual);
        struct udev_device *dev_actual = udev_device_new_from_syspath(context, path_actual);

        if (!dev_actual)
            continue;

        const char *devnode_actual = udev_device_get_devnode(dev_actual);
        const char *mnt = obtener(devnode_actual);
        if (!mnt)
        {
            udev_device_unref(dev_actual);
            continue;
        }

        int encontrado = 0;
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
        free((void *)mnt);
        udev_device_unref(dev_actual);
        actuales++;
    }
    return actuales;
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
            if (strcmp(archivos_viejos[i].nombre, archivos_nuevos[j].nombre) == 0 && strcmp(archivos_viejos[i].extension, archivos_nuevos[j].extension) == 0 && strcmp(archivos_viejos[i].ruta_completa, archivos_nuevos[j].ruta_completa) == 0)
            {
                long diferencia = archivos_nuevos[j].tamano - archivos_viejos[i].tamano;
                if (diferencia >= 10 * 1024 * 1024) // 10 MB
                {
                    printf("El archivo %s.%s en %s ha aumentado su tamaño en %ld bytes\n", archivos_nuevos[j].nombre, archivos_nuevos[j].extension, archivos_nuevos[j].ruta_completa, diferencia);
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
                        printf("El archivo %s.%s en %s ha cambiado su extencion a %s\n", archivos_viejos[i].nombre, archivos_viejos[i].extension, archivos_viejos[i].ruta_completa, archivos_nuevos[j].extension);
                    }
                }
                if (strcmp(archivos_viejos[i].nombre, archivos_nuevos[j].nombre) == 0 && strcmp(archivos_viejos[i].extension, archivos_nuevos[j].extension) == 0 && strcmp(archivos_viejos[i].ruta_completa, archivos_nuevos[j].ruta_completa) == 0)
                {
                    // if (archivos_viejos[i].tamano == 47)
                    // {
                    //     printf("%s %o\n", archivos_viejos[i].nombre, archivos_viejos[i].permisos & 0777);
                    // }
                    if ((archivos_viejos[i].permisos & 0777) != (archivos_nuevos[j].permisos & 0777))
                    {
                        printf("El archivo %s.%s en %s ha cambiado sus permisos de %o a %o\n", archivos_viejos[i].nombre, archivos_viejos[i].extension, archivos_viejos[i].ruta_completa, archivos_viejos[i].permisos & 0777, archivos_nuevos[j].permisos & 0777);
                    }
                }
            }
        }
    }
}

void timestamps_ownership(ArchivoInfo *archivos_viejos, int cantidad_viejos, ArchivoInfo *archivos_nuevos, int cantidad_nuevos)
{
    if (cantidad_nuevos == cantidad_viejos)
    {
        for (int i = 0; i < cantidad_viejos; i++)
        {
            for (int j = 0; j < cantidad_nuevos; j++)
            {
                if (strcmp(archivos_viejos[i].nombre, archivos_nuevos[j].nombre) == 0 && strcmp(archivos_viejos[i].extension, archivos_nuevos[j].extension) == 0 && strcmp(archivos_viejos[i].ruta_completa, archivos_nuevos[j].ruta_completa) == 0)
                {
                    if (archivos_viejos[i].ultima_modificacion != archivos_nuevos[j].ultima_modificacion)
                    {
                        printf("El archivo %s.%s en %s ha cambiado su timestamps de %ld a %ld\n", archivos_viejos[i].nombre, archivos_viejos[i].extension, archivos_viejos[i].ruta_completa, archivos_viejos[i].ultima_modificacion, archivos_nuevos[j].ultima_modificacion);
                    }
                }
                if (strcmp(archivos_viejos[i].nombre, archivos_nuevos[j].nombre) == 0 && strcmp(archivos_viejos[i].extension, archivos_nuevos[j].extension) == 0 && strcmp(archivos_viejos[i].ruta_completa, archivos_nuevos[j].ruta_completa) == 0)
                {
                    if (archivos_viejos[i].owner_uid != archivos_nuevos[j].owner_uid)
                    {
                        printf("El archivo %s.%s en %s ha cambiado su ownership de %d a %d\n", archivos_viejos[i].nombre, archivos_viejos[i].extension, archivos_viejos[i].ruta_completa, archivos_viejos[i].owner_uid, archivos_nuevos[j].owner_uid);
                    }
                }
            }
        }
    }
}

int is_name(ArchivoInfo viejo, ArchivoInfo nuevo)
{
    // Verifica si el nombre del archivo nuevo comienza con el nombre del viejo
    return strncmp(nuevo.nombre, viejo.nombre, strlen(viejo.nombre)) == 0;
}

int copia(ArchivoInfo archivo, ArchivoInfo *archivos_viejos, int cantidad_viejos)
{
    for (int i = 0; i < cantidad_viejos; i++)
    {
        if (strcmp(archivos_viejos[i].extension, archivo.extension) == 0 && archivos_viejos[i].tamano == archivo.tamano && is_name(archivos_viejos[i], archivo))
        {

            printf("Posible copia detectada: %s se parece a %s\n", archivo.nombre, archivos_viejos[i].nombre);
            return 1; // se detectó una copia
        }
    }
    return 0;
}
void anadir_eliminar(ArchivoInfo *archivos_viejos, int cantidad_viejos, ArchivoInfo *archivos_nuevos, int cantidad_nuevos, int viejos, int actuales)
{
    if (cantidad_viejos > cantidad_nuevos)
    {
        for (int i = 0; i < cantidad_viejos; i++)
        {
            int encontrado = 0;
            for (int j = 0; j < cantidad_nuevos; j++)
            {
                if (strcmp(archivos_viejos[i].nombre, archivos_nuevos[j].nombre) == 0 && strcmp(archivos_viejos[i].extension, archivos_nuevos[j].extension) == 0 && strcmp(archivos_viejos[i].ruta_completa, archivos_nuevos[j].ruta_completa) == 0)
                {
                    encontrado += 1;
                }
            }
            if (encontrado == 0)
            {
                if (viejos == actuales)
                {
                    printf("El archivo %s.%s en %s ha sido eliminado\n", archivos_viejos[i].nombre, archivos_viejos[i].extension, archivos_viejos[i].ruta_completa);
                }
            }
        }
    }
    else if (cantidad_viejos < cantidad_nuevos && viejos == actuales)
    {
        for (int i = 0; i < cantidad_nuevos; i++)
        {
            int encontrado = 0;
            for (int j = 0; j < cantidad_viejos; j++)
            {
                if (strcmp(archivos_viejos[j].nombre, archivos_nuevos[i].nombre) == 0 && strcmp(archivos_viejos[j].extension, archivos_nuevos[i].extension) == 0 && strcmp(archivos_viejos[j].ruta_completa, archivos_nuevos[i].ruta_completa) == 0)
                {
                    encontrado += 1;
                }
            }
            if (encontrado == 0)
            {
                if (copia(archivos_nuevos[i], archivos_viejos, cantidad_viejos) == 0)
                {
                    printf("El archivo %s.%s en %s ha sido anadido\n", archivos_nuevos[i].nombre, archivos_nuevos[i].extension, archivos_nuevos[i].ruta_completa);
                }
            }
        }
    }
}

int main()
{
    struct udev *context = udev_new();
    struct udev_list_entry *lista_dispositivos = actualizar(context);
    int viejos = contar_dispositivos(lista_dispositivos);
    mostrar(lista_dispositivos, context);
    ArchivoInfo *lista_archivos = NULL;
    int cantidad_viejos = 0;
    ensenar(lista_dispositivos, context, &lista_archivos, &cantidad_viejos);
    while (1)
    {
        struct udev_list_entry *lista_disp_actuales = actualizar(context);
        int actuales = alerta(context, lista_dispositivos, lista_disp_actuales);
        ArchivoInfo *archivos_nuevos = NULL;
        int cantidad_nuevos = 0;
        ensenar(lista_disp_actuales, context, &archivos_nuevos, &cantidad_nuevos);
        tamano(lista_archivos, cantidad_viejos, archivos_nuevos, cantidad_nuevos);
        anadir_eliminar(lista_archivos, cantidad_viejos, archivos_nuevos, cantidad_nuevos, viejos, actuales);
        extension_permisos(lista_archivos, cantidad_viejos, archivos_nuevos, cantidad_nuevos);
        timestamps_ownership(lista_archivos, cantidad_viejos, archivos_nuevos, cantidad_nuevos);
        for (int i = 0; i < cantidad_viejos; i++)
        {
            free(lista_archivos[i].nombre);
            free(lista_archivos[i].extension);
            free(lista_archivos[i].ruta_completa);
        }
        free(lista_archivos);
        lista_archivos = archivos_nuevos;
        cantidad_viejos = cantidad_nuevos;
        lista_dispositivos = lista_disp_actuales;
        viejos = actuales;
        sleep(3);
    }
    udev_unref(context);
    return 0;
}