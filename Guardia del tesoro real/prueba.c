#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

long get_total_cpu_time() {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) {
        perror("Error al abrir /proc/stat");
        return -1;
    }

    char line[1024];
    fgets(line, sizeof(line), f);
    fclose(f);

    // Esperamos una línea como: cpu  123 456 789 0 0 ...
    char cpu_label[5];
    long user, nice, system, idle, iowait, irq, softirq, steal;

    int parsed = sscanf(line, "%s %ld %ld %ld %ld %ld %ld %ld %ld",
                        cpu_label, &user, &nice, &system, &idle,
                        &iowait, &irq, &softirq, &steal);

    if (parsed < 5) {
        fprintf(stderr, "Error al parsear /proc/stat\n");
        return -1;
    }

    return user + nice + system + idle + iowait + irq + softirq + steal;
}

long get_process_cpu_time(pid_t pid) {
    char path[64];
    sprintf(path, "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) {
        perror("Error al abrir /proc/[pid]/stat");
        return -1;
    }

    char buf[2048];
    fgets(buf, sizeof(buf), f);
    fclose(f);

    // Saltar hasta el campo 14 (utime) y 15 (stime)
    char *ptr = buf;
    int i;
    for (i = 0; i < 13; i++) {
        ptr = strchr(ptr, ' ') + 1;
    }

    long utime, stime;
    sscanf(ptr, "%ld %ld", &utime, &stime);
    return utime + stime;
}

int main() {
    pid_t pid = 62854; // o cualquier otro PID válido

    long total1 = get_total_cpu_time();
    long proc1 = get_process_cpu_time(pid);
    sleep(1);
    long total2 = get_total_cpu_time();
    long proc2 = get_process_cpu_time(pid);

    if (total1 < 0 || total2 < 0 || proc1 < 0 || proc2 < 0) {
        fprintf(stderr, "Error al obtener datos de CPU\n");
        return 1;
    }

    long delta_total = total2 - total1;
    long delta_proc = proc2 - proc1;

    if (delta_total == 0) {
        printf("CPU Usage: 0.00%% (tiempo total no avanzó)\n");
    } else {
        float usage = 100.0f * delta_proc / delta_total;
        printf("CPU Usage: %.2f%%\n", usage);
    }

    return 0;
}
