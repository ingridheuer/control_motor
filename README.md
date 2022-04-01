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
* Manejo de pulsadores y microswitches, con debounce.*
* Corrección automática de backlash.**

\* Filtrado de ruidos en la señal de los pulsadores y microswitches.\
\** Al cambiar de dirección el motor se puede "desacoplar" del eje y deja de transmitir la rotación hasta que se vuelve a acoplar.

##

Usamos la librería [Accelstepper](https://www.airspayce.com/mikem/arduino/AccelStepper/) para controlar el motor y [digitalPinFast](https://github.com/TheFidax/digitalPinFast) para reducir el tiempo de ejecución.

Para calibración del motor y curvas de velocidad ver las [ecuaciones que usa Accelstepper](https://www.embedded.com/generate-stepper-motor-speed-profiles-in-real-time/)

No usamos librerías de Arduino en el programa de LabVIEW, manejamos la comunicación por serial con los subVI's de VISA y el protocolo de texto que armamos.


## Instalación:
Se requiere tener instalado previamente Arduino IDE y LabVIEW versión 2016 o posterior.

Descargar la carpeta PushButton en el directorio de librerías de arduino (/Arduino/libraries). Funciona como una pequeña "librería" para manejar los pulsadores del gabinete. 

Descargar el archivo motor.ino en la carpeta de sketches de arduino (/Arduino/sketchbook).

En la carpeta labview está el main vi y los subvi, dejarlos todos juntos en la misma carpeta.

##Uso desde LabVIEW:
Asegurarse de que el Arduino esté conectado vía USB antes de iniciar el programa. Nota: si el gabinete está apagado, el arduino va a funcionar igual por la corriente del USB, si se prende después el gabinete no pasa nada pero el Arduino se va a reiniciar.

Abrir el programa motor_main.vi, el front panel debería ser el siguiente:
![front panel](front_panel.PNG?raw=true "Title")

El recuadro verde que dice "Status" indica si se conectó con el arduino correctamente. Además cuando el arduino se enciende, parpadean las luces del gabinete 3 veces.

Las funciones de los botones son:

Home: hace un homing automático para definir el origen de pasos del motor y calibrar el backlash. Se recomienda hacer un homing al encender y antes de apagar el sistema

Medir Ángulo: dice cual es el ángulo actual

Ir al Setpoint: gira el electroimán hasta la posición elegida (medida desde el origen/home)

Mov. Relativo: gira el electroimán x grados relativos a la posición actual

Definir Ángulo: para redefinir la posición actual, por si se quiere cambiar el origen de la rotación

STOP detiene cualquier movimiento que esté haciendo el motor.

EXIT detiene el programa y cierra la conexión con el Arduino. IMPORTANTE: siempre cerrar el programa con EXIT para evitar problemas con la conexión al arduino y VISA.

En el recuadro de configuración general se puede elegir la velocidad de giro, la frecuencia de adquisición y el microstep. El microstep se debe configurar solo si se cambió desde el driver del motor. El botón de backlash nos dice cual es el último backlash que se midió en la calibración.

###Adquisición:
Para empezar la adquisición de datos activar el botón "adquirir ángulo". Se va a ver en tiempo real el ángulo de rotación del motor en el recuadro negro de arriba a la derecha. 

Para automatizar la rotación hay dos opciones:

Movimiento continuo: configurar el principio y fin de recorrido, velocidades de ida y vuelta y cantidad de repeticiones en el recuadro de "Automático Continuo". El electroimán va a hacer un barrido "continuo".

Automático discreto: el electroimán se mueve de a pasos de ancho constante, mejora la adquisición. Configurar en "total" el ángulo total que se quiere barrer y en "paso" el ancho de los pasos (en grados).

Para adquirir y guardar los datos (junto con los datos de los demás equipos), abrir este programa junto con el de Juan. Cuando se quiere medir, activar el botón de "adquirir ángulo" y después iniciar el programa de Juan.

##Uso desde Arduino IDE:
Se puede controlar todo el dispositivo desde el Serial Monitor del IDE usando los comandos que están especificados en el archivo "comandos.txt", respetando la sintaxis <CMD,int,long> (esto está mejor explicado en la sección 2.4 del informe).

Se pueden agregar y cambiar todos los comandos que quieran siempre y cuando respeten esa sintaxis y tengan cuidado de no causar overflows (también está explicado en 2.4 del informe)
