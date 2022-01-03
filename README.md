# Control y automatización de electroimán rotante

## About:

Este programa es parte de un proyecto de laboratorio 6 y 7, donde diseñamos un sistema para controlar la rotación de un electroimán que se usa para experimentos de superconductividad. 
Las mediciones se realizaban girando el electroimán manualmente, lo cual era muy tedioso y complicaba la adquisición de datos.
Nosotros diseñamos un sistema mecánico con un motor para rotar la plataforma, este programa se encarga de controlar ese motor (entre otras cosas).

El programa cumple principalmente 3 funciones:
- Controla un motor stepper con varias funciones para automatizar y medir su rotación.
- Implementa un "protocolo" simple para controlarlo con comandos de texto (que podría ser adaptado para controlar sistemas similares).
- Tiene una interfaz gráfica para facilitar su uso e integrarlo con otros sistemas de medición.

Para controlar y automatizar el motor usamos un Arduino UNO, para la interfaz gráfica armamos un programa de LabVIEW que se comunica con el Arduino con protocolo VISA.

## Algunas funciones:
* Homing automático (con microswitch, se podría adaptar a un sensor óptico)
* Interrupciones de seguridad con microswitches de fin de recorrido (idem anterior)
* Movimiento preseteado automático: se define principio y fin, velocidad de ida y vuelta y cantidad de repeticiones, el equipo hace todo el recorrido y mide automáticamente.
* Movimiento escalonado: como el anterior, pero definiendo un "ancho" de pasos y deteniendose en cada paso (esto mejora la precisión angular).
* Manejo de pulsadores y microswitches, con debounce*.
* Corrección automática de backlash** al cambiar de dirección.

Usamos la librería [Accelstepper](https://www.airspayce.com/mikem/arduino/AccelStepper/) para controlar el motor y [digitalPinFast](https://github.com/TheFidax/digitalPinFast) para reducir el tiempo de ejecución.

Para calibración del motor y curvas de velocidad ver las [ecuaciones que usa Accelstepper](https://www.embedded.com/generate-stepper-motor-speed-profiles-in-real-time/)

No usamos librerías de Arduino en el programa de LabVIEW, manejamos la comunicación por serial con los subVI's de VISA y el protocolo de texto que armamos.
