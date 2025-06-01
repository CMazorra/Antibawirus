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

    long user, nice, system, idle;
    fscanf(file, "cpu %ld %ld %ld %ld", &user, &nice, &system, &idle);
    fclose(file);
    return user + nice + system + idle;
}


