# Control de motor stepper con Arduino

Control de motor paso a paso con comandos por serial e interfaz con LabVIEW, sin uso de firmwares o librerías de LB para Arduino. Proyecto de laboratorio 6 y 7.
Diseñado para automatizar mediciones con un electroimán rotante, en el laboratorio de bajas temperaturas del DF, FCEN (UBA).

## Algunas funciones:
* Homing automático con microswitch
* Interrupciones de seguridad con microswitches
* Movimiento preseteado automático
* Mini librería para manejar funciones de pulsadores y debounce
* Protocolo de comunicación por serial con LabVIEW
* Interfaz gráfica de LabVIEW con comandos y gráficos en tiempo real

Programamos todo el control del motor en Arduino, LabVIEW funciona solo como interfaz gráfica para envíar comandos y recibir información. 
El programa funciona independientemente de LabVIEW y los comandos se pueden envíar por serial monitor o con pulsadores.

Usamos la librería [Accelstepper]https://www.airspayce.com/mikem/arduino/AccelStepper/
Para calibración y errores ver las [ecuaciones que usa la librería accelstepper]
https://www.embedded.com/generate-stepper-motor-speed-profiles-in-real-time/

