#!/bin/bash

echo "Iniciando carga de CPU con procesos 'yes'..."

# Número de procesos yes (puede ser igual al número de núcleos de tu CPU)
N=4

# Lanzar N procesos yes en segundo plano y guardar sus PIDs
for i in $(seq 1 $N); do
    yes > /dev/null &
    PIDS[$i]=$!
done

echo "Procesos lanzados: ${PIDS[*]}"
echo "Esperando 30 segundos..."
sleep 30

echo "Terminando procesos..."
for pid in "${PIDS[@]}"; do
    kill $pid 2>/dev/null
done

echo "Listo. Carga finalizada."
