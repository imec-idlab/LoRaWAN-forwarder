LoRa forwarder
==============

This project provides a proof-of-concept of a LoRa forwarding device. The forwarder will act as a gateway to the end-device and act as an end-device to the upstream gateway. The software only forwards traffic for a single end-device. Only class A and class C end-devices are supported.

For hardware the forwarder uses a Raspberry Pi connected to an [iC880A concentrator](https://wireless-solutions.de/products/radiomodules/ic880a.html) board and a [EFM32 Giant Gecko](http://www.silabs.com/products/mcu/32-bit/efm32-giant-gecko) using a [HopeRF RFM95W](http://www.hoperf.com/rf_transceiver/lora/RFM95W.html) radio chip. The concentrator board is connected as explained [here](https://github.com/ttn-zh/ic880a-gateway/wiki#putting-it-all-together). The gecko is connected over a [USB to Serial Breakout](https://www.sparkfun.com/products/12731) on the PE0 and PE1 pins.

The software has to run on Raspbian Stretch or a comparable distro. The majority of the software consists of several existing projects to which minor changes are applied. The following external projects are used:

- [TheThingsNetwork/lora_gateway](https://github.com/TheThingsNetwork/lora_gateway/)
- [TheThingsNetwork/packet_forwarder](https://github.com/TheThingsNetwork/packet_forwarder/)
- [brocaar/lora-gateway-bridge](https://github.com/brocaar/lora-gateway-bridge)
- [brocaar/lora-server](https://github.com/brocaar/loraserver)
- [brocaar/lora-app-server](https://github.com/brocaar/lora-app-server)
- [imec-idlab/LoRaMac-node](https://github.com/imec-idlab/LoRaMac-node)


Dependencies
------------

Make sure you have the required packages installed:
```
sudo apt-get install gcc-arm-none-eabi \
                     golang \
                     protobuf-compiler \
                     go-bindata \
                     mosquitto \
                     redis-server \
                     postgresql \
                     npm

pip install paho-mqtt
```


Setup
-----

Make sure the concentrator board is connected properly (as explained [here](https://github.com/ttn-zh/ic880a-gateway/wiki#putting-it-all-together)) and that the antenna is attached.

Make sure SPI is enabled on the Raspberry Pi. You can enable it by running `sudo raspi-config`, selecting `Interface Options` and enabling `SPI` there.

Create a database for the LoRa Server:
```
make db
```

Build and install the software to `/opt/LoRa-forwarder`:
```
make build
make install
```

Start the lora-app-server to configure the settings for the end-device. To do this, run `sudo systemctl start lora-app-server`. You can then connect to `https://0.0.0.0:8080`. This will print a warning about the certificate because a self-signed certificate was used, discard this warning (in chrome press `ADVANCED` and then `Proceed to 0.0.0.0 (unsafe)`). Click the `CREATE APPLICATION` button on the page and enter any application name and description  in the `Application details` tab. Under the `Network settings` tab, you must set the `Receive window delay` high enough so that the forwarder has time enough to send the packet to the upstream gateway and get a response back. Currently you must set this to 10. After hitting `Submit` you can press the `CREATE NODE` button to setup your end-device to your needs.

You will also have to enter the Device EUI of the end-device in the `DEVICE_EUI` constant on top of `/opt/LoRa-forwarder/connect-with-gecko.py`. The `APPLICATION_ID` constant can remain 1 if you didn't create multiple applications. Note that the ApplicationID is not the same as the AppEUI.

The last thing to do is flash `LoRaMac-node/LoRaMac-forwarder.hex` to the gecko that will be connected to the Raspberry Pi while using the forwarder.


Running
-------

Connect the gecko to the Raspberry Pi or press its reset button when already connected. It will immediately attempt to join the upstream gateway. Once it has done this, it will start listening for serial communication with the Raspberry Pi.

Now run the python script that will communicate with the gecko later when there is a packet to forward to the upstream gateway. Note that this command is blocking.
```
python /opt/LoRa-forwarder/connect-with-gecko.py
```

Start the LoRa Server which will handle the connection with the end-device.
```
sudo systemctl start lora-app-server loraserver lora-gateway-bridge
```

Finally start the packet forwarder that will configure the concentrator board and pass packets to the LoRa Server. This command is blocking.
```
cd /opt/LoRa-forwarder/
sudo ./reset-concentrator.sh
./poly_pkt_fwd
```
