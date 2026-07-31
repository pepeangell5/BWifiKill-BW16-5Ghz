# BWifiKill BW16 5GHz

<p align="center">
  <a href="https://www.instagram.com/esp32_tools/">
    <img alt="Instagram: esp32_tools" src="https://img.shields.io/badge/Instagram-esp32_tools-E4405F?style=for-the-badge&logo=instagram&logoColor=white">
  </a>
  <a href="https://www.facebook.com/esp32-tools">
    <img alt="Facebook: esp32-tools" src="https://img.shields.io/badge/Facebook-esp32--tools-1877F2?style=for-the-badge&logo=facebook&logoColor=white">
  </a>
  <a href="https://github.com/pepeangell5">
    <img alt="GitHub: pepeangell5" src="https://img.shields.io/badge/GitHub-pepeangell5-181717?style=for-the-badge&logo=github&logoColor=white">
  </a>
</p>

Firmware academico para Ai-Thinker BW16 / RTL8720DN orientado a pruebas de laboratorio WiFi 2.4 GHz / 5 GHz y BLE. El proyecto integra escaneo, seleccion de objetivo, analisis de trafico, sniffer promiscuo, prueba Deauther controlada, Beacon spam de laboratorio, BLE scanner y BLE spam.

> Uso previsto: laboratorio propio o autorizado. Las funciones activas de transmision deben ejecutarse solo en redes y dispositivos de prueba.

## Estructura del repositorio

```text
BWifiKill-BW16-5Ghz/
|-- README.md
`-- BWifiKill-BW16/
    |-- bw16/
    |   |-- bw16.ino
    |   |-- *.cpp / *.h
    |   `-- img/
    `-- doc/
        |-- AVANCE_PROYECTO.md
        |-- INFORME_TECNICO.md
        `-- img/
```

## Plataforma

- Modulo: Ai-Thinker BW16.
- Chipset: Realtek RTL8720DN.
- Core Arduino: Realtek AmebaD 3.1.9.
- Pantalla: ST7735 TFT vertical.
- Controles: 3 botones fisicos.
- Firmware: BWifiKill.
- Version declarada: `v0.2`.

## Componentes usados

- 1 modulo Ai-Thinker BW16 / RTL8720DN.
- 1 pantalla TFT ST7735.
- 3 botones momentaneos.
- Resistencias/cableado segun montaje.
- Cable USB para programacion y monitor serial.
- Red WiFi de laboratorio 2.4 GHz / 5 GHz.
- Cliente de prueba, por ejemplo celular o laptop.

## Conexiones

### Pantalla ST7735

| Pin ST7735 | Conexion BW16 | Funcion |
| --- | --- | --- |
| `GND` | `GND` | Tierra comun |
| `VCC` | `3V3` | Alimentacion a 3.3 V |
| `SCL` / `SCK` / `CLK` | `PA14` | Reloj SPI |
| `SDA` / `MOSI` / `DIN` | `PA12` | Datos SPI hacia pantalla |
| `CS` | `PA27` | Chip select, `TFT_CS` |
| `DC` / `A0` | `PA25` | Datos/comando, `TFT_DC` |
| `RST` / `RES` | `PA26` | Reset, `TFT_RST` |
| `BLK` / `LED` | `PA30` | Retroiluminacion, `TFT_BL` |
| `MISO` / `SDA_OUT` | Sin conectar | No se utiliza en esta interfaz |

La libreria usada por la interfaz es `Adafruit_ST7735` con rotacion vertical `0`.

### Botones

Los botones trabajan con `INPUT_PULLUP`, por lo que deben cerrar a GND al presionarse.

| Boton | Pin BW16 | Otro terminal | Funcion |
| --- | --- | --- | --- |
| `UP` | `PB1` | `GND` | Navegar arriba / volver en algunas pantallas |
| `OK` | `PB3` | `GND` | Entrar / ejecutar / detener pruebas activas |
| `DOWN` | `PB2` | `GND` | Navegar abajo / volver en algunas pantallas |

### Montaje de referencia

#### Diagrama de conexiones completo

<p align="center">
  <img src="BWifiKill-BW16/doc/img/conexiones-bw16.jpg" alt="Diagrama completo de conexiones BW16" width="960">
</p>

#### Componentes

<p align="center">
  <img src="BWifiKill-BW16/doc/img/ST7735-pantalla.jpg" alt="Pantalla ST7735" width="210">
  <img src="BWifiKill-BW16/doc/img/ST7735-pantalla-frente.jpg" alt="Frente de pantalla ST7735" width="210">
  <img src="BWifiKill-BW16/doc/img/boton.png" alt="Botones" width="150">
</p>

## Funciones principales

### Menu principal

- WiFi.
- Bluetooth.
- Sistema.

![Menu principal](BWifiKill-BW16/bw16/img/splash.JPG)

### WiFi

El menu WiFi incluye:

- Scan redes.
- Radar WiFi.
- Objetivo.
- Deauther rapido.
- Analizador.
- Trafico 2.4G.
- Trafico 5G.
- Sniffer 2.4G.
- Sniffer 5G.
- Beacon 2.4G.
- Beacon 5G.

![Menu WiFi](BWifiKill-BW16/bw16/img/wifi/wifi-menu.JPG)

El scanner separa redes por banda, permite elegir objetivo por BSSID y muestra SSID, BSSID, canal, RSSI y seguridad.

El Radar WiFi realiza un scan inicial, permite elegir banda y AP, y sigue el BSSID elegido con una visualizacion RSSI animada. La barrida continua mientras se obtiene la siguiente lectura.

![Scan WiFi](BWifiKill-BW16/bw16/img/wifi/wifi-scann.JPG)
![Objetivo WiFi](BWifiKill-BW16/bw16/img/wifi/wifi-objetivo.JPG)

