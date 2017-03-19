set -e
set -o xtrace

export GOPATH=$(pwd)/go
export GOARCH=arm
export PATH=$PATH:$GOPATH/bin

sudo apt-get install gcc-arm-none-eabi golang protobuf-compiler go-bindata mosquitto redis-server postgresql
pip install paho-mqtt

cd lora_gateway
make -j4
cd ..

cd packet_forwarder
make -j4
cd ..

cd LoRaMac-node
make -j4
cd ..

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
cp packet_forwarder/poly_pkt_fwd/*.json packet_forwarder/poly_pkt_fwd/poly_pkt_fwd /opt/LoRa-forwarder/
cp connect-with-gecko/connect-with-gecko.py /opt/LoRa-forwarder/
cp reset-concentrator.sh /opt/LoRa-forwarder/
