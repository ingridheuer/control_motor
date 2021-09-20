
/* Automatización de motor V1
   Ingrid Heuer
   Julio 2021
   Labo 7

   Automatización y control del electroimán del laboratorio de bajas temperaturas
*/

#include <AccelStepper.h> //librería para control de steppers
#include <PushButton.h> //mini lib que me arme para los botones

//Configuración general
const byte homeSwitch = 2,
           //endSwitch = 3,
           dirPin = 4,
           pulsePin = 5,
           rightPin = 6,
           leftPin = 7,
           stopPin = 8,
           homePin = 9,
           greenPin = 10,
           redPin = 11;

const long debounceDelay = 50;
bool buttonsEnabled = true;
long baudRate = 115200;

AccelStepper stepper(1, dirPin, pulsePin); // 1 -> driver de 2 cables

PushButton rightButton(rightPin, false, debounceDelay);
PushButton leftButton(leftPin, false, debounceDelay);
PushButton stopButton(stopPin, true, debounceDelay);
PushButton homeButton(homePin, true, debounceDelay);

//=====================================
// Variables para movimiento automático
//=====================================

//Estados que puede tomar el programa cuando está en modo automático
enum states
{
  autoDisabled, goingToStart, waitingStart, goingToFinish, waitingFinish
};
states state = autoDisabled;

typedef struct {
  long start;
  long finish;
  int total_loops;
  int speed_ida;
  int speed_vuelta;
} Recorrido;

Recorrido recorrido;

long backlash;
const long wait = 1000;
//===========================
// Variables para leer Serial
//===========================

// Si entraron datos por serial o no
bool newData = false;

//El mensaje que tiene que armar
typedef struct {
  String command;
  int integer_input;
  long long_input;
} Message;

const byte numChars = 32;
char receivedChars[numChars];
char tempChars[numChars];

//=========================
// Rutinas de interrupción
//=========================

void ISRhome() {
  if (!digitalRead(homeSwitch)) {
    stepper.stop();
    state = autoDisabled;
    buttonsEnabled = false;
    digitalWrite(redPin, HIGH);
  }
  else {
    digitalWrite(redPin, LOW);
  }
}

/*
  void ISRend() {
  if (!digitalRead(endSwitch)) {
    stepper.stop();
    state = autoDisabled;
    buttonsEnabled = false;
    digitalWrite(redPin, HIGH);
  }
  else {
    digitalWrite(redPin, LOW);
  }
  }
*/
//======================

void setup() {
  Serial.begin(baudRate);

  stepper.setMaxSpeed(100);
  stepper.setAcceleration(200);

  pinMode(homeSwitch, INPUT_PULLUP);
  //pinMode(endSwitch, INPUT_PULLUP);
  pinMode(greenPin, OUTPUT);
  pinMode(redPin, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(homeSwitch), ISRhome, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(endSwitch), ISRend, CHANGE);
}


void loop() {

  readserial();
  //===== se ejecutan solo si le hablamos por serial
  if (newData == true) {
    strcpy(tempChars, receivedChars);
    Message newMessage = parseData();
    newData = false;
    serialCommands(newMessage);
  }

  //============ se ejecutan en cada loop
  buttonCommands();
  automed();
  stepper.run();

  if (stepper.isRunning()) {
    digitalWrite(greenPin, HIGH);
  }
  else {
    digitalWrite(greenPin, LOW);
    if ((state == autoDisabled) && !buttonsEnabled) {
      //Si los deshabilité y terminó de moverse, los vuelvo a prender
      buttonsEnabled = true;
    }
  }
}

//=========================
// Funciones de movimiento
//=========================

void rehome() {
  digitalWrite(greenPin, HIGH);

  while (digitalRead(homeSwitch)) {
    stepper.move(-1);
    stepper.run();
  }

  stepper.setCurrentPosition(0); //para medir el backlash

  while (!digitalRead(homeSwitch)) {
    stepper.move(1);
    stepper.run();
  }

  backlash = stepper.currentPosition(); //cuantos pasos dió desde que pisó el switch hasta que lo soltó
  stepper.setCurrentPosition(0);
  digitalWrite(greenPin, LOW);
}