### Deauth / Deauther

El modulo Deauther usa el objetivo seleccionado por BSSID y ejecuta una prueba activa controlada en laboratorio.

Flujo tecnico:

1. Valida objetivo seleccionado.
2. Detecta PMF probable por WPA3, WPA2/WPA3 o WPA2 AES-CMAC.
3. Convierte BSSID a bytes MAC.
4. Fija canal del objetivo.
5. Envia tramas de deauth a broadcast con origen/AP igual al BSSID objetivo.
6. Muestra contador de tramas TX aceptadas.
7. Permite detener con `OK`.
8. Al detener, regresa al menu anterior sin re-scan.

Para evitar congelamientos del driver Realtek, la transmision se ejecuta por ciclos:

```cpp
static const uint8_t LAB_DEAUTH_CYCLE_LIMIT = 10;
static const uint16_t LAB_DEAUTH_TX_GAP_MS = 100;
```

Entre ciclos se rearma WiFi con `wifi_off()`, `wifi_on(RTW_MODE_STA)` y `wifi_set_channel(channel)`.

![Menu Deauth](BWifiKill-BW16/bw16/img/deauth/deauth-menu.JPG)
![Deauther](BWifiKill-BW16/bw16/img/deauth/deauth.JPG)
![Precheck](BWifiKill-BW16/bw16/img/deauth/deauth-precheck.JPG)
![Resultados](BWifiKill-BW16/bw16/img/deauth/deauth-resultados.JPG)

### Analizador y sniffer WiFi

El sniffer WiFi usa modo promiscuo con salto de canales para 2.4 GHz y 5 GHz. Registra:

- Tramas totales.
- Bytes totales.
- Management/control/data.
- Beacons.
- Probe request/response.
- Deauth/disassoc observados.
- EAPOL.
- Frames por canal.

El analizador WiFi conserva historial de 60 bins de 100 ms, calcula PPS/FPS efectivo, baseline movil y estado LOW/NORMAL/HIGH.

![Trafico 2.4G](BWifiKill-BW16/bw16/img/wifi/wifi-trafico-2g.JPG)
![Trafico 5G](BWifiKill-BW16/bw16/img/wifi/wifi-trafico-5g.JPG)
![Sniffer 2.4G](BWifiKill-BW16/bw16/img/wifi/sniffer-2g.JPG)
![Sniffer 5G](BWifiKill-BW16/bw16/img/wifi/sniffer-5g.JPG)

### Beacon spam de laboratorio

El modulo Beacon spam transmite beacons con SSID rotativo en 2.4 GHz o 5 GHz, cambia de canal por banda y muestra contador TX.

![Beacon 2.4G](BWifiKill-BW16/bw16/img/wifi/beacon-2g.JPG)
![Beacon 5G](BWifiKill-BW16/bw16/img/wifi/beacon-5g.JPG)

### Bluetooth BLE

El menu Bluetooth incluye:

- Scan dispositivos.
- Analizador.
- BLE spam.

El scanner BLE lista dispositivos cercanos y clasifica anuncios por fabricante/tipo:

- iBeacon.
- Apple Continuity.
- Microsoft.
- Google.
- Samsung.
- Dispositivos con nombre.
- BLE generico.

![Menu Bluetooth](BWifiKill-BW16/bw16/img/bluetooth/bluetooth-menu.JPG)
![BLE Scan](BWifiKill-BW16/bw16/img/bluetooth/bt-scan.JPG)
![BLE Analizador](BWifiKill-BW16/bw16/img/bluetooth/bt-analiza.JPG)
![BLE Spam](BWifiKill-BW16/bw16/img/bluetooth/bt-spam.JPG)

### Sistema

Muestra datos generales del firmware, objetivo y escaneo.

![Menu Sistema](BWifiKill-BW16/bw16/img/sistema/sistema-menu.JPG)
![Sistema Info](BWifiKill-BW16/bw16/img/sistema/sistem-info.JPG)

## PMF / WPA3

PMF / 802.11w protege tramas de administracion como deauthentication y disassociation. En redes WPA3 o WPA2/WPA3 mixtas, clientes compatibles deben ignorar deauth no protegidos.

Por eso, si una red con PMF activo no se ve afectada, el resultado debe interpretarse como defensa correcta, no como error del firmware.

## Compilacion

Abrir `BWifiKill-BW16/bw16/bw16.ino` en Arduino IDE con el core Realtek AmebaD instalado y seleccionar:

```text
FQBN: realtek:AmebaD:Ai-Thinker_BW16
```

Compilacion verificada localmente:

```text
El Sketch usa 910076 bytes (43%) del espacio de almacenamiento de programa.
```

## Documentacion

- [Avance del proyecto](BWifiKill-BW16/doc/AVANCE_PROYECTO.md)
- [Informe tecnico](BWifiKill-BW16/doc/INFORME_TECNICO.md)

## Agradecimientos y referencias

Este proyecto se apoyo en investigacion, ejemplos y referencias de firmware para RTL8720DN/BW16:

- tesa-klebeband: referencia de packet injection y envio de tramas de gestion.
  - https://github.com/tesa-klebeband
  - https://github.com/tesa-klebeband/RTL8720dn-WiFi-Packet-Injection
- Rusyln: referencia de firmware BW16, conexiones ST7735 y botones.
  - https://github.com/rusyln
  - https://github.com/rusyln/R4TKN-FIRMWARE-BW16

Gracias a esos trabajos base fue posible adaptar el firmware a un flujo academico de laboratorio, con interfaz propia, seleccion de objetivo, mediciones y documentacion tecnica.
