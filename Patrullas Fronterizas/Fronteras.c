#include <stdio.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>

libusb_device **actualizar(libusb_context **context)
{
    libusb_device **list = NULL;
    ssize_t count = libusb_get_device_list(*context, &list);
    return list;
}

libusb_device **mostrar(libusb_context **context)
{
    libusb_init(context);
    libusb_device **list = NULL;
    ssize_t count = libusb_get_device_list(*context, &list);

    for (ssize_t i = 0; i < count; i++)
    {
        libusb_device *device = list[i];
        struct libusb_device_descriptor desc;
        libusb_get_device_descriptor(device, &desc);

        printf("Dispositivo %ld: ID %04x:%04x\n", i, desc.idVendor, desc.idProduct);
    }

    return list;
}
void alerta(libusb_device **new_list, libusb_device **list, ssize_t count_new_list, ssize_t count_list)
{
    if (count_list > count_new_list)
    {
        int *array = (int *)malloc(sizeof(int) * count_list);
        for (ssize_t i = 0; i < count_list; i++)
        {
            array[i] = 0;
        }
        for (ssize_t i = 0; i < count_list; i++)
        {
            struct libusb_device_descriptor desc;
            libusb_get_device_descriptor(list[i], &desc);
            for (ssize_t j = 0; j < count_new_list; j++)
            {
                struct libusb_device_descriptor new_desc;
                libusb_get_device_descriptor(new_list[j], &new_desc);
                if (desc.idVendor == new_desc.idVendor && desc.idProduct == new_desc.idProduct)
                {
                    array[i] += 1;
                }
            }
        }
        for (ssize_t i = 0; i < count_list; i++)
        {
            if (array[i] == 0)
            {
                struct libusb_device_descriptor desc;
                libusb_get_device_descriptor(list[i], &desc);
                printf(" El dispositivo %ld: ID %04x:%04x ha sido eliminado\n", i, desc.idVendor, desc.idProduct);
            }
        }
    }
    else if (count_list < count_new_list)
    {
        int *array = (int *)malloc(sizeof(int) * count_new_list);
        for (ssize_t i = 0; i < count_new_list; i++)
        {
            array[i] = 0;
        }
        for (ssize_t i = 0; i < count_new_list; i++)
        {
            struct libusb_device_descriptor new_desc;
            libusb_get_device_descriptor(new_list[i], &new_desc);
            for (ssize_t j = 0; j < count_list; j++)
            {
                struct libusb_device_descriptor desc;
                libusb_get_device_descriptor(list[j], &desc);
                if (desc.idVendor == new_desc.idVendor && desc.idProduct == new_desc.idProduct)
                {
                    array[i] += 1;
                }
            }
        }
        for (ssize_t i = 0; i < count_list; i++)
        {
            if (array[i] == 0)
            {
                struct libusb_device_descriptor desc;
                libusb_get_device_descriptor(new_list[i], &desc);
                printf(" El dispositivo %ld: ID %04x:%04x ha sido añadido\n", i, desc.idVendor, desc.idProduct);
            }
        }
    }
}

int main()
{
    libusb_context *context = NULL;
    libusb_device **list = mostrar(&context);
    ssize_t count_list = libusb_get_device_list(context, &list);
    int true = 0;
    while (true == 0)
    {
        libusb_device **new_list = actualizar(&context);
        ssize_t count_new_list = libusb_get_device_list(context, &new_list);
        alerta(new_list, list, count_new_list, count_list);
        list = new_list;
        count_list = count_new_list;
    }

    return 0;
}