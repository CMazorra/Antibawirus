#include <gtk/gtk.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gspawn.h>
#include <locale.h>

// Widgets globales para acceso desde funciones
GtkWidget *window;
GtkWidget *text_view;
GtkWidget *scroll;
GtkTextBuffer *buffer;
GtkWidget *button1;
GtkWidget *button2;
GtkWidget *button3;
GtkWidget *button4;
GtkWidget *exit_button;
GtkWidget *entry_start_port;
GtkWidget *entry_end_port;
GtkWidget *entry_cpu_threshold;
GtkWidget *entry_mem_threshold;
GString *usb_scan_output;
GMutex    usb_mutex;

void on_hide_text(GtkButton *btn, gpointer user_data)
{
    gtk_widget_hide(scroll);
    gtk_widget_hide(exit_button);
}

typedef struct
{
    GPid child_pid;
    GIOChannel *io_channel;
    GtkWidget *window;
    GtkTextBuffer *buffer;
} ProcessWindowData;

// Estructura para pasar datos al callback
typedef struct
{
    GPid child_pid;
    GIOChannel *io_channel;
    GtkWidget *window;
    GtkListStore *store;
} TableProcessData;

gboolean read_guardia_info(GIOChannel *source, GIOCondition cond, gpointer data)
{
    ProcessWindowData *pdata = (ProcessWindowData *)data;
    gchar buf[512];
    gsize bytes_read;
    GError *error = NULL;

    GIOStatus status = g_io_channel_read_chars(source, buf, sizeof(buf) - 1, &bytes_read, &error);
    if (status == G_IO_STATUS_NORMAL && bytes_read > 0)
    {
        buf[bytes_read] = '\0';
        gtk_text_buffer_insert_at_cursor(pdata->buffer, buf, -1);
        return TRUE;
    }
    if (status == G_IO_STATUS_EOF)
    {
        return FALSE;
    }
    return TRUE;
}

gboolean read_guardia_alertas_table(GIOChannel *source, GIOCondition cond, gpointer data)
{
    TableProcessData *pdata = (TableProcessData *)data;
    gchar *line = NULL;
    gsize len = 0;
    GError *error = NULL;

    GIOStatus status = g_io_channel_read_line(source, &line, &len, NULL, &error);
    if (status == G_IO_STATUS_NORMAL && line)
    {
        if (g_str_has_prefix(line, "NUEVA_ITERACION"))
         {
            gtk_list_store_clear(pdata->store); // Limpiar la tabla antes de agregar nueva iteración
          return TRUE;
          }

        if(!g_str_has_prefix(line, "ALERTA_USO_CPU") && !g_str_has_prefix(line, "ALERTA_PICO_CPU") && !g_str_has_prefix(line, "ALERTA_USO_RAM") && !g_str_has_prefix(line, "ALERTA_PICO_RAM"))
        {
            g_free(line);
            return TRUE;
        }
        gchar tipo[32] = {0};
        gchar nombre[128] = {0};
        gchar pid[32] = {0};
        float valor = 0.0;

        // Parsear la línea con sscanf
        // Ejemplo esperado:
        // ALERTA_USO_CPU: 12.34% | Nombre: firefox | PID: 1234
        
        g_strchomp(line);
        char valor_str[32] = {0};
        int parsed = sscanf(line, "%31[^:]: %31[^%%]%% | Nombre: %127[^|] | PID: %31s", tipo, valor_str, nombre, pid);
        
        if (parsed == 4)
        {
            float valor = g_ascii_strtod(valor_str, NULL);
            // Normalizar nombre del tipo para mostrar bonito
            gchar *tipo_mostrar = NULL;
            if (g_strcmp0(tipo, "ALERTA_USO_CPU") == 0) tipo_mostrar = "Uso CPU";
            else if (g_strcmp0(tipo, "ALERTA_USO_RAM") == 0) tipo_mostrar = "Uso RAM";
            else if (g_strcmp0(tipo, "ALERTA_PICO_CPU") == 0) tipo_mostrar = "Pico CPU";
            else if (g_strcmp0(tipo, "ALERTA_PICO_RAM") == 0) tipo_mostrar = "Pico RAM";
            else tipo_mostrar = tipo; // fallback

            GtkTreeIter iter;
            gtk_list_store_append(pdata->store, &iter);
            gtk_list_store_set(pdata->store, &iter,
                               0, pid,
                               1, tipo_mostrar,
                               2, g_strdup_printf("%.2f%%", valor),
                               3, nombre,
                               -1);
        }
        else
        {
            g_print("⚠️ Línea malformada: %s", line);
        }

        g_free(line);
        return TRUE;
    }
    else if (status == G_IO_STATUS_EOF)
    {
        return FALSE;
    }
    return TRUE;
}


