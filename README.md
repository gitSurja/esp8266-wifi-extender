# ESP8266 WiFi Extender

An ESP8266 2.4 GHz WiFi access point with NAPT forwarding to an upstream WiFi network.

## Features

- Configurable upstream SSID and password from `/setup`
- Extender SSID is generated as `<upstream-SSID>_EXT`
- Credentials are stored in EEPROM
- Dashboard available through the upstream and extender interfaces
- Live status, signal, memory, clients, and connection-history graphs
- Dashboard LED test controls
- Built-in LED blinks while the upstream is disconnected
- GPIO16 signals client connect/disconnect events

## Hardware and limits

This project targets the ESP8266 and supports 2.4 GHz only. Because the ESP8266 uses one radio for receiving and retransmitting, throughput is substantially lower than a dual-radio extender.

Use a stable regulated 5 V USB supply. Do not connect the board directly to mains or an unregulated high-voltage charger output.

## Build

Use Arduino CLI with the ESP8266 core. Select `Generic ESP8266 Module` and the `4M1M` flash layout. The recommended lwIP option is `hb2f`.

Compile `WifiExtenderWeb/WifiExtenderWeb.ino` and upload it to the board.

## Configuration

On first boot the example upstream is `MIS`; configure its password through `/setup` before deployment. After saving, the extender SSID changes to `<new-SSID>_EXT`.

The dashboard is available at the AP address shown in Serial Monitor, normally `http://192.168.5.1/`.
