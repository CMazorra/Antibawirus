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

#define UmbralCPU 10.0
#define UmbralMem 100000

pthread_mutex_t tracker_mutex = PTHREAD_MUTEX_INITIALIZER;

void cleanup(ProcessTracker *tracker)
{
   pthread_mutex_lock(&tracker_mutex);
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
   pthread_mutex_unlock(&tracker_mutex);

}

void update_process(ProcessTracker *tracker, const char*pid, const char*name, long current_proc_time,
                    long current_total_cpu, long memory, float* cpu_out, long* delta_proc_out, float ram_percent)
{
   pthread_mutex_lock(&tracker_mutex);

   for(int i = 0; i< tracker -> count;i++)
   {
      if(strcmp(tracker->processes[i].pid, pid) == 0)
      {
         float cpu_us = 0.0;
         long delta_total = current_total_cpu - tracker->processes[i].prev_total_cpu;
         long delta_proc = current_proc_time - tracker->processes[i].prev_proc_time;
         long clock_ticks = sysconf(_SC_CLK_TCK);
         int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);

         if(delta_total > 0)
          cpu_us = ((float)delta_proc / delta_total) * num_cpus * 100.0;

         long mem_us = memory - tracker -> processes[i].prev_memory; 

         if(cpu_us - tracker->processes[i].prev_cpu > UmbralCPU && !lista_blanca(name))
         {
            printf("⚠️ [CPU] %s (PID: %s) +%.2f%%\n", name, pid, cpu_us-tracker->processes[i].prev_cpu);
         }
         if(mem_us - tracker->processes[i].prev_memory > UmbralMem / 2 && !lista_blanca(name))
         {
            printf("⚠️ [RAM] %s (PID: %s) +%ld KB\n", name, pid, mem_us- tracker->processes[i].prev_memory); 
         }
         if(ram_percent - tracker->processes[i].prev_ram_percent > 10.0 && !lista_blanca)
         {
            printf("⚠️ [RAM%%] %s (PID: %s) +%.2f%%\n", name, pid, ram_percent - tracker->processes[i].prev_ram_percent);
         }

         tracker->processes[i].prev_cpu = cpu_us;
         tracker->processes[i].prev_memory = memory;
         tracker->processes[i].prev_proc_time = current_proc_time;
         tracker->processes[i].prev_total_cpu = current_total_cpu;
         tracker->processes[i].prev_ram_percent = ram_percent;

         *cpu_out = cpu_us;
         *delta_proc_out = delta_proc;
          pthread_mutex_unlock(&tracker_mutex);
          return;
      }
   }

   if(tracker->count >= tracker->capacity)
   {
      int new_capacity = tracker->capacity*2;
      ProcessHistory* new_processes = realloc(tracker->processes, new_capacity*sizeof(ProcessHistory));
      if(!new_processes)
      {
         perror("Error al redimensionar la memoria");
         exit(EXIT_FAILURE);
      }
      tracker->processes = new_processes;
      tracker->capacity = new_capacity;
   }

   strncpy(tracker->processes[tracker->count].pid, pid,
      sizeof(tracker->processes[tracker->count].pid)-1);
   strncpy(tracker->processes[tracker->count].name, name, 
      sizeof(tracker->processes[tracker->count].name)-1);
   tracker->processes[tracker->count].prev_cpu = 0.0;
   tracker->processes[tracker->count].prev_memory = memory;
   tracker->processes[tracker->count].prev_proc_time = current_proc_time;
   tracker->processes[tracker->count].prev_total_cpu = current_total_cpu;
   tracker->count++;
   
   *cpu_out = 0.0;
   *delta_proc_out = 0;
   pthread_mutex_unlock(&tracker_mutex);
   return;
}

