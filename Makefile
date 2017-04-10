export GOPATH := ${CURDIR}/go
export GOARCH := arm
export PATH   := $(PATH):$(GOPATH)/bin

.PHONY: build lora_gateway packet_forwarder LoRaMac-node lora-gateway-bridge loraserver lora-app-server certificate db install clean

build: lora_gateway packet_forwarder LoRaMac-node lora-gateway-bridge loraserver lora-app-server certificate

lora_gateway:
	$(MAKE) -C lora_gateway

packet_forwarder: lora_gateway
	$(MAKE) -C packet_forwarder

LoRaMac-node:
	$(MAKE) -C LoRaMac-node

lora-gateway-bridge:
	$(MAKE) build -C go/src/github.com/brocaar/lora-gateway-bridge

loraserver:
	$(MAKE) requirements -C go/src/github.com/brocaar/loraserver SHELL=/bin/bash
	$(MAKE) generate -C go/src/github.com/brocaar/loraserver
	$(MAKE) build -C go/src/github.com/brocaar/loraserver

lora-app-server: loraserver
	cp go/src/github.com/brocaar/loraserver/api/as/* go/src/github.com/brocaar/lora-app-server/vendor/github.com/brocaar/loraserver/api/as/
	$(MAKE) requirements -C go/src/github.com/brocaar/lora-app-server SHELL=/bin/bash
	go get -u github.com/google/protobuf | true
	cp go/src/github.com/google/protobuf/src/google/protobuf go/src/github.com/grpc-ecosystem/grpc-gateway/third_party/googleapis/google/ -R
	$(MAKE) ui-requirements -C go/src/github.com/brocaar/lora-app-server
	$(MAKE) ui -C go/src/github.com/brocaar/lora-app-server
	$(MAKE) api -C go/src/github.com/brocaar/lora-app-server
	$(MAKE) statics -C go/src/github.com/brocaar/lora-app-server
	$(MAKE) build -C go/src/github.com/brocaar/lora-app-server

db:
	sudo -u postgres psql -c "create role loraserver with login password 'dbpassword';"
	sudo -u postgres psql -c "create database loraserver with owner loraserver;"

certificate:
	openssl req -x509 -newkey rsa:2048 -keyout http-key.pem -out http.pem -days 365 -nodes -batch -subj "/CN=localhost"

install:
	sudo cp -r systemd/* /etc/systemd/system/
	sudo mkdir -p /opt/LoRa-forwarder
	sudo chown pi:pi /opt/LoRa-forwarder
	cp http-key.pem http.pem /opt/LoRa-forwarder/
	cp go/src/github.com/brocaar/lora-gateway-bridge/build/lora-gateway-bridge /opt/LoRa-forwarder/
	cp go/src/github.com/brocaar/loraserver/build/loraserver /opt/LoRa-forwarder/
	cp go/src/github.com/brocaar/lora-app-server/build/lora-app-server /opt/LoRa-forwarder/
	cp packet_forwarder/poly_pkt_fwd/*.json packet_forwarder/poly_pkt_fwd/poly_pkt_fwd /opt/LoRa-forwarder/
	cp connect-with-gecko/connect-with-gecko.py /opt/LoRa-forwarder/
	cp reset-concentrator.sh /opt/LoRa-forwarder/

clean:
	$(MAKE) clean -C lora_gateway
	$(MAKE) clean -C packet_forwarder
	$(MAKE) clean -C LoRaMac-node
	$(MAKE) clean -C go/src/github.com/brocaar/lora-gateway-bridge
	$(MAKE) clean -C go/src/github.com/brocaar/loraserver
	$(MAKE) clean -C go/src/github.com/brocaar/lora-app-server
	rm -f http-key.pem http.pem
