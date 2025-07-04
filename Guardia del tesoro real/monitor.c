#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include "monitor.h"

int lista_blanca(const char* name)
{
 const char* lista[] = {"systemd", "kthreadd", "rcu_sched", "gnome-shell","sshd", NULL};
 for (int i = 0; lista[i] != NULL; i++)
 {
   if(strcmp(name, lista[i]) == 0)
    return 1;
 }
 return 0;
}

int pid_exists(const char* pid)
{
   char path[256];
   snprintf(path, sizeof(path), "/proc/%s", pid);
   return (access(path, F_OK) == 0);
}

void init_tracker(ProcessTracker *tracker)
{
   tracker -> processes = malloc(32*sizeof(ProcessHistory));
   if(!tracker->processes)
   {
      perror("error al asignar memoria");
      exit(EXIT_FAILURE);
   }
   tracker->count = 0;
   tracker ->capacity = 32;
}

long get_cpu_time() 
{
    FILE* file = fopen("/proc/stat", "r");
    if (!file)
    {
        perror("Error al abrir /proc/stat");
        return 0;
    }

    char line[1024];
    if (!fgets(line, sizeof(line), file)) {
        perror("Error al leer /proc/stat");
        fclose(file);
        return 0;
    }

    fclose(file);

    long user, nice, system, idle, iowait, irq, softirq, steal;
    int matched = sscanf(line, "cpu %ld %ld %ld %ld %ld %ld %ld %ld",
                         &user, &nice, &system, &idle,
                         &iowait, &irq, &softirq, &steal);

    if (matched < 4) {
        fprintf(stderr, "Error: formato inesperado en /proc/stat\n");
        return 0;
    }

    // Solo suma los campos disponibles según `matched`
    long total = user + nice + system + idle;
    if (matched >= 5) total += iowait;
    if (matched >= 6) total += irq;
    if (matched >= 7) total += softirq;
    if (matched >= 8) total += steal;

    return total;
}

long get_total_ram()
{
   FILE* file = fopen("/proc/meminfo", "r");
   if(!file) return 0;

   char line[256];
   long mem_total = 0;
   while(fgets(line, sizeof(line), file))
   {
      if(strncmp(line, "MemTotal:", 9) == 0)
      {
         sscanf(line, "MemTotal: %ld KB", &mem_total);
         break;
      }
   }
   fclose(file);
   return mem_total;
}
