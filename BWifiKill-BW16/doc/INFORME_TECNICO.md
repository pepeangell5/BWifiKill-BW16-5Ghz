# Informe tecnico - BWifiKill BW16 / RTL8720DN

## Contexto

El firmware BWifiKill usa un modulo Ai-Thinker BW16 con chipset RTL8720DN y core Arduino Realtek AmebaD 3.1.9. El objetivo academico es evaluar, en un entorno de laboratorio autorizado, comportamiento WiFi/BLE, trafico visible, protecciones modernas y efectos de tramas de gestion sobre clientes asociados.

La version actual incluye funciones pasivas de medicion y funciones activas de demostracion. Las funciones activas deben ejecutarse solo en redes y dispositivos de prueba.

## Modulos principales

### WiFi scanner

El scanner WiFi registra SSID, BSSID, canal, RSSI y seguridad. Permite separar redes por 2.4 GHz y 5 GHz, guardar un objetivo por BSSID y consultar detalles del objetivo seleccionado.

### Deauther de laboratorio

La funcion Deauther realiza una prueba activa controlada:

1. Requiere un objetivo seleccionado por BSSID.
2. Bloquea o marca como no efectiva la prueba si detecta PMF probable:
   - WPA3.
   - WPA2/WPA3 mixto.
   - WPA2 AES-CMAC.
3. Convierte el BSSID del AP objetivo a bytes MAC.
4. Fija el canal del objetivo.
5. Envia tramas de deauth con:
   - Source/AP: BSSID objetivo.
   - Destination: broadcast.
   - Reason code: `0x06`.
6. Muestra contador de paquetes enviados.
7. Se detiene manualmente con `OK`.
8. Al detenerse por boton regresa al menu anterior sin re-scan.

### Control de estabilidad del driver

Durante las pruebas se observo que el driver Realtek puede congelarse si se encadenan demasiadas llamadas a `dump_mgntframe()`. Para mitigarlo, el firmware usa ciclos de transmision:

- Envia hasta `LAB_DEAUTH_CYCLE_LIMIT` paquetes por ciclo.
- Espera `LAB_DEAUTH_TX_GAP_MS` entre paquetes.
- Rearma el WiFi entre ciclos con:
  - `wifi_off()`.
  - `wifi_on(RTW_MODE_STA)`.
  - `wifi_set_channel(channel)`.

Este diseno reduce saturacion de la cola interna del driver y permite una demostracion mas estable.

### Prueba principal

La prueba principal es una medicion por BSSID. Ejecuta scan, busca el objetivo, actualiza estadisticas y clasifica el resultado como medido, bloqueado o sin objetivo.

### Sniffer WiFi promiscuo

El sniffer usa modo promiscuo para observar trafico en 2.4 GHz o 5 GHz con salto de canales. Registra:

- Tramas totales.
- Bytes totales.
- Tramas management, control y data.
- Beacons.
- Probe requests/responses.
- Deauth/disassoc observados.
- EAPOL.
- Frames por canal.

### Analizador WiFi

El analizador WiFi toma muestras de trafico en bins de 100 ms y mantiene 60 bins de historial. Calcula PPS/FPS efectivo, baseline movil y estado LOW/NORMAL/HIGH. Tambien muestra actividad por canal y canal mas ocupado.

### Bluetooth BLE scanner

El scanner BLE lista dispositivos cercanos y registra direccion, nombre, RSSI, fabricante y tipo. Clasifica anuncios comunes:

- iBeacon.
- Apple Continuity.
- Microsoft.
- Google.
- Samsung.
- Dispositivos con nombre.
- BLE generico.

### Analizador BLE

El analizador BLE mantiene historial de paquetes por segundo, RSSI promedio, baseline movil y estado LOW/NORMAL/HIGH. Sirve para mostrar actividad BLE del entorno.

### Beacon spam WiFi

El modulo Beacon spam genera beacons de laboratorio en 2.4 GHz o 5 GHz. Usa una lista de SSID, cambia canal por banda y mantiene contador de TX. Es una funcion activa y debe limitarse a entorno controlado.

### BLE spam

El modulo BLE spam anuncia nombres BLE no conectables y rota el nombre publicado. Muestra el nombre actual y contador de TX. Debe usarse solo como demostracion local autorizada.

## Limitacion por PMF / 802.11w

Un deauth clasico depende de tramas de gestion 802.11 no protegidas. Si la red objetivo tiene PMF/802.11w activo, especialmente con WPA3 o WPA2/WPA3 mixto, los clientes compatibles deben ignorar deauth/disassoc no protegidos.

Por esto, una prueba puede "fallar" por una defensa correcta de la red, no por error del firmware.

## Interpretacion de resultados

- Si un cliente se desconecta y reconecta durante Deauther, existe efecto observable de las tramas de gestion sobre el cliente.
- Si el AP sigue apareciendo en scan, no significa que Deauther no funcione: el beacon del AP puede seguir transmitiendose normalmente.
- Si el cliente no se desconecta en WPA3/PMF, el resultado esperado es proteccion efectiva.
- Si el firmware detecta PMF probable, el resultado debe reportarse como bloqueado o no efectivo.

## Recomendacion para evaluacion

Se recomienda presentar tres escenarios:

1. Red WPA2 de laboratorio sin PMF:
   - Resultado esperado: desconexion/reconexion temporal del cliente.
2. Red WPA3 o WPA2/WPA3 con PMF:
   - Resultado esperado: bloqueo o ausencia de efecto.
3. Trafico WiFi/BLE ambiental:
   - Resultado esperado: conteo de frames, canales ocupados, dispositivos BLE y variaciones de PPS.

## Riesgos y controles

- Las funciones activas pueden interferir con redes/dispositivos si se usan fuera del laboratorio.
- El driver Realtek usa simbolos internos para raw TX; por tanto, la estabilidad depende del control de tasa.
- La demo debe ejecutarse en red propia o autorizada.
- Conviene documentar configuracion del AP, seguridad, canal, cliente usado y observaciones.

## Conclusion tecnica

El firmware actual ya demuestra seleccion de objetivo, medicion pasiva, inyeccion controlada de tramas, observacion de trafico WiFi, analisis BLE y generacion de anuncios/beacons de laboratorio. La principal limitacion tecnica no es la UI ni la seleccion de objetivo, sino la estabilidad del driver al transmitir tramas raw y la proteccion PMF de redes modernas. Ambas limitaciones estan controladas o documentadas dentro del flujo actual.