void process_info(const char* pid, ProcessTracker *tracker)
{
 char path[256], line[1024], name[256];
 FILE* file;
 long utime = 0, stime=0, start_time = 0, rss = 0, vsize = 0;
 int threads = 0;

 //Name, rss and thread amount of the processes
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
        else if(strncmp(line, "VmRSS:", 6) == 0)
         sscanf(line, "VmRSS: %ld KB", &rss);  
    }
    fclose(file);
 }

 //CPU
snprintf(path, sizeof(path), "/proc/%s/stat", pid);
    sprintf(path, "/proc/%s/stat", pid);
    file = fopen(path, "r");
    if (!file) {
        perror("Error al abrir /proc/[pid]/stat");
    }

    fgets(line, sizeof(line), file);
    fclose(file);

    // Saltar hasta el campo 14 (utime) y 15 (stime)
    char *ptr = line;
    int i;
    for (i = 0; i < 13; i++) {
        ptr = strchr(ptr, ' ') + 1;
    }
    sscanf(ptr, "%ld %ld", &utime, &stime);


//  //Memory
//  snprintf(path, sizeof(path), "/proc/%s/statm", pid);
//     file = fopen(path, "r");
//     if (file) {
//         fscanf(file, "%*s %ld", &rss);
//         fclose(file);
//         rss *= sysconf(_SC_PAGESIZE) / 1024; 
//     }
 //virtual memory
 snprintf(path, sizeof(path), "/proc/%s/statm",pid);
 file = fopen(path, "r");
 if(file)
 {
  fscanf(file, "%lu", &vsize);
  fclose(file);
  vsize *= sysconf(_SC_PAGESIZE) / 1024;
 }

 long current_proc_time = utime + stime;  
 long current_total_cpu = get_cpu_time();
 float cpu = 0.0;
 long delta_proc = 0;
 long total_ram = get_total_ram();
 float ram_percent = total_ram > 0 ? ((float)rss / total_ram) * 100.0 : 0.0;
 
 update_process(tracker, pid, name, current_proc_time, current_total_cpu, rss, &cpu, &delta_proc,ram_percent);

  if(cpu > UmbralCPU && !lista_blanca(name))
  {
  printf("\n🚨 CPU: %.2f%% (PID: %s)\n", cpu, pid);
  }

  if(rss > UmbralMem && !lista_blanca(name))
 {
  printf("\n🚨 Memoria: %6ld KB (PID: %s)\n", rss, pid);
 }
 
 long clk_ticks = sysconf(_SC_CLK_TCK);
 float delta_proc_sec = (float)delta_proc / clk_ticks;

 printf("PID: %-6s | Nombre: %-20s | CPU: %6.2f%% | Tiempo en CPU: %8.2f s | RAM: %6ld KB (%.2f%%) | %6lu KB (Virtual) | Hilo: %d\n",
         pid, name, cpu, delta_proc_sec, rss, ram_percent, vsize, threads);
 
//  printf("PID: %-6s | Nombre: %-20s | CPU: %6.2f%% | Tiempo en CPU: %8ld ms | Memoria: %6ld KB (RSS) | %6lu KB (Virtual) | Hilo: %d\n",
//          pid, name, cpu, delta_proc ,rss, vsize, threads);
}

void* process_info_thread(void* arg)
{
   ThreadArgs* args = (ThreadArgs*)arg;
   process_info(args->pid,args->tracker);
   free(args);
   return NULL;
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
      pthread_t thread;
      if(isdigit(entry->d_name[0]))
      {
         ThreadArgs* args = malloc(sizeof(ThreadArgs));
         strncpy(args->pid, entry->d_name, sizeof(args->pid));
         args->tracker = &tracker;

         if(pthread_create(&thread, NULL, process_info_thread, args) != 0)
         {
            perror("Error al crear hilo");
            free(args);
         }
         else
         {
            pthread_detach(thread);
         }
      }
    }
    closedir(dir);

    sleep(10);
   }
   
   free(tracker.processes);
   pthread_mutex_destroy(&tracker_mutex);
   return 0;
}