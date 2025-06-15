#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <stdlib.h>

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

void on_hide_text(GtkButton *btn, gpointer user_data) {
    gtk_widget_hide(scroll);
    gtk_widget_hide(exit_button);
}

void on_monitoring_clicked(GtkButton *btn, gpointer user_data)
{
    //Kelen   
}

void on_scan_clicked(GtkButton *btn, gpointer user_data)
{
    //Adriana
}

void on_scan_port_clicked(GtkButton *btn, gpointer user_data) {
    gtk_widget_show(scroll);
    gtk_widget_show(exit_button);
    gtk_text_buffer_set_text(buffer, "Iniciando escaneo...\n", -1);

    const gchar *scanner_path = "../Defensores\\ de\\ la\\ muralla/scanner";
    FILE *fp = popen(scanner_path, "r");

    if (!fp) {
        gtk_text_buffer_insert_at_cursor(buffer, "Error al ejecutar el escáner.\n", -1);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        gtk_text_buffer_insert_at_cursor(buffer, line, -1);

        // Refresh the interface to show the text as quick as possible
        while (gtk_events_pending())
            gtk_main_iteration();
    }

    pclose(fp);
}



void do_all(GtkButton *btn, gpointer user_data)
{
    on_scan_clicked();
    on_monitoring_clicked()
    on_scan_port_clicked(btn,user_data);
}

int main(int argc, char *argv[]) {
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

    //Button to do all
    button4 = gtk_button_new_with_label("Iniciar todo");
    g_signal_connect(button4, "clicked",G_CALLBACK(do_all), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), button4, FALSE, FALSE, 5);

    //Button to close text box
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
    gtk_widget_hide(scroll);
    gtk_widget_hide(exit_button);
    gtk_main();

    return 0;
}
