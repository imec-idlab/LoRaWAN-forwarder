set -e
set -o xtrace

# Install dependencies
sudo apt-get install gcc-arm-none-eabi golang protobuf-compiler go-bindata mosquitto redis-server postgresql
pip install paho-mqtt

# Export the needed environment variables
mkdir -p go
export GOPATH=$(pwd)/go
export GOARCH=arm
export PATH=$PATH:$GOPATH/bin

# Build the LoRa Gateway
git clone https://github.com/TheThingsNetwork/lora_gateway
cd lora_gateway
git checkout f4329a63a08fb643d1e05815529f8be1461b7dd9
sed -i 's/PLATFORM= kerlink/PLATFORM=imst_rpi/g' libloragw/library.cfg
make -j4
cd ..

# Build the Packet forwarder
git clone https://github.com/TheThingsNetwork/packet_forwarder
cd packet_forwarder
git checkout 884bb13504137bb2af32ce487698d30a10952c86
make -j4
cp poly_pkt_fwd/poly_pkt_fwd ../poly_pkt_fwd/
cd ..

# Build the LoRa node
git clone https://github.com/imec-idlab/LoRaMac-node
cd LoRaMac-node
git checkout bccf111f5df25ac5139008367e204f38b6de7ccd
git apply ../patches/lora-node.patch
mkdir -p src/apps/forwarder/EFM32GG_STK3700
cp ../gecko-forwarder/Comissioning.h ../gecko-forwarder/main.c src/apps/forwarder/EFM32GG_STK3700/
mv LoRaMac-node.ld LoRaMac-forwarder.ld
make -j4
cd ..

# Download the LoRa Server from brocaar
go get github.com/brocaar/lora-gateway-bridge | true
cd go/src/github.com/brocaar/lora-gateway-bridge
git checkout 10664d9014535305026e17c86d584156e6ea71b4
cd ../../../../..
go get github.com/brocaar/loraserver | true
cd go/src/github.com/brocaar/loraserver
git checkout 81e806e82ad2be746f407afd5461731c9a1d2e02
cd ../../../../..
go get github.com/brocaar/lora-app-server | true
cd go/src/github.com/brocaar/lora-app-server
git checkout 52768a83c8224df5743a719812039fb479ad6528
cd ../../../../..

# Patch the LoRa Server
cd go/src/github.com/brocaar/loraserver
git apply ../../../../../patches/loraserver.patch
cd ../../../../..
cd go/src/github.com/brocaar/lora-app-server
git apply ../../../../../patches/lora-app-server.patch
cd ../../../../..

# Build the LoRa Server
cd go/src/github.com/brocaar/lora-gateway-bridge
make build
cd ../../../../..
cd go/src/github.com/brocaar/loraserver
make requirements SHELL=/bin/bash
make generate
make build
cd ../../../../..
cd go/src/github.com/brocaar/lora-app-server
cp ../loraserver/api/as/* vendor/github.com/brocaar/loraserver/api/as/
make requirements
go get -u github.com/google/protobuf | true
cp ../../google/protobuf/src/google/protobuf ../../grpc-ecosystem/grpc-gateway/third_party/googleapis/google/ -R
make generate
make build
cd ../../../../..

# Setup the postgres database
sudo -u postgres psql -c "create role loraserver with login password 'dbpassword';"
sudo -u postgres psql -c "create database loraserver with owner loraserver;"

# Install the forwarder
sudo cp -r systemd/* /etc/systemd/system/
sudo mkdir -p /opt/LoRa-forwarder
sudo chown pi:pi /opt/LoRa-forwarder
openssl req -x509 -newkey rsa:2048 -keyout /opt/LoRa-forwarder/http-key.pem -out /opt/LoRa-forwarder/http.pem -days 365 -nodes -batch -subj "/CN=localhost"
cp go/src/github.com/brocaar/lora-gateway-bridge/build/lora-gateway-bridge /opt/LoRa-forwarder/
cp go/src/github.com/brocaar/loraserver/build/loraserver /opt/LoRa-forwarder/
cp go/src/github.com/brocaar/lora-app-server/build/lora-app-server /opt/LoRa-forwarder/
cp poly_pkt_fwd/* /opt/LoRa-forwarder/
cp connect-with-gecko/connect-with-gecko.py /opt/LoRa-forwarder/
cp reset-concentrator.sh /opt/LoRa-forwarder/
