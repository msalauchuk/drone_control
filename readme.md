# Drone Control System

A swarm drone control system for large area verification. The system uses a central control center to coordinate multiple drones, optimizing area coverage while managing battery life and drone coordination.

## Prerequisites

Use the package manager of your OS to install Redis:

```bash
# Mac
brew install redis

# Linux
sudo apt-get install redis-server

# Windows
Download from https://redis.io/download
```

You'll also need:
- CMake (version 3.X or higher)
- C++ compiler with C++20 support
- Redis server

## Installation

After installing Redis, build the project using CMake:

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

First, start Redis server in project directory:

```bash
redis-server --dir ./ --dbfilename dump.rdb
```

Then run the control center:

```bash
./control_center
```

In another terminal, run the drone simulation:

```bash
./drone
```
Clean compiled files but keep CMake configuration
```bash
make clean
make
```

Complete clean rebuild
```bash
rm -rf build/*
cmake ..
make
```




To visualize the coverage results:

```bash

pip install pandas matplotlib seaborn

python plotter.py test/system.csv
```

## Troubleshooting

If Redis fails to start:

```bash
# Stop any existing Redis server
redis-cli shutdown

# Check for processes using Redis port
lsof -i :6379

# Kill process if needed
kill <PID>

# Start Redis again
redis-server --dir ./ --dbfilename dump.rdb
```

## Project Structure

```
├── src/redis             # Redis connection handling
├── src/control_center    # Control center implementation
├── src/drone             # Drone logic implementation  
├── src/common/           # System configuration
└── test/                 # Visualization tools
```

## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.

Please make sure to update tests as appropriate.

## License

MIT
