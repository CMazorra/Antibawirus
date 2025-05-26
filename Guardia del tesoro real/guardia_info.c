#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

typedef struct 
{
   char pid[16];
   char name[256];
   float prev_cpu;
   long prev_memory;
} ProcessHistory;

typedef struct
{
   ProcessHistory *processes;
   int count;
   int capacity;
}ProcessTracker;

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

void cleanup(ProcessTracker *tracker)
{
   int i = 0;
   while(i< tracker-> count)
   {
      if(!pid_exists(tracker->processes[i].pid))
      {
         tracker->processes[i] = tracker->processes[tracker->count-1];
         tracker->count--;
      }
      else 
       i++;
   }
}

void update_process(ProcessTracker *tracker, const char* pid,const char* name, float cpu, long memory)
{
   for(int i = 0; i< tracker->count; i++)
   {
    if(strcmp(tracker->processes[i].pid, pid) == 0)
     {
       float cpu_us = cpu - tracker->processes[i].prev_cpu;
       long mem_us = memory - tracker->processes[i].prev_memory;

       if(cpu_us > 10.0)
         printf("⚠️ [CPU] %s (PID: %s) +%.2f%%\n", name, pid, cpu_us);
       if(mem_us > 100000/2)
         printf("⚠️ [RAM] %s (PID: %s) +%ld KB\n", name, pid, mem_us); 

       tracker->processes[i].prev_cpu = cpu;
       tracker->processes[i].prev_memory = memory;
       return;
     }
   }

   if(tracker->count >= tracker->capacity)
   {
      int new_capacity = tracker->capacity*2;
      ProcessHistory *new_processes = realloc(tracker->processes, new_capacity * sizeof(ProcessHistory));
      if(!new_processes)
      {
         perror("Error al redimensionar memoria");
         exit(EXIT_FAILURE);
      }
       
      tracker->processes = new_processes;
      tracker -> capacity = new_capacity;
   }

   strncpy(tracker->processes[tracker->count].pid, pid,
            sizeof(tracker->processes[tracker->count].pid)-1);
   strncpy(tracker->processes[tracker->count].name, name,
               sizeof(tracker->processes[tracker->count].name)-1);         
   tracker->processes[tracker->count].prev_cpu = cpu;
   tracker->processes[tracker->count].prev_memory = memory;
   tracker->count++;
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

void process_info(const char* pid, ProcessTracker *tracker)
{
 char path[256], line[1024], name[256];
 FILE* file;
 long utime = 0, stime=0, start_time = 0, rss = 0, vsize = 0;
 int threads = 0;

 //Name and thread amount of the process
 snprintf(path, sizeof(path), "/proc/%s/status",pid);
 file = fopen(path, "r");
 if(file)
 {
    while(fgets(line,sizeof(line),file))
    {
        if(strncmp(line, "Name:", 5) == 0)
         sscanf(line, "Name:\t%s", name);
        else if(strncmp(line, "Threads:", 8) == 0)
         sscanf(line, "Threads:\t%d", &threads); 
    }
    fclose(file);
 }

 //CPU and memory
 snprintf(path, sizeof(path), "/proc/%s/stat",pid);
 file = fopen(path, "r");
 if(file)
 {
  fscanf(file, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu %*d %*d %*d %*d %*d %*d %*u %lu %ld",
        &utime, &stime,&start_time, &rss);
  fclose(file);      
 }

 //virtual memory
 snprintf(path, sizeof(path), "/proc/%s/statm",pid);
 file = fopen(path, "r");
 if(file)
 {
  fscanf(file, "%lu", &vsize);
  fclose(file);
  vsize *= sysconf(_SC_PAGESIZE) / 1024;
 }

 long total_time = utime + stime;
 long seconds = sysconf(_SC_CLK_TCK);
 long total_seconds = total_time / seconds;
 long total_cpu = get_cpu_time();
 float cpu = 0.0;
 if(total_cpu > 0)
 {
    cpu = (total_time * 100.0) / total_cpu;
 }
 
 update_process(tracker,pid,name,cpu,rss);

 printf("PID: %-6s | Nombre: %-20s | CPU: %6.2f%% | Tiempo en CPU: %8ld ms | Memoria: %6ld KB (RSS) | %6lu KB (Virtual) | Hilo: %d\n",
         pid, name, cpu, total_seconds ,rss * 4, vsize, threads);
}

int main()
{
   DIR* dir;
   struct dirent* entry;
   ProcessTracker tracker;
   time_t last_cleanup = time(NULL);

   init_tracker(&tracker);

   printf("🛡️ Guardias del Tesoro Real - Monitoreo activado\n");
   printf("-------------------------------------------\n");


   while(1)
   {
    if(difftime(time(NULL), last_cleanup) > 30)
    {
      cleanup(&tracker);
      last_cleanup = time(NULL);
      printf("\n--- Limpieza realizada. Procesos activos: %d ---\n", tracker.count);
    }
    
    dir= opendir("/proc");
    if(!dir)
    {
      perror("Error al abrir /proc");
      sleep(5);
      continue;
    }

    while((entry =readdir(dir)) != NULL)
    {
     if(isdigit(entry->d_name[0]))
      process_info(entry->d_name, &tracker);
    }
    closedir(dir);

    sleep(2);
   }
   
   free(tracker.processes);
   return 0;
}