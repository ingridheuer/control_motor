# Control de motor stepper con Arduino

## Qué hace (a 20/7):
+ Homing automático con microswitch
+ Comandos por serial:
  - Config de velocidad y aceleración
  - Setpoint
  - Homing
  - Medir posición (en pasos, respecto al home)
  - Media vuelta
+ Rutina de interrupción por microswitches
+ Medir posición en instante arbitrario (no bloquea)
+ Calcula backlash en homing 
+ Media vuelta automática
+ Comandos básicos con botonera: movimiento, dirección y homing
+ Interfaz con labview
+ Ajustado por reducción 

## Qué falta (a 20/7):
+ Probar todos los comandos de la botonera (me faltan cables)
+ Ajustar por "juego" (labview?)
+ Feedback en ángulos (labview?)
+ Ajuste de velocidad y aceleración preseteados (se puede hacer desde aca, ver por labview)
+ Calibración (labview?)

Para calibración y errores ver las ecuaciones que usa la librería accelstepper:
https://www.embedded.com/generate-stepper-motor-speed-profiles-in-real-time/

