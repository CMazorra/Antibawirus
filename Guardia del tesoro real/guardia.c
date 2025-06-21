// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <dirent.h>
// #include <ctype.h>

// void obtener_informacion_proceso(const char *pid) {
//     char ruta[256], nombre[256], buffer[1024];
//     FILE *archivo;

//     // Obtener el nombre del proceso desde /proc/[PID]/status
//     snprintf(ruta, sizeof(ruta), "/proc/%s/status", pid);
//     archivo = fopen(ruta, "r");
//     if (archivo == NULL) {
//         perror("Error al abrir archivo de status");
//         return;
//     }

//     while (fgets(buffer, sizeof(buffer), archivo)) {
//         if (strncmp(buffer, "Name:", 5) == 0) {
//             sscanf(buffer, "Name:\t%255s", nombre);
//             break;
//         }
//     }
//     fclose(archivo);

//     // Obtener el uso de CPU y memoria desde /proc/[PID]/stat
//     snprintf(ruta, sizeof(ruta), "/proc/%s/stat", pid);
//     archivo = fopen(ruta, "r");
//     if (archivo == NULL) {
//         perror("Error al abrir archivo de stat");
//         return;
//     }

//     long utime, stime, rss;
//     int i = 0;
//     while (fscanf(archivo, "%s", buffer) == 1) {
//         i++;
//         if (i == 14) utime = atol(buffer);  // Tiempo de CPU en modo usuario
//         if (i == 15) stime = atol(buffer);  // Tiempo de CPU en modo sistema
//         if (i == 24) rss = atol(buffer);    // Uso de memoria
//     }
//     fclose(archivo);

//     printf("PID: %s, Nombre: %s, CPU: %ld ticks, Memoria: %ld KB\n", pid, nombre, utime + stime, rss * 4);
// }

// int main() {
//     DIR *dir = opendir("/proc");
//     if (dir == NULL) {
//         perror("No se puede abrir /proc");
//         return 1;
//     }

//     struct dirent *entry;
//     while ((entry = readdir(dir)) != NULL) {
//         // Verificar si el nombre del directorio es un número (PID)
//         if (isdigit(entry->d_name[0])) {
//             obtener_informacion_proceso(entry->d_name);
//         }
//     }

//     closedir(dir);
//     return 0;
// }

// void update_process(ProcessTracker *tracker, const char* pid,const char* name, float cpu, long memory)
// {
//    for(int i = 0; i< tracker->count; i++)
//    {
//     if(strcmp(tracker->processes[i].pid, pid) == 0)
//      {
//        float cpu_us = cpu - tracker->processes[i].prev_cpu;
//        long mem_us = memory - tracker->processes[i].prev_memory;

//        if(cpu_us > 10.0)
//        {
//          printf("⚠️ [CPU] %s (PID: %s) +%.2f%%\n", name, pid, cpu_us);
//        }
//        if(mem_us > 100000/2)
//        {
//          printf("⚠️ [RAM] %s (PID: %s) +%ld KB\n", name, pid, mem_us); 
//        }

//        tracker->processes[i].prev_cpu = cpu;
//        tracker->processes[i].prev_memory = memory;
//        return;
//      }
//    }

//    if(tracker->count >= tracker->capacity)
//    {
//       int new_capacity = tracker->capacity*2;
//       ProcessHistory *new_processes = realloc(tracker->processes, new_capacity * sizeof(ProcessHistory));
//       if(!new_processes)
//       {
//          perror("Error al redimensionar memoria");
//          exit(EXIT_FAILURE);
//       }
       
//       tracker->processes = new_processes;
//       tracker -> capacity = new_capacity;
//    }

//    strncpy(tracker->processes[tracker->count].pid, pid,
//             sizeof(tracker->processes[tracker->count].pid)-1);
//    strncpy(tracker->processes[tracker->count].name, name,
//                sizeof(tracker->processes[tracker->count].name)-1);         
//    tracker->processes[tracker->count].prev_cpu = cpu;
//    tracker->processes[tracker->count].prev_memory = memory;
//    tracker->count++;
//  }

//  snprintf(path, sizeof(path), "/proc/%s/stat",pid);
//  file = fopen(path, "r");
//  if(file)
//  {
//   fscanf(file, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu %*d %*d %*d %*d %*d %*d %*u %lu %ld",
//         &utime, &stime,&start_time, &rss);
//   fclose(file);      
//  }

//long get_cpu_time() 
//{
//    FILE* file = fopen("/proc/stat", "r");
//    if (!file)
//    {
//      perror("Error al abrir /proc/stat");
//      return 0;
//    } 

//    long user, nice, system, idle;
//    fscanf(file, "cpu %ld %ld %ld %ld", &user, &nice, &system, &idle);
   // fclose(file);
 //   return user + nice + system + idle;
//}

//snprintf(path, sizeof(path), "/proc/%s/stat", pid);
//file = fopen(path, "r");
//if (file) {
////int skip;
////char comm[256], state;
////unsigned long dummy;
////fscanf(file, "%d %s %c", &skip, comm, &state);
////// Skip next 11 fields
////for (int i = 0; i < 11; i++)
//////fscanf(file, "%lu", &dummy);
////fscanf(file, "%lu %lu", &utime, &stime);
////fclose(file);
  //  }
        