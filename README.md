# Control de motor stepper con Arduino

Control de motor paso a paso con comandos por serial e interfaz con LabVIEW. Proyecto de laboratorio 6 y 7.
Diseñado para automatizar mediciones con un electroimán rotante, en el laboratorio de bajas temperaturas del DF, FCEN (UBA).

## Algunas funciones:
* Homing automático con microswitch
* Interrupciones de seguridad con microswitches
* Movimiento preseteado automático
* Mini lib para manejar funciones de pulsadores y switches, con debounce (filtrado de ruidos).
* Corrección automática de backlash en cambio de dirección
* Programa de LabVIEW para controlar con interfaz gráfica
* Interfaz de LabVIEW para mediciones
* Protocolo de comunicación por serial con LabVIEW

Programamos todo el control del motor en Arduino, LabVIEW funciona solo como interfaz gráfica para envíar comandos y recibir información. 
El programa funciona independientemente de LabVIEW y los comandos se pueden envíar por serial monitor del IDE.

Usamos la librería [Accelstepper](https://www.airspayce.com/mikem/arduino/AccelStepper/).

Para calibración y errores ver las [ecuaciones que usa la librería](https://www.embedded.com/generate-stepper-motor-speed-profiles-in-real-time/)

