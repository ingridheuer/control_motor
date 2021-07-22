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



// ======== Variables para inicializar el motor
int initial_homing = -1; //va a arrancar moviendose un poquito para atrás
long left = -1;
long right = 1;
bool at_home = false;
int Position;
int backlash_0;
int backlash_1;

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
  //attachInterrupt(digitalPinToInterrupt(end_switch), ISRend, CHANGE);

  delay(10);
  homing();

}


void loop() {

  //===== se ejecutan solo si le hablamos por serial
  readserial();
  if (newData == true) {
    strcpy(tempChars, receivedChars);
    // this temporary copy is necessary to protect the original data
    //   because strtok() used in parseData() replaces the commas with \0
    parseData();
    showParsedData();
    newData = false;
    setpoint();
    measure();
    rehome();
    setspeed();
    setaccel();

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

          if (stepper.isRunning()){
            digitalWrite(greenpin,HIGH);
          }
          else
          {
            digitalWrite(greenpin,LOW);
          }

    */
  }

  //============ se ejecuta en cada loop
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
  if (Position == 0){
    at_home = true;
  }
  else{
    at_home = false;
  }
}

void measure() {
  if (command == "measure") {
    Serial.println(Position);
  }
}

void setpoint() {
  if (command == "setpoint") {
    stepper.moveTo(integer_input);
  }
}

void setspeed() {
  if (command == "setspeed") {
    stepper.setMaxSpeed(integer_input);
  }
}

void setaccel() {
  if (command == "setaccel") {
    stepper.setAcceleration(integer_input);
  }
}

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
    backlash_1 = stepper.currentPosition();
    stepper.setCurrentPosition(0);
    Serial.print("Done Homing \n");
  }

  at_home = true;
  digitalWrite(greenpin, LOW);
  Serial.println("Backlash: ");
  Serial.println(backlash_1);
}

void rehome(){
  if (command == "rehome"){
    homing();
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


//============

void showParsedData() {
  Serial.print("Comando: ");
  Serial.println(command);
  Serial.print("Input: ");
  Serial.println(integer_input);
}
