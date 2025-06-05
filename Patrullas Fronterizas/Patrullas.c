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

void list_files(const char *path, int depth)
{
    DIR *dir;
    struct dirent *entry;
    char fullpath[1024];

    if (!(dir = opendir(path)))
    {
        printf("  No se pudo abrir el directorio: %s\n", strerror(errno));
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        for (int i = 0; i < depth; i++)
        {
            printf("  ");
        }
        printf("├─ %s\n", entry->d_name);

        struct stat statbuf;
        if (stat(fullpath, &statbuf) == 0 && S_ISDIR(statbuf.st_mode))
        {
            list_files(fullpath, depth + 1);
        }
    }
    closedir(dir);
}

const char *get_mount_point(const char *device)
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
            return strdup(ent->mnt_dir); // Necesita ser liberado después
        }
    }

    endmntent(mtab);
    return NULL;
}

int main()
{
    struct udev *context = udev_new();
    if (!context)
    {
        fprintf(stderr, "Error al crear el contexto udev\n");
        return 1;
    }

    struct udev_enumerate *enumerate = udev_enumerate_new(context);
    udev_enumerate_add_match_subsystem(enumerate, "block");
    udev_enumerate_add_match_property(enumerate, "ID_USB_DRIVER", "usb-storage");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry *entry;

    printf("Dispositivos USB de almacenamiento conectados:\n");
    printf("=============================================\n\n");

    int count = 0;

    udev_list_entry_foreach(entry, devices)
    {
        const char *syspath = udev_list_entry_get_name(entry);
        struct udev_device *device = udev_device_new_from_syspath(context, syspath);

        if (device)
        {
            const char *devnode = udev_device_get_devnode(device);
            const char *vendor = udev_device_get_property_value(device, "ID_VENDOR");
            const char *model = udev_device_get_property_value(device, "ID_MODEL");
            const char *label = udev_device_get_property_value(device, "ID_FS_LABEL");
            const char *fstype = udev_device_get_property_value(device, "ID_FS_TYPE");

            if (devnode && strstr(devnode, "/dev/sd") != NULL)
            {
                count++;

                printf("\nDispositivo #%d\n", count);
                if (vendor)
                    printf("  Fabricante: %s\n", vendor);
                if (model)
                    printf("  Modelo: %s\n", model);
                if (label)
                    printf("  Etiqueta: %s\n", label);
                if (fstype)
                    printf("  Tipo de sistema de archivos: %s\n", fstype);
                printf("  Nodo: %s\n", devnode);

                // Obtener punto de montaje actual
                const char *mount_point = get_mount_point(devnode);

                if (mount_point)
                {
                    printf("  Ya montado en: %s\n", mount_point);
                    printf("  Contenido del dispositivo:\n");
                    list_files(mount_point, 1);
                    free((void *)mount_point); // Liberar memoria asignada por strdup
                }
                else
                {
                    printf("  El dispositivo no está montado\n");

                    // Intentar montar solo si no está montado
                    char mount_point_temp[] = "/tmp/usb_mount_XXXXXX";
                    if (mkdtemp(mount_point_temp))
                    {
                        if (mount(devnode, mount_point_temp, fstype ? fstype : "auto", MS_RDONLY, NULL) == 0)
                        {
                            printf("  Montado temporalmente en: %s\n", mount_point_temp);
                            printf("  Contenido del dispositivo:\n");
                            list_files(mount_point_temp, 1);
                            umount(mount_point_temp);
                        }
                        else
                        {
                            printf("  Error al montar: %s\n", strerror(errno));
                        }
                        rmdir(mount_point_temp);
                    }
                }
            }

            udev_device_unref(device);
        }
    }

    if (count == 0)
    {
        printf("\nNo se encontraron dispositivos USB de almacenamiento conectados.\n");
    }
    else
    {
        printf("\nTotal de dispositivos USB de almacenamiento encontrados: %d\n", count);
    }

    udev_enumerate_unref(enumerate);
    udev_unref(context);

    return 0;
}