// Callback para leer y parsear la salida de guardia_info
gboolean read_guardia_info_table(GIOChannel *source, GIOCondition cond, gpointer data)
{
    TableProcessData *pdata = (TableProcessData *)data;
    gchar *line = NULL;
    gsize len = 0;
    GError *error = NULL;

    GIOStatus status = g_io_channel_read_line(source, &line, &len, NULL, &error);
    if (status == G_IO_STATUS_NORMAL && line)
    {

        if (g_str_has_prefix(line, "NUEVA_ITERACION"))
         {
            gtk_list_store_clear(pdata->store);
          return TRUE;
          }

        
        if (!strchr(line, ';')) {
            g_free(line);
            return TRUE;
        }
        gchar **fields = g_strsplit(line, ";", -1); // divide en todos los ';'

        if (fields[0] && fields[1] && fields[2] && fields[3] && fields[4] && fields[5])
        {
            g_strchomp(fields[5]); // elimina salto de línea del campo "hilos"

            GtkTreeIter iter;
            gtk_list_store_append(pdata->store, &iter);
            gtk_list_store_set(pdata->store, &iter,
                               0, fields[0], // PID
                               1, fields[1], // Nombre
                               2, fields[2], // %CPU
                               3, fields[3], // %RAM
                               4, fields[4], // MemVirtual
                               5, fields[5], // Hilos
                               -1);
        }
        else
        {
            g_print("⚠️ Línea malformada: %s\n", line);
        }
        g_strfreev(fields);

        g_free(line);
        return TRUE;
    }

    if (status == G_IO_STATUS_EOF)
    {
        g_print("EOF alcanzado\n");
        return FALSE;
    }

    return TRUE;
}

// Detener el proceso y cerrar ventana
void stop_table_guardia_info(GtkButton *btn, gpointer user_data)
{
    TableProcessData *pdata = (TableProcessData *)user_data;
    if (!pdata)
        return; 

    if (pdata->child_pid > 0)
    {
        kill(pdata->child_pid, SIGTERM);
        pdata->child_pid = 0;
    }
    if (pdata->window)
    {
        gtk_widget_destroy(pdata->window);
        pdata->window = NULL;
    }
    if (pdata->io_channel)
    {
        g_io_channel_unref(pdata->io_channel);
        pdata->io_channel = NULL;
    }
    // Marca el puntero como NULL para evitar doble free
    user_data = NULL;
    g_free(pdata);
}
void on_alertas_clicked(GtkButton *btn, gpointer user_data)
{
    GtkWidget *alert_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(alert_window), "Alertas de Procesos");
    gtk_window_set_default_size(GTK_WINDOW(alert_window), 900, 500);

    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
                                    "window, treeview, .view { background: #232629; color: #e0e0e0; }"
                                    "treeview row { background: #232629; color: #e0e0e0; }"
                                    "treeview row:selected { background: #44475a; }",
                                    -1, NULL);
    GtkStyleContext *context = gtk_widget_get_style_context(alert_window);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(alert_window), vbox);

    // Modelo de la tabla (PID, Tipo, Valor)
    GtkListStore *store = gtk_list_store_new(4,
                                             G_TYPE_STRING, // PID
                                             G_TYPE_STRING, // Tipo de alerta
                                             G_TYPE_STRING,  // Valor
                                             G_TYPE_STRING  // Nombre del proceso
    );

    GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), TRUE);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1, "PID", renderer, "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1, "Tipo", renderer, "text", 1, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1, "Valor", renderer, "text", 2, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1, "Nombre", renderer, "text", 3, NULL);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), tree);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 5);

    GtkWidget *stop_button = gtk_button_new_with_label("Detener");
    gtk_box_pack_start(GTK_BOX(vbox), stop_button, FALSE, FALSE, 5);

    gtk_widget_show_all(alert_window);

    const gchar *cpu_text = gtk_entry_get_text(GTK_ENTRY(entry_cpu_threshold));
    const gchar *mem_text = gtk_entry_get_text(GTK_ENTRY(entry_mem_threshold));

    if (strlen(cpu_text) == 0 || strlen(mem_text) == 0) 
    {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(alert_window), GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "Debe ingresar ambos umbrales (CPU y RAM).");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return;
    }

    float cpu_umbral = atof(cpu_text);
    float ram_umbral = atof(mem_text);

    gchar *cpu_str = g_strdup_printf("%.2f", cpu_umbral);
    gchar *ram_str = g_strdup_printf("%.2f", ram_umbral);
    // Lanzar guardia_info y leer solo alertas
    gchar *argv[] = {"../Guardia del tesoro real/test", cpu_str, ram_str, NULL};
    GPid child_pid;
    gint out_fd;
    GError *error = NULL;

    if (!g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &child_pid, NULL, &out_fd, NULL, &error)) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(alert_window), GTK_DIALOG_MODAL,
        GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Error al ejecutar guardia_info.");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_error_free(error);
    g_free(cpu_str);
    g_free(ram_str);
    return;
}

    GIOChannel *io_channel = g_io_channel_unix_new(out_fd);

    TableProcessData *pdata = g_new0(TableProcessData, 1);
    pdata->child_pid = child_pid;
    pdata->io_channel = io_channel;
    pdata->window = alert_window;
    pdata->store = store;

    g_io_add_watch(io_channel, G_IO_IN | G_IO_HUP, read_guardia_alertas_table, pdata);
    g_signal_connect(stop_button, "clicked", G_CALLBACK(stop_table_guardia_info), pdata);

    g_free(cpu_str);
    g_free(ram_str);
}

