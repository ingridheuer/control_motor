/* Automatización de motor V1
   Ingrid Heuer, Marco Crivaro Nicolini
   Julio 2021
   Labo 7

   Automatización y control del electroimán del laboratorio de bajas temperaturas
*/

//librería para control de steppers
#include <AccelStepper.h>

// ======= Variables para leer el Serial
const byte numChars = 32;
char receivedChars[numChars];
char tempChars[numChars];        // array temporario para parsing

// variables para parsing
char message[numChars] = {0};
int integer_input = 0;
String command;

boolean newData = false;
// =======

enum states {DISABLED, INIT, TOSTART, TOEND};
uint8_t state = DISABLED;

// ======== Variables para inicializar el motor
//int initial_homing = -1;
long left = -1;
long right = 1;
//bool at_home = false;
int Position;
int backlash;
int Start = 0;
int Stop = 1;
int current_loops;
int total_loops;
int speed_ida = 500;
int speed_vuelta = 500;
// =========
#define greenpin 12
#define redpin 13
#define home_switch 2
#define end_switch 3
#define right_button 8
#define left_button 9
#define home_button 10
#define dir 4
#define pulse 5

//1 = driver de 2 cables
AccelStepper stepper(1, dir, pulse);

//========== Rutinas de interrupción

void ISRhome() {
  if (!digitalRead(home_switch)) {
    stepper.stop();
    state = DISABLED;
    digitalWrite(redpin, HIGH);
  }
  else {
    digitalWrite(redpin, LOW);
  }
}


void ISRend() {
  if (!digitalRead(end_switch)) {
    stepper.stop();
    state = DISABLED;
    digitalWrite(redpin, HIGH);
  }
  else {
    digitalWrite(redpin, LOW);
  }
}

//======================

void setup() {
  Serial.begin(9600);

  stepper.setMaxSpeed(500);
  stepper.setAcceleration(200);

  pinMode(home_switch, INPUT_PULLUP);
  pinMode(end_switch, INPUT_PULLUP);
  pinMode(greenpin, OUTPUT);
  pinMode(redpin, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(home_switch), ISRhome, CHANGE);
  attachInterrupt(digitalPinToInterrupt(end_switch), ISRend, CHANGE);
}


void loop() {

  //===== se ejecutan solo si le hablamos por serial
  readserial();
  if (newData == true) {
    strcpy(tempChars, receivedChars); //hacemos una copia temporal de lo que recibimos en tempChars para proteger los datos originales
    parseData();
    newData = false;
    setpoint();
    measure();
    rehome();
    setspeed();
    setaccel();
    set_start();
    set_stop();
    if (command == "backlash") {
      Serial.println(backlash);
    }
    if (command == "speed_ida") {
      speed_ida = integer_input;
      Serial.print("Speed ida set at: ");
      Serial.println(speed_ida);
    }
    if (command == "speed_vuelta"){
      speed_vuelta = integer_input;
      Serial.print("Speed vuelta set at: ");
      Serial.println(speed_vuelta);
    }

    //Manda el motor al inicio de la medición y cambia el estado a init. Resetea el contador de vueltas
    if (command == "automed") {
      total_loops = integer_input;
      current_loops = 0;
      stepper.moveTo(Start);
      state = INIT;
      Serial.println("Automed init");
    }

    if (command == "stop"){
      Serial.println("Stopping");
      stepper.stop();
      state = DISABLED;
    }

    if (command == "state"){
      Serial.println(state);
    }


    //==== comandos via botonera
    //==== revisar si no conviene que use move()
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

    */
  }

  //============ se ejecuta en cada loop
  
  automed();
  run_onestep();

  if (stepper.isRunning()) {
    digitalWrite(greenpin, HIGH);
  }
  else
  {
    digitalWrite(greenpin, LOW);
  }

}

//=============
//Funciones de movimiento
//==============

void run_onestep() {
  stepper.run();
  Position = stepper.currentPosition();
  /*
  if (Position == 0) {
    at_home = true;
  }
  else {
    at_home = false;
  }*/
}

void measure() {
  if (command == "measure") {
    Serial.println(Position);
  }
}

void setpoint() {
  if (command == "setpoint") {
    stepper.moveTo(integer_input);
    Serial.print("Setpoint: ");
    Serial.println(integer_input);
  }
}

void setspeed() {
  if (command == "setspeed") {
    stepper.setMaxSpeed(integer_input);
    Serial.print("Speed set: ");
    Serial.println(integer_input);
  }
}

void setaccel() {
  if (command == "setaccel") {
    stepper.setAcceleration(integer_input);
    Serial.print("Accel set: ");
    Serial.println(integer_input);
  }
}

void set_start() {
  if (command == "setstart") {
    Start = integer_input;
    Serial.print("Start set at: ");
    Serial.println(Start);
  }
}

void set_stop() {
  if (command == "setstop") {
    Stop = integer_input;
    Serial.print("Stop set at: ");
    Serial.println(Stop);
  }
}

void rehome() {
  if ((command == "rehome")){
    Serial.println("Homing");
    digitalWrite(greenpin, HIGH);

    while (digitalRead(home_switch)) {
      stepper.move(-1);
      stepper.run();
    }

    //stepper wait ==========
    stepper.setCurrentPosition(0); //para medir el backlash

    while (!digitalRead(home_switch)) {
      stepper.move(1);
      stepper.run();
    }

    backlash = stepper.currentPosition(); //cuantos pasos dió desde que pisó el switch hasta que lo soltó
    stepper.setCurrentPosition(0);
    digitalWrite(greenpin, LOW);
    //at_home = true;
  }
}

void automed() {
  switch (state) {

    case DISABLED: //default
        break;      

    case INIT: //si ya se empezó el automed y se llegó al inicio, cambia el setpoint a Stop y pasa a TOEND
      if (stepper.distanceToGo() == 0) {
        stepper.moveTo(Stop);
        stepper.setMaxSpeed(speed_ida);
        state = TOEND;
      }
      break;

    case TOEND: //idem pero si llegó al final, pasa a TOSTART
      if (stepper.distanceToGo() == 0) {
        stepper.moveTo(Start);
        stepper.setMaxSpeed(speed_vuelta);
        state = TOSTART;
      }
      break;

    case TOSTART: //si terminó suma una vuelta al contador. si dio todas las vueltas pasa a DISABLED, sino vuelve a empezar
      if (stepper.distanceToGo() == 0){
        current_loops++;
        if (current_loops == total_loops){
          state = DISABLED;
        }
        else{
          state = INIT;
        }
      }
      break;
  }
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
