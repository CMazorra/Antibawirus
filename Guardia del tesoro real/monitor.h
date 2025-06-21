#ifndef Processes_H
#define Processes_H

typedef struct 
{
   char pid[16];
   char name[256];
   float prev_cpu;
   long prev_memory;
   long prev_ram_percent;
   int prev_alert;
   long prev_proc_time;
   long prev_total_cpu;
} ProcessHistory;

typedef struct
{
   ProcessHistory *processes;
   int count;
   int capacity;
}ProcessTracker;

typedef struct{
   char pid[16];
   ProcessTracker* tracker;
}ThreadArgs;


int lista_blanca(const char* name);
int pid_exists(const char* pid);
void init_tracker(ProcessTracker *tracker);
long get_cpu_time();
long get_total_ram();

#endif