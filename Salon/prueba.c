#include <gtk/gtk.h>
#include <glib.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

// Estructura para manejar los datos del proceso
typedef struct {
    GPid child_pid;
    GIOChannel *io_channel;
    GtkWidget *window;
    GtkListStore *store;
} ProcessMonitorData;

// Leer y parsear cada línea que imprime el proceso hijo
gboolean read_monitor_output(GIOChannel *source, GIOCondition cond, gpointer data) {
    ProcessMonitorData *pdata = (ProcessMonitorData *)data;
    gchar *line = NULL;
    gsize len = 0;
    GError *error = NULL;

    GIOStatus status = g_io_channel_read_line(source, &line, &len, NULL, &error);
    if (status == G_IO_STATUS_NORMAL && line) {
        gchar **fields = g_strsplit(line, ";", -1);

        if (g_strv_length(fields) >= 6) {
            // Limpiar tabla antes de insertar nuevos datos
            gtk_list_store_clear(pdata->store);

            GtkTreeIter iter;
            gtk_list_store_append(pdata->store, &iter);
            gtk_list_store_set(pdata->store, &iter,
                0, fields[0], // PID
                1, fields[1], // Nombre
                2, fields[2], // %CPU
                3, fields[3], // %RAM
                4, fields[4], // Memoria Virtual
                5, fields[5], // Hilos
                -1);
        }

        g_strfreev(fields);
        g_free(line);
        return TRUE;
    }

    if (status == G_IO_STATUS_EOF) {
        return FALSE;
    }

    return TRUE;
}

// Detener el proceso hijo y cerrar la ventana
void stop_monitor(GtkButton *btn, gpointer user_data) {
    ProcessMonitorData *pdata = (ProcessMonitorData *)user_data;
    if (pdata->child_pid > 0) {
        kill(pdata->child_pid, SIGTERM);
        pdata->child_pid = 0;
    }
    gtk_widget_destroy(pdata->window);
    if (pdata->io_channel) g_io_channel_unref(pdata->io_channel);
    g_free(pdata);
}

// Crear y mostrar la ventana con la tabla de procesos
void show_monitor_window() {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Monitoreo de Procesos");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 500);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkListStore *store = gtk_list_store_new(6,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), TRUE);

    const gchar *columns[] = { "PID", "Nombre", "%CPU", "%RAM", "Mem. Virtual (KB)", "Hilos" };
    for (int i = 0; i < 6; i++) {
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1,
            columns[i], renderer, "text", i, NULL);
    }

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), tree);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 5);

    GtkWidget *stop_button = gtk_button_new_with_label("Detener Monitoreo");
    gtk_box_pack_start(GTK_BOX(vbox), stop_button, FALSE, FALSE, 5);

    gtk_widget_show_all(window);

    // Ruta al ejecutable (ajusta si está en otro lugar)
    gchar *argv[] = {"../Guardia del tesoro real/test", NULL};
    GPid child_pid;
    gint out_fd;
    GError *error = NULL;

    if (!g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL,
        &child_pid, NULL, &out_fd, NULL, &error)) {

        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
            GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
            "Error al ejecutar el monitor de procesos: %s", error->message);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    // Canal para leer la salida del ejecutable
    GIOChannel *channel = g_io_channel_unix_new(out_fd);
    g_io_channel_set_encoding(channel, NULL, NULL);
    g_io_channel_set_buffered(channel, FALSE);

    ProcessMonitorData *pdata = g_new0(ProcessMonitorData, 1);
    pdata->child_pid = child_pid;
    pdata->io_channel = channel;
    pdata->window = window;
    pdata->store = store;

    g_io_add_watch(channel, G_IO_IN | G_IO_HUP, read_monitor_output, pdata);
    g_signal_connect(stop_button, "clicked", G_CALLBACK(stop_monitor), pdata);
    g_signal_connect(window, "destroy", G_CALLBACK(stop_monitor), pdata);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    show_monitor_window();
    gtk_main();
    return 0;
}
