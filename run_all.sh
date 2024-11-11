#!/bin/bash

# Step 1: Avvia il server Redis se non è già in esecuzione
if ! pgrep -x "redis-server" > /dev/null
then
    echo "Starting Redis server..."
    redis-server --daemonize yes
else
    echo "Redis server is already running."
fi

# Step 2: Configura e compila il progetto usando CMake
echo "Configuring and building the project..."
mkdir -p build               # Crea una directory di build
cd build                     # Entra nella directory di build
cmake ..                     # Configura il progetto (usa CMakeLists.txt nella directory principale)
make                         # Compila il progetto (usa i Makefiles generati da CMake)

# Step 3: Avvia il Control Center in background
echo "Starting Control Center..."
./control_center &
CONTROL_CENTER_PID=$!         # Salva il PID del processo per poterlo terminare successivamente

# Step 4: Avvia il Drone Simulation
echo "Starting Drone Simulation..."
./drone                      # Avvia il drone

# Step 5: Gestione della terminazione
trap "cleanup" SIGINT        # Configura il comportamento per Ctrl+C

cleanup() {
    echo "Stopping all processes..."
    kill $CONTROL_CENTER_PID # Termina il Control Center
    redis-cli shutdown       # Spegne il server Redis
    echo "All processes stopped."
    exit 0
}