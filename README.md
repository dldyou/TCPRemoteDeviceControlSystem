# TCP Remote Device Control System

## Tech Stack
- Language: C
- Build: CMake, Shell Script
- Platform: Raspberry Pi OS (64-bit), Ubuntu 24.04
- Network: TCP Socket
- Hardware: LED, Buzzer, Light Sensor(CDS), 7-segment
- Toolchain: aarch64-linux-gnu-gcc

## Key Features
- TCP socket-based client-server communication
- CLI command parsing and device control
- LED brightness control, buzzer tone playback, light sensor reading, and 7-segment number display
- Modularized Device Manager, Plugin Loader, and Logging components
- Cross-compilation build script for Raspberry Pi 4

## Environment
- Server: Raspberry Pi OS (64-bit)
- Client: Ubuntu 24.04 LTS
## Quick Start
### Prerequisites
Your Raspberry Pi should have SSH enabled and be connected to the same network as your client machine. You also need to have the necessary permissions to access the Raspberry Pi remotely.

PINs used in this project:
- LED: GPIO 17
- Buzzer: GPIO 27
- Light Sensor: GPIO 22
- 7-segment Display: GPIO 5, 6, 13, 19, 26, 12, 16, 20 (A, B, C, D, E, F, G, DP)

### Build and Run
First, you need to set up the environment at the root directory of this project. You can create a `config.ini` file with the following content:
```ini
[Server]
addr=your_server_ip_address
port=your_server_port

[RaspberryPi]
RPI_USER=your_raspberry_pi_username
RPI_IP=your_raspberry_pi_ip_address
RPI_DEST=your_raspberry_pi_destination_path
```
#### Build
And then you can build the project using the following command:
```bash
# working directory (/path/to/your_TCPRemoteDeviceControlSystem)
# Make the build script executable
chmod +x ./build.sh
# Run the build script
./build.sh
```
#### Run
At **server** side, you can run the server using the following command:
```bash 
# working directory (/path/to/your_raspberry_pi_TCPRemoteDeviceControlSystem)
./server
```

At **client** side, you can run the client using the following command:
```bash
# working directory (/path/to/your_TCPRemoteDeviceControlSystem)
./build_client/client/client
```

Now you can communicate with the server and control the Raspberry Pi remotely.

## CLI Commands

In the client terminal, you can use the following commands to control the Raspberry Pi:

| Command | Description |
| --- | --- |
| `help` | Show the help message |
| `status` | Show the status of the devices |
| `plugin list` | List all available plugins |
| `led on\|bright <0-255>\|off` | Control the LED light |
| `buzzer music <.mp3 path>\|on\|off\|beep <1-10000>` | Control the buzzer |
| `light read` | Read the light sensor value |
| `seg display <0-9>` | Display a number on the 7-segment display |
| `quit` | Quit Server |

## System Structure
### System Architecture
```bash
TCP Remote Device Control System
├── Client
│   ├── Command Line Interface (CLI)
│   └── Network Communication
└── Server
    ├── Network Communication
    ├── Command Parsing
    ├── Device Management
    │   ├── LED Control
    │   ├── Buzzer Control
    │   ├── Light Sensor Reading
    │   └── 7-segment Display Control
    ├── Plugin System
    └── Logging
```
### Project Structure
```bash
├── CMakeLists.txt
├── README.md
├── build.sh
├── client
│   ├── CMakeLists.txt
│   └── main.c
├── common
│   ├── app_config.c
│   ├── app_config.h
│   ├── command.c
│   └── command.h
├── config.ini
├── raspberrypi4_toolchain.cmake
├── resource
│   └── .wav files
└── server
    ├── CMakeLists.txt
    ├── device
    │   ├── CMakeLists.txt
    │   ├── buzzer
    │   │   ├── CMakeLists.txt
    │   │   ├── buzzer.c
    │   │   ├── buzzer.h
    │   │   ├── pitch_detector.c
    │   │   ├── pitch_detector.h
    │   │   ├── wav_reader.c
    │   │   └── wav_reader.h
    │   ├── device_manager.c
    │   ├── device_manager.h
    │   ├── device_result.c
    │   ├── device_result.h
    │   ├── device_worker.c
    │   ├── device_worker.h
    │   ├── led
    │   │   ├── CMakeLists.txt
    │   │   ├── led.c
    │   │   └── led.h
    │   ├── light
    │   │   ├── CMakeLists.txt
    │   │   ├── light.c
    │   │   └── light.h
    │   ├── plugin_loader.c
    │   ├── plugin_loader.h
    │   ├── plugin_registry.c
    │   ├── plugin_registry.h
    │   └── segment
    │       ├── CMakeLists.txt
    │       ├── segment.c
    │       └── segment.h
    ├── main.c
    ├── parsing.c
    ├── parsing.h
    ├── server_log.c
    ├── server_log.h
    ├── socket.c
    └── socket.h
```