void automed() {

  //se inicializan a 0 por default
  static unsigned long startMillis;
  static unsigned long stopMillis;
  static int current_loop;

  switch (state) {

    case autoDisabled:
      break;

    case goingToStart:
      if (stepper.distanceToGo() == 0) {
        startMillis = millis();
        current_loop++;
        if (current_loop > recorrido.total_loops) {
          //Serial.println("Termine las vueltas, cambio a disabled y reseteo loops");
          state = autoDisabled;
          current_loop = 0;
        }
        else {
          //Serial.println("Cambio a waiting start");
          state = waitingStart;
        }
      }
      break;

    case waitingStart:
      if (millis() - startMillis > wait) {
        //Serial.println("Cambio a going to finish");
        stepper.moveTo(recorrido.finish);
        stepper.setMaxSpeed(recorrido.speed_ida);
        state = goingToFinish;
      }
      break;

    case goingToFinish:
      if (stepper.distanceToGo() == 0) {
        //Serial.println("Cambio a waiting finish");
        stopMillis = millis();
        state = waitingFinish;
      }
      break;

    case waitingFinish:
      if (millis() - stopMillis > wait) {
        //Serial.println("Cambio a going to stsart");
        stepper.moveTo(recorrido.start);
        stepper.setMaxSpeed(recorrido.speed_vuelta);
        state = goingToStart;
      }
      break;
  }
}

//====================================
// Funciones para comandos de LabVIEW
//====================================

void serialCommands(Message message) {
  if (message.command == "MES") {
    Serial.println(stepper.currentPosition());
  }
  else if (message.command == "STP") {
    Serial.println("STP");
    stepper.stop();
    state = autoDisabled;
    buttonsEnabled = false;
  }
  else if (message.command == "SET") {
    Serial.println("SET");
    buttonsEnabled = false;
    stepper.setMaxSpeed(message.integer_input);
    stepper.moveTo(message.long_input);
  }
  else if (message.command == "ACC") {
    Serial.println("ACC");
    stepper.setAcceleration(message.integer_input);
  }
  else if (message.command == "VUE") {
    Serial.println("VUE");
    recorrido.start = message.long_input;
    recorrido.speed_vuelta = message.integer_input;
  }
  else if (message.command == "IDA") {
    Serial.println("IDA");
    recorrido.finish = message.long_input;
    recorrido.speed_ida = message.integer_input;
  }
  else if (message.command == "AUT") {
    Serial.println("AUT");
    recorrido.total_loops = message.integer_input;
    stepper.moveTo(recorrido.start);
    buttonsEnabled = false;
    state = goingToStart;
  }
  else if (message.command == "BCK") {
    Serial.println(backlash);
  }
  else if (message.command = "HOM") {
    Serial.println("HOM");
    rehome();
  }
  else if (message.command == "INF") {
    Serial.print("Auto State: ");
    Serial.println(state);
    Serial.print("Buttons State: ");
    Serial.println(buttonsEnabled);
    Serial.print("BAUD rate: ");
    Serial.println(baudRate);
  }
  else if (message.command == "IDN"){
    Serial.println("Ingrid Heuer, Agosto 2021");
  }
  else {
    Serial.println("NAK");
  }
}



//=============
// Botones
//=============

void buttonCommands() {
  if (stopButton.isOn()) {
    //Serial.println("Stop button pressed");
    stepper.stop();
    state = autoDisabled;
    buttonsEnabled = false;
  }
  else if (buttonsEnabled && rightButton.isOn()) {
    //Serial.println("+1");
    stepper.move(1);
  }
  else if (buttonsEnabled && leftButton.isOn()) {
    //Serial.println("-1");
    stepper.move(-1);
  }
  else if (homeButton.isOn()) {
    //Serial.println("Home button pressed");
    rehome();
  }
}


//=========================
// Comunicación por Serial
//=========================


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


Message parseData() {      // split the data into its parts
  Message newMessage;
  char received[numChars] = {0};

  char * strtokIndx; // this is used by strtok() as an index

  strtokIndx = strtok(tempChars, ",");     // get the first part - the string
  strcpy(received, strtokIndx); // copy it to message
  newMessage.command = String(received);

  strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
  newMessage.integer_input = atoi(strtokIndx);     // convertimos a un int

  strtokIndx = strtok(NULL, ",");
  newMessage.long_input = atol(strtokIndx);  //convertimos a un long

  return newMessage;
}
