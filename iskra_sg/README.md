# ESP32 Ethernet Board

ESP32 je zmogljiv komunikacijski kontroler, ki podpira WiFi, Ethernet in Bluetooth komunikacije.
SW projekt vsebuje:
- Ethernet support
- Wifi support
- Nastavitev wifi parametrov preko menuconfig ali preko BT
- Nastavitve za ethernet
- Nastavitve za mqtt
- Network time (NTP) 

## Priprava okolja za prevajanje (Ubuntu 18.04)

Okolje je pripravljeno po navodilih [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html)

* Instalacija potrebnih aplikacij:

```bash
sudo apt-get install gcc git wget make libncurses-dev flex bison gperf python python-pip python-setuptools python-serial python-cryptography python-future python-pyparsing python-pyelftools
```

* Instalacija toolchaina (trenutna verzija 8.2.0):

```bash
mkdir esp
cd ~/esp
wget https://dl.espressif.com/dl/xtensa-esp32-elf-gcc8_2_0-esp32-2019r1-linux-amd64.tar.gz
tar -xzf xtensa-esp32-elf-gcc8_2_0-esp32-2019r1-linux-amd64.tar.gz
```

* Instalacija ESP-IDF okolja

```bash
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
```

* Vnos poti do prevajalnika in ESP-IDF okolja

```bash
export PATH="$HOME/esp/xtensa-esp32-elf/bin:$PATH"
export IDF_PATH=~/esp/esp-idf
```

* Update ESP-IDF okolja

```bash
cd esp-idf
git pull
git submodule update --init --recursive
```
##Konfiguracija projekta
Vse nastavitve za aplikacijo so izvedene preko spremenljivk v strukturi `settings`, ki so shranjene v NVS (Non-volatile-storage). 
Struktura `settings` spremenljivke:

```c
typedef struct settingsStruct {
  char wifi_ssid[32];
  char wifi_password[64];
  char serial_number[16];
  char ntp_server1[32];
  char mqtt_server[32];
  int mqtt_port;
  char mqtt_cert_path[32]; //mqtt certificate path
  char mqtt_key_path[32]; //mqtt private key path
  char mqtt_root_topic[32];
  char mqtt_subscribe_topic[32];
  char mqtt_publish_topic[32];
  int8_t mqtt_tls;
  int8_t connection_mode; //wifi or ethernet
  int8_t wifi_max_retry;
} settingsStruct_t;
```
Settings struktura je ob prvem programiranju prazna. Za ta primer se lahko v menuconfig definirajo začetne vrednosti teh spremenljivk.

Vnesi `make menuconfig` in izberi `Ethernet Board Configuration` menu.

Na voljo so naslednje nastavitve:
- Serial Number (15 alfanumeričnih znakov, potrebnih za unikaten mqtt topic)
- WiFi or Ethernet connection --->
- Bluetooth provisioning --->
- Web Server  --->
- WiFi Configuration  --->
    - WiFi SSID  
    - WiFi Password 
    - Maximum WiFi connection retry (kolikokrat naj se poskuša WiFi povezat v omrežje, da ne obstane v neskončni zanki)
- Ethernet Configuration  --->
    - Ethernet PHY Device (LAN8720)  --->
    - Ethernet PHY Address
    - Use PHY Power (enable / disable) pin     
