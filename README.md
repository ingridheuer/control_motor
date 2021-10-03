# Control y automatización de electroimán rotante con motor paso a paso (Proyecto de laboratorio 6 y 7)

Diseñado para automatizar mediciones angulares en experimentos de superconductividad, en el laboratorio de bajas temperaturas del DF, FCEN (UBA).
El programa controla un motor paso a paso que se usa para girar la plataforma que sostiene al electroimán. La idea es poder automatizar el movimiento de la plataforma para poder realizar más mediciones sin tener que rotar el equipo manualmente.

Queríamos darle bastante flexibildiad al código para que más adelante se puedan agregar comandos y funciones nuevas, así que usamos un "protocolo" sencillo para usar comandos desde el monitor serial del IDE. Armamos también un programa en LabVIEW que se comunica con el Arduino usando el mismo protocolo, para tener una interfaz gráfica desde la PC. Para que el sistema pueda ser independiente de la PC, también agregamos controles con pulsadores.

Por seguridad agregamos interrupciones con microswitches de fin de recorrido, que también usamos para homing y calibración.

## Algunas funciones:
* Homing automático
* Interrupciones de seguridad con microswitches de fin de recorrido
* Movimiento preseteado automático: se puede elegir principio y fin, velocidad de ida y vuelta y cantidad de repeticiones.
* Mini lib para manejar funciones de pulsadores y switches, con debounce (filtrado de ruidos).
* Corrección automática de backlash (juego de la reducción) en cambio de dirección
* Programa de LabVIEW para controlar con interfaz gráfica y realizar mediciones 
* Protocolo de comunicación por serial con LabVIEW

Usamos la librería [Accelstepper](https://www.airspayce.com/mikem/arduino/AccelStepper/).

Para calibración y errores ver las [ecuaciones que usa la librería](https://www.embedded.com/generate-stepper-motor-speed-profiles-in-real-time/)

