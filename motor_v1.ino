/* Automatización de motor V1
   Ingrid Heuer, Marco Crivaro Nicolini
   Julio 2021
   Labo 7

   Automatización y control del electroimán del laboratorio de bajas temperaturas

   Qué hace:
   Control por serial, botonera
   Homing automático con microswitches
   Movimiento continuo con botonera
   Homing por botonera
   Movimiento automático por serial
   Config. de velocidad por serial
   Interrupción por microswitches
   Feedback de la posición (en pasos) respecto a un 0 (home)
   Media vuelta automática buscando switches


   Qué falta (a 20/7):
   Interfaz con labview
   Media vuelta automática sin switches
   Mediciones configuradas por usuario (ver labview)
   Probar todos los comandos de la botonera (me faltan cables)
   Config. de aceleración por serial
   Ajustar por reducción (labview?)
   Ajustar por "juego" (labview?)
   Feedback en ángulos (labview?)
   Ajuste de velocidad y aceleración preseteados (se puede hacer desde aca, ver por labview)
   Calibración (labview?)
*/

//librería para control de steppers
#include <AccelStepper.h>

// ======= Variables para leer el Serial
const byte numChars = 32;
char receivedChars[numChars];
char tempChars[numChars];        // temporary array for use when parsing

// variables to hold the parsed data
char message[numChars] = {0};
int integer_input = 0;
String command;

boolean newData = false;

// =======


// ======== Variables para inicializar el motor
int initial_homing = -1; //va a arrancar moviendose un poquito para atrás
int initial_ending = 1;
long left = -1;
long right = 1;
bool at_home = false;
bool at_end = false;
int Position;
int steps_to_end = 800; //lo que corresponda a media vuelta (configurar esto)

//importante que estén todos cableados como pullup (la resistencia va al positivo)
#define greenpin 12
#define redpin 13
#define home_switch 2
#define end_switch 3
#define right_button 8
#define left_button 9
#define home_button 10
#define dir 4
#define pulse 5

//1=driver de 2 cables
AccelStepper stepper(1, dir, pulse);

//========== Rutinas de interrupción

void ISRhome() {
  if (!digitalRead(home_switch)) {
    stepper.stop();
    digitalWrite(redpin, HIGH);
  }
  else {
    digitalWrite(redpin, LOW);
  }
}

void ISRend() {
  if (!digitalRead(end_switch)) {
    stepper.stop();
    digitalWrite(redpin, HIGH);
  }
  else {
    digitalWrite(redpin, LOW);
  }
}

//======================

void setup() {
  Serial.begin(9600);
  delay(5);

  stepper.setMaxSpeed(100);
  stepper.setAcceleration(50);
  stepper.moveTo(-1);

  pinMode(home_switch, INPUT_PULLUP);
  pinMode(end_switch, INPUT_PULLUP);
  pinMode(greenpin, OUTPUT);
  pinMode(redpin, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(home_switch), ISRhome, CHANGE);
  attachInterrupt(digitalPinToInterrupt(end_switch), ISRend, CHANGE);

  delay(10);
  homing();

}


void loop() {
  //===== comandos via serial

  readserial();
  if (newData == true) {
    strcpy(tempChars, receivedChars);
    // this temporary copy is necessary to protect the original data
    //   because strtok() used in parseData() replaces the commas with \0
    parseData();
    showParsedData();
    newData = false;

    if (command.equals("run")) {
      digitalWrite(greenpin, HIGH);
      stepper.moveTo(integer_input);
      stepper.runToPosition();
      digitalWrite(greenpin, LOW);
    }
    else if (command.equals("rehome")) {
      at_home = false;
      homing();
    }
    else if (command.equals("current position")) {
      Position = stepper.currentPosition();
      Serial.print(Position);
    }
    else if (command.equals("speed")) {
      stepper.setMaxSpeed(integer_input);
    }
    else if (command.equals("ida y vuelta")) {
      ida_y_vuelta();
    }
  }

  //==== comandos via botonera
  /*
      //se mueve mientras se apreta el botón
      if (!digitalRead(right_button)){
        right++;
        stepper.moveTo(right);
        stepper.run();
      }

      if (!digitalRead(left_button)){
        left--;
        stepper.moveTo(left);
        stepper.run();
      }

      if(!digitalRead(home_button)){
        homing();
      }

      if (stepper.isRunning()){
        digitalWrite(greenpin,HIGH);
      }
      else
      {
        digitalWrite(greenpin,LOW);
      }

  */

}

//============= 
//Funciones de movimiento 
//==============

void homing() {
  if (!at_home) {

    delay(5);
    Serial.print("Start homing \n");

    //se va a mover un pasito a la vez mientras el switch no esté activado
    while (digitalRead(home_switch)) {
      digitalWrite(greenpin, HIGH);
      stepper.moveTo(initial_homing);
      initial_homing--;
      stepper.run();
      delay(5);
    }

    //una vez que activó el switch le decimos que ese es el 0

    stepper.setCurrentPosition(0);
    initial_homing = 1; //ahora va para el otro lado hasta que suelta el switch

    while (!digitalRead(home_switch)) {
      stepper.moveTo(initial_homing);
      stepper.run();
      initial_homing++;
      delay(5);
    }

    //soltó el switch, fin del homing
    stepper.setCurrentPosition(0);
    Serial.print("Done Homing \n");
  }

  at_home = true;
  digitalWrite(greenpin, LOW);
}

void to_end() {
  if (!at_end) {
    
    delay(5);
    Serial.print("Going to end \n");

    //se va a mover un pasito a la vez mientras el switch no esté activado
    while (digitalRead(end_switch)) {
      digitalWrite(greenpin, HIGH);
      stepper.moveTo(initial_ending);
      initial_ending++;
      stepper.run();
      delay(5);
    }

    //una vez que activó el switch le decimos que ese es el 180

    stepper.setCurrentPosition(steps_to_end);
    initial_ending = -1; //ahora va para el otro lado hasta que suelta el switch

    while (!digitalRead(end_switch)) {
      stepper.moveTo(initial_ending);
      stepper.run();
      initial_ending--;
      delay(5);
    }

    //soltó el switch, fin del homing
    stepper.setCurrentPosition(steps_to_end); //configurar esto
    Serial.print("At end \n");
  }

  at_end = true;
  digitalWrite(greenpin, LOW);
}

void ida_y_vuelta() {
  homing();
  Serial.print("At start \n");
  to_end();
  Serial.print("At end \n");
  at_home = false;
  homing();
  Serial.print("Done \n");
}



//========================
//Funciones de Serial
//========================

void readserial() {
  static boolean recvInProgress = false;
  static byte ndx = 0;
  char startMarker = '<';
  char endMarker = '>';
  char rc;

  while (Serial.available() > 0 && newData == false) {
    rc = Serial.read();

    if (recvInProgress == true) {
      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      }
      else {
        receivedChars[ndx] = '\0'; // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    }

    else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
}

//============

void parseData() {      // split the data into its parts

  char * strtokIndx; // this is used by strtok() as an index

  strtokIndx = strtok(tempChars, ",");     // get the first part - the string
  strcpy(message, strtokIndx); // copy it to message

  strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
  integer_input = atoi(strtokIndx);     // convert this part to an integer

  command = String(message);

}


//============

void showParsedData() {
  Serial.print("Comando: ");
  Serial.println(command);
  Serial.print("Input: ");
  Serial.println(integer_input);
}
