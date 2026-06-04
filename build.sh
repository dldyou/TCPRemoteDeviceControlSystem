#!/bin/bash

set -e

# GET Config
CONFIG_FILE="config.ini"

if [ ! -f "$CONFIG_FILE" ]; then
    echo "❌ Config file '$CONFIG_FILE' not found!"
    exit 1
fi

while IFS='=' read -r key value; do
    value="${value%$'\r'}"
    key="${key%$'\r'}"
    
    if [[ -z "$key" || "$key" =~ ^[#\;] || "$key" =~ ^\[.*\]$ ]]; then
        continue
    fi
    
    declare "$key"="$value"
done < "config.ini"

if [ "$1" == "clean" ]; then
    echo "🧹 Cleaning build directories..."
    rm -rf build_client build_server
    echo "✨ Clean complete!"
    exit 0
fi


echo "Building VEDA_TcpChat..."

echo "==============================================="
echo "🚀 [Client] Build for Ubuntu..."
echo "==============================================="
mkdir -p build_client && cd build_client
cmake -DCMAKE_C_COMPILER=gcc -DBUILD_SERVER=OFF .. 
cmake --build . --target client
cd ..

# if [ "$1" == "linux" ]; then
#     echo "==============================================="
#     echo "🚀 [Server] Build for Ubuntu..."
#     echo "==============================================="
#     mkdir -p build_server && cd build_server
#     cmake -DCMAKE_C_COMPILER=gcc -DBUILD_CLIENT=OFF ..
#     cmake --build . --target server
#     cd ..

#     echo "==============================================="
#     echo "✨ Build & Transfer Complete!"
#     echo "==============================================="
#     exit 0
# fi

echo "==============================================="
echo "🚀 [Server] Build for RaspberryPi4..."
echo "==============================================="
mkdir -p build_server && cd build_server
cmake -DCMAKE_TOOLCHAIN_FILE=../raspberrypi4_toolchain.cmake -DBUILD_CLIENT=OFF ..
cmake --build . --target server
cd ..

echo "==============================================="
echo "🚚 Sending Server to Raspberry Pi4..."
echo "==============================================="

ssh ${RPI_USER}@${RPI_IP} "mkdir -p ${RPI_DEST}"

mkdir -p temp
cp build_server/server/server temp/
cp build_server/server/device/led/libled.so temp/
cp build_server/server/device/buzzer/libbuzzer.so temp/
cp build_server/server/device/light/liblight.so temp/
cp build_server/server/device/segment/libsegment.so temp/
cp resource/* temp/
cp config.ini temp/
scp -r temp/* ${RPI_USER}@${RPI_IP}:${RPI_DEST}/
rm -rf temp

echo "==============================================="
echo "✅ Server sent to Raspberry Pi4(${RPI_USER}@${RPI_IP}:${RPI_DEST}/) successfully!"
echo "✨ Build & Transfer Complete!"
echo "==============================================="