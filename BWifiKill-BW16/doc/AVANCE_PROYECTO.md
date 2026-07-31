# BWifiKill - Avance del firmware

## Plataforma

- Modulo: Ai-Thinker BW16 / RTL8720DN.
- Core: Arduino Realtek AmebaD 3.1.9.
- Pantalla: ST7735 vertical con rotacion 0.
- Botones: UP PB1, OK PB3, DOWN PB2.
- Firmware: BWifiKill-BW16.

## Estado general

El firmware ya integra funciones WiFi, laboratorio de deauth, analisis de trafico, Bluetooth BLE y pantallas de presentacion. La version actual dejo de ser un prototipo de escaneo y ya funciona como herramienta de demostracion para laboratorio controlado.

## Funciones implementadas

- Splash screen con icono personalizado.
- Menu principal: WiFi, Bluetooth y Sistema.
- Menus redisenados con icono central, contador de posicion `N/TOTAL`, etiqueta resaltada y animacion ligera.
- Scanner WiFi 2.4 GHz / 5 GHz.
- Separacion de redes por banda.
- Detalle de red con SSID, BSSID, canal, RSSI y seguridad.
- Radar WiFi: scan inicial, seleccion de banda y AP, seguimiento por BSSID y lectura RSSI con barrida verde animada.
- Seleccion y persistencia de objetivo por BSSID.
- Atajos de ejecucion:
  - `WiFi > Deauther rapido`.
  - Desde detalle de red: `OK` guarda objetivo y otro `OK` ejecuta Deauther.
  - Desde objetivo actual: `OK` ejecuta Deauther.
- Analizador pasivo por banda:
  - Total de redes.
  - Red mas fuerte.
  - Canal mas cargado.
- Sniffer WiFi promiscuo para 2.4 GHz y 5 GHz:
  - Conteo de tramas totales y bytes.
  - Conteo de management/control/data.
  - Conteo de beacons, probe request/response, deauth, disassoc y EAPOL.
  - Salto de canales por banda.
- Analizador de trafico WiFi:
  - Historial de 60 bins de 100 ms.
  - PPS/FPS efectivo.
  - Baseline movil.
  - Estado LOW/NORMAL/HIGH.
  - Barras por canal y canal mas cargado.
- Monitor de objetivo:
  - Re-scan por BSSID.
  - Actualizacion de RSSI/canal.
  - Deteccion de objetivo perdido.
- Estadisticas de laboratorio:
  - Muestras.
  - Detecciones.
  - Perdidas.
  - RSSI minimo/promedio/maximo.
  - Ultimo canal.
- Bluetooth BLE:
  - Scan de dispositivos BLE.
  - Lista de dispositivos con RSSI y nombre.
  - Clasificacion por fabricante/tipo: Apple, iBeacon, Microsoft, Google, Samsung, Named y BLE generico.
  - Analizador BLE con PPS, RSSI promedio, baseline e historial.
- BLE spam:
  - Publicidad BLE no conectable.
  - Rotacion de nombres BLE.
  - Contador de anuncios.
- Beacon spam WiFi:
  - Emision de beacons de laboratorio por 2.4 GHz o 5 GHz.
  - Rotacion de SSID.
  - Salto de canales.
  - Contador de TX.

## Deauther / prueba activa de laboratorio

La opcion `WiFi > Deauther rapido` ya invoca `packet-injection.cpp` desde el menu. El flujo actual es:

1. Verifica que exista un objetivo seleccionado.
2. Valida PMF probable por seguridad WPA3, WPA2/WPA3 o WPA2 AES-CMAC.
3. Convierte el BSSID `AA:BB:CC:DD:EE:FF` a bytes MAC.
4. Fija el canal del objetivo.
5. Ejecuta ciclos controlados de packet injection.
6. Envia tramas de deauth con origen/AP igual al BSSID objetivo y destino broadcast.
7. Muestra contador de paquetes enviados en pantalla.
8. Permite detener manualmente con `OK`.
9. Al detener manualmente, regresa al menu anterior sin re-scan.

Para evitar congelamientos del driver Realtek al encadenar demasiadas tramas de management, la implementacion usa ciclos controlados:

- `LAB_DEAUTH_CYCLE_LIMIT = 10`.
- `LAB_DEAUTH_TX_GAP_MS = 100`.
- Rearme de WiFi entre ciclos con `wifi_off()`, `wifi_on(RTW_MODE_STA)` y `wifi_set_channel(channel)`.

Este comportamiento fue elegido porque en pruebas de laboratorio el driver se congelaba al saturar la cola interna de `dump_mgntframe()`.

## Funcion principal actual

La opcion `Deauth > Prueba principal` ejecuta el flujo de medicion:

1. Valida que exista un objetivo seleccionado.
2. Guarda el BSSID objetivo.
3. Ejecuta un scan WiFi.
4. Busca el mismo BSSID.
5. Actualiza estadisticas.
6. Evalua el estado:
   - `MEDIDO`: objetivo observado y datos registrados.
   - `BLOCKED`: WPA3/WPA2-WPA3/PMF probable.
   - `IDLE`: sin objetivo o prueba no ejecutada.
7. Imprime reporte por Serial.

## Pendientes recomendados

- Validar con el profesor el alcance permitido de las funciones activas: deauth, beacon spam y BLE spam.
- Definir escenarios de demo:
  - Red WPA2 sin PMF para observar efecto de deauth.
  - Red WPA3 o WPA2/WPA3 con PMF para demostrar bloqueo por defensa moderna.
  - Trafico WiFi normal vs trafico alto para el analizador.
  - BLE scan con dispositivos reales del entorno.
- Documentar evidencia de la demo:
  - Capturas de pantalla del contador TX.
  - Cliente movil desconectando/reconectando en red de prueba.
  - Logs Serial.
  - Comparacion con PMF activo.
- Revisar los nombres de SSID/BLE usados en spam antes de presentar, para mantener tono academico.

## Notas tecnicas

- La transmision activa depende de simbolos internos del driver Realtek y no de una API publica estable.
- PMF/802.11w puede hacer que deauth no tenga efecto; eso debe presentarse como resultado esperado de una defensa correcta.
- El AP objetivo puede seguir apareciendo en scan aunque un cliente se desconecte; el beacon del AP no desaparece por deauth.
- La evidencia mas clara del deauth es observar un cliente asociado, no solo el listado de redes.

## Repositorios de referencia

tesa-klebeband:

- Paquetes de inyeccion y envio de tramas de gestion.
- https://github.com/tesa-klebeband
- https://github.com/tesa-klebeband/RTL8720dn-WiFi-Packet-Injection

Rusyln:

- Referencia de conexiones y firmware para BW16 con ST7735.
- Modulo: Ai-Thinker BW16 / RTL8720DN.
- Pantalla: ST7735 vertical.
- Botones: UP PB1, OK PB3, DOWN PB2.
- https://github.com/rusyln
- https://github.com/rusyln/R4TKN-FIRMWARE-BW16