void on_all_processes_clicked(GtkButton *btn, gpointer user_data)
{
    // Ventana para mostrar procesos
    GtkWidget *proc_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(proc_window), "Procesos en tiempo real");
    gtk_window_set_default_size(GTK_WINDOW(proc_window), 900, 500);

    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
                                    "window, treeview, .view { background: #232629; color: #e0e0e0; }"
                                    "treeview row { background: #232629; color: #e0e0e0; }"
                                    "treeview row:selected { background: #44475a; }",
                                    -1, NULL);
    GtkStyleContext *context = gtk_widget_get_style_context(proc_window);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(proc_window), vbox);

    // Modelo de la tabla
    GtkListStore *store = gtk_list_store_new(6,
                                             G_TYPE_STRING, // PID
                                             G_TYPE_STRING, // Nombre
                                             G_TYPE_STRING, // %CPU
                                             G_TYPE_STRING, // %RAM
                                             G_TYPE_STRING, // Memoria Virtual
                                             G_TYPE_STRING  // Hilos
    );

    // TreeView
    GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), TRUE);

    // Columnas
    GtkCellRenderer *renderer;
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1, "PID", renderer, "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1, "Nombre", renderer, "text", 1, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1, "% CPU", renderer, "text", 2, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1, "% RAM", renderer, "text", 3, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1, "Memoria Virtual (KB)", renderer, "text", 4, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1, "Hilos", renderer, "text", 5, NULL);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), tree);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 5);

    GtkWidget *stop_button = gtk_button_new_with_label("Detener");
    gtk_box_pack_start(GTK_BOX(vbox), stop_button, FALSE, FALSE, 5);

    gtk_widget_show_all(proc_window);

    const gchar *cpu_text = gtk_entry_get_text(GTK_ENTRY(entry_cpu_threshold));
    const gchar *mem_text = gtk_entry_get_text(GTK_ENTRY(entry_mem_threshold));

    if (strlen(cpu_text) == 0 || strlen(mem_text) == 0)
    {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(proc_window), GTK_DIALOG_MODAL,
            GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "Debe ingresar ambos umbrales (CPU y RAM).");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    float cpu_umbral = atof(cpu_text);
    float ram_umbral = atof(mem_text);
    gchar *cpu_str = g_strdup_printf("%.2f", cpu_umbral);
    gchar *ram_str = g_strdup_printf("%.2f", ram_umbral);

    // 👇 Lanzar el ejecutable con los argumentos
    gchar *argv[] = {"../Guardia del tesoro real/test", cpu_str, ram_str, NULL};
    GPid child_pid;
    gint out_fd;
    GError *error = NULL;

    if (!g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL,
        &child_pid, NULL, &out_fd, NULL, &error))
    {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(proc_window), GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Error al ejecutar guardia_info.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_error_free(error);
        g_free(cpu_str);
        g_free(ram_str);
        return;
    }

    GIOChannel *io_channel = g_io_channel_unix_new(out_fd);

    TableProcessData *pdata = g_new0(TableProcessData, 1);
    pdata->child_pid = child_pid;
    pdata->io_channel = io_channel;
    pdata->window = proc_window;
    pdata->store = store;

    g_io_add_watch(io_channel, G_IO_IN | G_IO_HUP, read_guardia_info_table, pdata);
    g_signal_connect(stop_button, "clicked", G_CALLBACK(stop_table_guardia_info), pdata);

    g_free(cpu_str);
    g_free(ram_str);
}