- MQTT Configuration --->
    - Broker URL (format: `mqtt://hostname<port>` ali `mqtts://hostname<port>`
    - Client certificate filename
    - Client key filename   
    - MQTT Root Topic
    - MQTT Publish Topic
    - MQTT Subscribe Topic
    - MQTT Buffer Size    
- NTP Server URL
- LED GPIO Number

###Ostale nastavitve za ethernet
In the `Component config > Ethernet` menu:

* Enable `Use ESP32 internal EMAC controller`, and then go into this menu.
* In the `PHY interface`, it's highly recommended that you choose `Reduced Media Independent Interface (RMII)` which will cost fewer pins.
* In the `RMII clock mode`, you can choose the source of RMII clock (50MHz): `Input RMII clock from external` or `Output RMII clock from internal`.
* Once `Output RMII clock from internal` is enabled, you also have to set the number of the GPIO used for outputting the RMII clock under `RMII clock GPIO number`. In this case, you can set the GPIO number to 16 or 17.
* Once `Output RMII clock from GPIO0 (Experimental!)` is enabled, then you have no choice but GPIO0 to output the RMII clock.
* Set SMI MDC/MDIO GPIO number according to board schematic, by default these two GPIOs are set as below:

  | Default Example GPIO | RMII Signal | Notes         |
  | -------------------- | ----------- | ------------- |
  | GPIO23               | MDC         | Output to PHY |
  | GPIO18               | MDIO        | Bidirectional |

* If you have connect a GPIO to the PHY chip's RST pin, then you need to enable `Use Reset Pin of PHY Chip` and set the GPIO number under `PHY RST GPIO number`.

### Extra configuration in the code (Optional)

* By default Ethernet driver will assume the PHY address to `1`, but you can alway reconfigure this value after `eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();`. The actual PHY address should depend on the hardware you use, so make sure to consult the schematic and datasheet.

##Bluetooth
Preko bluetooth-a je podprto konfiguriranje WiFi (WiFi provisioning). Če se te funkcionalnosti ne potrebuje, je priporočljivo onemogočiti Bluetooth v menuconfig-u, ker se na ta način sprosti okoli 64 kbytov RAM-a. 

###Onemogočanje bluetooth
V `menuconfig` je potrebno onemogočiti bluetooth na dveh mestih:

 `Component config > Bluetooth` 
 
 `Ethernet Board Configuration > Bluetooth provisioning > Disabled`

#MQTT client
mqtt client je prijavljen na topic:
`settings.mqtt_root_topic/settings.serial_number/settings.mqtt_subscribe_topic`

pošilja pa na:
`settings.mqtt_root_topic/settings.serial_number/settings.mqtt_publish_topic`



##MQTT komande

###Set settings komanda (brez mqtt security)
Na subscribe topic se pošlje komanda v sledečem formatu:

```json
{
"data":
  {
	"cmd": "set_settings",
       "serial_number":	"12345678",	
		"wifi_ssid":"Tsenzor",
       "wifi_password":"mer2senzor_temp33",
       "wifi_max_retry": 10,			
		"ntp_server1":	"ntp1.arnes.si",
		"mqtt_server":	"10.120.4.160",
		"mqtt_root_topic":"devices",
       "mqtt_pub_topic":"publish",
       "mqtt_sub_topic":"cmd",		
		"mqtt_tls":	0,
		"mqtt_port":	1883,               
		"connection_mode":	1
  }
}
```
###Set settings (z mqtt security)
Za mqtt security se potrebujeta client certifikat in client private key. Postopek je opisan v dokumentu README_MQTT.md.
TLS Security potrebuje za delovanje kot 50kB RAM-a.

Na subscribe topic se pošlje komanda v sledečem formatu:

```
{
"data":
  {
	"cmd": "set_settings",
       "serial_number":	"12345678",	
		"wifi_ssid":"Tsenzor",
       "wifi_password":"mer2senzor_temp33",
       "wifi_max_retry": 10,			
		"ntp_server1":	"ntp1.arnes.si",
		"mqtt_server":	"10.120.4.160",
		"mqtt_root_topic":"devices",
       "mqtt_pub_topic":"publish",
       "mqtt_sub_topic":"cmd",		
		"mqtt_tls":	1,
		"mqtt_port":	8883,
       "mqtt_cert_path": "mqtt_cert.pem",
       "mqtt_key_path": "mqtt_private_key.pem",        
		"connection_mode":	1
  }
}
```
##Get settings komanda
Na subscribe topic se pošlje komanda v sledečem formatu:

```json
{
"data": {
	   "cmd": "get_settings"
       }
}
```

#OTA Upgrade
SW upgrade poteka preko vgrajene OTA funkcionalnosti. Ta podpira upgrade preko https strežnika, od koder se naloži upgrade datoteka.
Komanda za upgrade se pošilja preko mqtt, vsebuje pa pot do datoteke in certifikat strežnika, ki se potrebuje za varen prenos. 

##Postavitev https strežnika
Strežnik se lahko postavi na linux platformi s pomočjo openssl aplikacije. Najprej je potrebno generirati certifikat:

```bash
openssl req -x509 -newkey rsa:2048 -keyout ca_key.pem -out ca_cert.pem -days 365 -nodes
```

Vsa polja lahko pustiš na default, le Common Name (e.g. server FQDN or YOUR name): je potrebno vpisati IP naslov ali hostname strežnika. 

##Start https strežnika

```bash
openssl s_server -WWW -key ca_key.pem -cert ca_cert.pem -port 8070
```

##Pridobivanje certifikata za upgrade komando:
Ko https strežnik že teče, si lahko priskrbimo certifikat, ki ga bomo potrebovali pri upgrade komandi:

```bash
openssl s_client -showcerts -connect <hostname>: <port>
```

Primer:

```bash
openssl s_client -showcerts -connect 10.120.10.36:8070
```
##Upgrade komanda
Na subscribe topic se pošlje komanda v sledečem formatu:

```json
{ 
  "data": {
        "cmd": "upgrade_app",
        "uri": "https://10.120.10.36:8070/ethernet_board.bin",
        "cert":"ota_cert.pem"
  }
}
```
#SPIFFS

spiffsgen.py is a write-only Python SPIFFS implementation used to create filesystem images from the contents of a host folder. To use spiffsgen.py, open Terminal and run:

```bash
spiffsgen.py <image_size> <base_dir> <output_file>
```
Example:

```bash
$IDF_PATH/components/spiffs/spiffsgen.py 0xf0000 spiffs/ spiffs.bin
```
Nalaganje f SPI filesystema:

```bash
$IDF_PATH/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 230400 write_flash -z 0x310000 spiffs.bin
```


#Originalna navodila za ethernet projekt

This example demonstrates basic usage of `Ethernet driver` together with `tcpip_adapter`. The work flow of the example could be as follows:

1. Initialize the Ethernet interface and enable it
2. Send DHCP requests and wait for a DHCP lease
3. If get IP address successfully, then you will be able to ping the device

If you have a new Ethernet application to go (for example, connect to IoT cloud via Ethernet), try this as a basic template, then add your own code.

## How to use example

### Hardware Required

To run this example, you should have one ESP32 dev board integrated with an Ethernet interface, or just connect your ESP32 core board to a breakout board which featured with RMII Ethernet PHY. **Currently ESP-IDF officially supports three Ethernet PHY: `TLK110`, `LAN8720` and `IP101`, additional PHY drivers can be implemented by users themselves.**

### Configure the project

Enter `make menuconfig` if you are using GNU Make based build system or enter `idf.py menuconfig` if you' are using CMake based build system. Then go into `Example Configuration` menu.

* Choose PHY device under `Ethernet PHY Device` option

* Set PHY address under `Ethernet PHY address` option, this address depends on the hardware and the PHY configuration. Consult the documentation/datasheet for the PHY hardware you have.
  * Address 1 for the common Waveshare LAN8720 PHY breakout and the official ESP32-Ethernet-Kit board

* Check whether or not to control PHY's power under `Use PHY Power (enable / disable) pin` option, (if set true, you also need to set PHY Power GPIO number under `PHY Power GPIO` option)

* Set SMI MDC/MDIO GPIO number according to board schematic, default these two GPIOs are set as below:

  | Default Example GPIO | RMII Signal | Notes         |
  | -------------------- | ----------- | ------------- |
  | GPIO23               | MDC         | Output to PHY |
  | GPIO18               | MDIO        | Bidirectional |

* Select one kind of EMAC clock mode under `Ethernet PHY Clock Mode` option. Possible configurations of the clock are listed as below:

  | Mode     | GPIO Pin | Signal name  | Notes                                                        |
  | -------- | -------- | ------------ | ------------------------------------------------------------ |
  | external | GPIO0    | EMAC_TX_CLK  | Input of 50MHz PHY clock                                     |
  | internal | GPIO0    | CLK_OUT1     | Output of 50MHz APLL clock                                   |
  | internal | GPIO16   | EMAC_CLK_OUT | Output of 50MHz APLL clock                                   |
  | internal | GPIO17   | EMAC_CLK_180 | Inverted output of 50MHz APLL clock (suitable for long clock trace) |

  * The external reference clock of 50MHz must be supplied on `GPIO0`.
  * The ESP32 can generate a 50MHz clock using internal APLL. When the APLL is already used as clock source for other purposes (most likely I²S), you have no choice but choose external clock.

### Build and Flash

Enter `make -j4 flash monitor` if you are using GNU Make based build system or enter `idf.py build flash monitor` if you' are using CMake based build system.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output

```bash
I (0) cpu_start: App cpu up.
I (265) heap_init: Initializing. RAM available for dynamic allocation:
I (272) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (278) heap_init: At 3FFB3FF0 len 0002C010 (176 KiB): DRAM
I (284) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (291) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (297) heap_init: At 4008851C len 00017AE4 (94 KiB): IRAM
I (303) cpu_start: Pro cpu start user code
I (322) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (325) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE
I (325) emac: emac reset done
I (335) eth_example: Ethernet Started
I (4335) eth_example: Ethernet Link Up
I (6325) tcpip_adapter: eth ip: 192.168.2.164, mask: 255.255.255.0, gw: 192.168.2.2
I (6325) eth_example: Ethernet Got IP Address
I (6325) eth_example: ~~~~~~~~~~~
I (6325) eth_example: ETHIP:192.168.2.164
I (6335) eth_example: ETHMASK:255.255.255.0
I (6335) eth_example: ETHGW:192.168.2.2
I (6345) eth_example: ~~~~~~~~~~~
```

## Troubleshooting

* If the PHY address is incorrect then the EMAC will still be initialized, but all attempts to read/write configuration registers in the PHY's register will fail, for example, waiting for auto-negotiation done.

* Check PHY Clock

  > The ESP32's MAC and the External PHY device need a common 50MHz reference clock. This clock can either be provided externally by a crystal oscillator (e.g. crystal connected to the PHY or a separate crystal oscillator) or internally by generating from EPS32's APLL. The signal integrity of this clock is strict, so it is highly recommended to add a 33Ω resistor in series to reduce ringing.

* Check GPIO connections, the RMII PHY wiring is fixed which can not be changed through either IOMUX or GPIO Matrix. They're described as below:

  | GPIO   | RMII Signal | ESP32 EMAC Function |
  | ------ | ----------- | ------------------- |
  | GPIO21 | TX_EN       | EMAC_TX_EN          |
  | GPIO19 | TX0         | EMAC_TXD0           |
  | GPIO22 | TX1         | EMAC_TXD1           |
  | GPIO25 | RX0         | EMAC_RXD0           |
  | GPIO26 | RX1         | EMAC_RXD1           |
  | GPIO27 | CRS_DV      | EMAC_RX_DRV         |

* Check GPIO0

  > GPIO0 is a strapping pin for entering UART flashing mode on reset, care must be taken when using this pin as `EMAC_TX_CLK`. If the clock output from the PHY is oscillating during reset, the ESP32 may randomly enter UART flashing mode. One solution is to use an additional GPIO as a "power pin", which either powers the PHY on/off or enables/disables the PHY's own oscillator. This prevents the clock signal from being active during a system reset. For this configuration to work, `GPIO0` also needs a pullup resistor and the "power pin" GPIO will need a pullup/pulldown resistor - as appropriate in order to keep the PHY clock disabled when the ESP32 is in reset. See the example source code to see how the "power pin" GPIO can be managed in software.