void on_monitoring_clicked(GtkButton *btn, gpointer user_data)
{
    const gchar *cpu_text = gtk_entry_get_text(GTK_ENTRY(entry_cpu_threshold));
    const gchar *mem_text = gtk_entry_get_text(GTK_ENTRY(entry_mem_threshold));

    if (cpu_text[0] == '\0' || mem_text[0] == '\0')
    {
        GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
                                                   "Debe especificar ambos umbrales de CPU y Memoria.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    // Crear nueva ventana
    GtkWidget *monitor_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(monitor_window), "Monitoreo de Procesos");
    gtk_window_set_default_size(GTK_WINDOW(monitor_window), 400, 200);

    // Crear caja vertical
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(monitor_window), vbox);

    // Botón de Alertas
    GtkWidget *alert_button = gtk_button_new_with_label("Alertas");
    gtk_box_pack_start(GTK_BOX(vbox), alert_button, TRUE, TRUE, 10);

    // Botón de Todos los procesos
    GtkWidget *all_processes_button = gtk_button_new_with_label("Todos los procesos");
    gtk_box_pack_start(GTK_BOX(vbox), all_processes_button, TRUE, TRUE, 10);

    g_signal_connect(all_processes_button, "clicked", G_CALLBACK(on_all_processes_clicked), NULL);
    g_signal_connect(alert_button, "clicked", G_CALLBACK(on_alertas_clicked), NULL);
    gtk_widget_show_all(monitor_window);
}

gpointer usb_scan_thread(gpointer _unused) {
    const char *cmd = "stdbuf -oL -eL '../Patrullas Fronterizas/fronteras' 2>&1";
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        g_mutex_lock(&usb_mutex);
        g_string_append(usb_scan_output,
            "Error al ejecutar escaneo USB.\n");
        g_mutex_unlock(&usb_mutex);
        return NULL;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        g_mutex_lock(&usb_mutex);
        g_string_append(usb_scan_output, line);
        g_mutex_unlock(&usb_mutex);
    }
    pclose(fp);
    return NULL;
}

void on_scan_clicked(GtkButton *btn, gpointer user_data) {
    gchar *snapshot;

    // Toma snapshot protegido por mutex
    g_mutex_lock(&usb_mutex);
    snapshot = g_string_free(g_string_new(usb_scan_output->str), FALSE);
    g_mutex_unlock(&usb_mutex);

    gtk_widget_show(scroll);
    gtk_widget_show(exit_button);
    gtk_text_buffer_set_text(buffer, snapshot, -1);

    g_free(snapshot);
}

void on_scan_port_clicked(GtkButton *btn, gpointer user_data)
{
    const gchar *start_text = gtk_entry_get_text(GTK_ENTRY(entry_start_port));
    const gchar *end_text = gtk_entry_get_text(GTK_ENTRY(entry_end_port));

    if (start_text[0] == '\0' || end_text[0] == '\0')
    {
        gtk_text_buffer_set_text(buffer, "Debe especificar ambos puertos.\n", -1);
        return;
    }

    gchar range_arg[64];
    g_snprintf(range_arg, sizeof(range_arg), "%s-%s", start_text, end_text);

    gchar cmd[256];
    g_snprintf(cmd, sizeof(cmd), "\"../Defensores de las Murallas/scanner\" %s", range_arg);

    gtk_widget_show(scroll);
    gtk_widget_show(exit_button);
    gtk_text_buffer_set_text(buffer, "Iniciando escaneo de puertos...\n", -1);

    FILE *fp = popen(cmd, "r");
    if (!fp)
    {
        gtk_text_buffer_insert_at_cursor(buffer,
                                         "Error al ejecutar el escáner de puertos.\n", -1);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp))
    {
        gtk_text_buffer_insert_at_cursor(buffer, line, -1);
        while (gtk_events_pending())
            gtk_main_iteration();
    }

    pclose(fp);
}

void do_all(GtkButton *btn, gpointer user_data)
{
    on_scan_clicked(btn, user_data);
    on_monitoring_clicked(btn, user_data);
    on_scan_port_clicked(btn, user_data);
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "C");
    gtk_init(&argc, &argv);

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(provider, "style.css", NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);

    // Create window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Antibawirus");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Vertical box to order widgets
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Button to start scan of USB devices
    button1 = gtk_button_new_with_label("Iniciar Escaneo de Dispositivos Conectados");
    g_signal_connect(button1, "clicked", G_CALLBACK(on_scan_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), button1, FALSE, FALSE, 5);

    // Button to start process monitoring
    button2 = gtk_button_new_with_label("Iniciar Monitoreo de Procesos");
    g_signal_connect(button2, "clicked", G_CALLBACK(on_monitoring_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), button2, FALSE, FALSE, 5);

    // Button to start local ports scan
    button3 = gtk_button_new_with_label("Iniciar Escaneo de Puertos Locales");
    g_signal_connect(button3, "clicked", G_CALLBACK(on_scan_port_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), button3, FALSE, FALSE, 5);

    // Create grid for ports and thresholds
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

    // Label and Entry for starting port
    GtkWidget *label_start = gtk_label_new("Puerto inicial:");
    entry_start_port = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_start_port), "ej. 1");
    gtk_grid_attach(GTK_GRID(grid), label_start, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_start_port, 1, 0, 1, 1);

    // Label and Entry for cpu usage threshold
    GtkWidget *label_cou = gtk_label_new("Umbral de CPU (%):");
    entry_cpu_threshold = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_cpu_threshold), "ej. 80");
    gtk_grid_attach(GTK_GRID(grid), label_cou, 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_cpu_threshold, 3, 0, 1, 1);

    // Label and Entry for ending port
    GtkWidget *label_end = gtk_label_new("Puerto final:");
    entry_end_port = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_end_port), "ej. 1024");
    gtk_grid_attach(GTK_GRID(grid), label_end, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_end_port, 1, 1, 1, 1);

    // Label and Entry for memory usage threshold
    // GtkWidget *hbox_mem = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *label_mem = gtk_label_new("Umbral de Memoria (%):");
    entry_mem_threshold = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_mem_threshold), "ej. 90");
    gtk_grid_attach(GTK_GRID(grid), label_mem, 2, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_mem_threshold, 3, 1, 1, 1);

    // Add grid to vertical box
    gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 5);

    // Button to do all
    button4 = gtk_button_new_with_label("Iniciar todo");
    g_signal_connect(button4, "clicked", G_CALLBACK(do_all), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), button4, FALSE, FALSE, 5);

    // Button to close text box
    exit_button = gtk_button_new_with_label("Cerrar Resultados");
    g_signal_connect(exit_button, "clicked", G_CALLBACK(on_hide_text), NULL);
    gtk_box_pack_end(GTK_BOX(vbox), exit_button, FALSE, FALSE, 5);

    // Area to show results
    text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), text_view);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 5);

    gtk_widget_show_all(window);

    usb_scan_output = g_string_new(NULL);
    g_mutex_init(&usb_mutex);

    // Lanza el hilo
    g_thread_new("usb-scan", usb_scan_thread, NULL);

    gtk_widget_hide(scroll);
    gtk_widget_hide(exit_button);
    gtk_main();

    if (usb_scan_output)
    {
        g_string_free(usb_scan_output, TRUE);
    }

    return 0;
}