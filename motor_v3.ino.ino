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
           endSwitch = 3,
           dirPin = 4,
           pulsePin = 5,
           rightPin = 6,
           leftPin = 7,
           stopPin = 8,
           homePin = 9,
           greenPin = 10,
           redPin = 11;

const long debounceDelay = 50;
volatile bool buttonsEnabled = true;
long baudRate = 115200;
bool Print = false;

AccelStepper stepper(1, pulsePin, dirPin); // 1 -> driver de 2 cables

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
volatile states state = autoDisabled;

typedef struct {
  long start;
  long finish;
  int total_loops;
  int speed_ida;
  int speed_vuelta;
} Recorrido;

Recorrido recorrido;

//==========================
//Calibración y backlash
//==========================
long backlash = 0;
const long wait = 1000;

const bool CCW = true;
const bool CW = false; //CW = clockwise, CCW = counterclockwise ---> recordar que CW es true y CCW es false, also, positivo = CCW, negativo = CW

bool direc = CCW; //xq home empieza CCW
bool directionHasChanged = false; //empieza false asi no corrige porque si

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

//======================

void setup() {
  Serial.begin(baudRate);

  stepper.setMaxSpeed(400);  //Con la reduccion de 1:60 y el microswitch de 400 esto es 1 RPM en la plataforma
  stepper.setAcceleration(200);  //Esto hay que calibrar y elegirlo

  pinMode(homeSwitch, INPUT_PULLUP);
  pinMode(endSwitch, INPUT_PULLUP);
  pinMode(greenPin, OUTPUT);
  pinMode(redPin, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(homeSwitch), ISRhome, CHANGE);
  attachInterrupt(digitalPinToInterrupt(endSwitch), ISRend, CHANGE);

  blinkLeds(3, 300);

}

//============================================================
void loop() {

  readserial();
  if (newData == true) {
    strcpy(tempChars, receivedChars);
    Message newMessage = parseData();
    newData = false;
    serialCommands(newMessage);
  }

  buttonCommands();
  automed();

  updateDirection();
  adjustBacklash();

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

  if (Print) {
    Serial.println(stepper.currentPosition());
  }

}
//=================================================================================

//=========================
// Funciones de movimiento
//=========================

void gohome() {
  digitalWrite(greenPin, HIGH);
  Serial.println("Start homing");

  while (digitalRead(homeSwitch)) {
    stepper.move(-60);
    stepper.run();
  }

  Serial.println("Step back");
  stepper.setCurrentPosition(0); //para medir el backlash

  while (!digitalRead(homeSwitch)) {
    stepper.move(60);
    stepper.run();
  }

  Serial.println("Done");
  backlash = stepper.currentPosition(); //cuantos pasos dió desde que pisó el switch hasta que lo soltó
  stepper.setCurrentPosition(0);
  direc = CCW; //xq home termina yendo CW
  directionHasChanged = false;
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
          Serial.println("Termine las vueltas, cambio a disabled y reseteo loops");
          state = autoDisabled;
          current_loop = 0;
        }
        else {
          Serial.println("Cambio a waiting start");
          state = waitingStart;
        }
      }
      break;

    case waitingStart:
      if (millis() - startMillis > wait) {
        Serial.println("Cambiando direccion");
        //stepper.runToNewPosition(recorrido.start + backlash);
        //stepper.setCurrentPosition(recorrido.start);

        //Serial.println("Cambio a going to finish");
        stepper.moveTo(recorrido.finish);
        direc = getDirection();
        stepper.setMaxSpeed(recorrido.speed_ida);
        state = goingToFinish;
      }
      break;

    case goingToFinish:
      if (stepper.distanceToGo() == 0) {
        Serial.println("Cambio a waiting finish");
        stopMillis = millis();
        state = waitingFinish;
      }
      break;

    case waitingFinish:
      if (millis() - stopMillis > wait) {
        Serial.println("Cambiando direccion");
        //stepper.runToNewPosition(recorrido.finish - backlash);
        //stepper.setCurrentPosition(recorrido.finish);

        //Serial.println("Cambio a going to stsart");
        stepper.moveTo(recorrido.start);
        direc = getDirection();
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
    direc = getDirection();
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
    direc = getDirection();
    buttonsEnabled = false;
    state = goingToStart;
  }
  else if (message.command == "BCK") {
    Serial.println(backlash);
  }
  else if (message.command == "HOM") {
    Serial.println("HOM");
    gohome();
  }
  else if (message.command == "SCP") {
    Serial.println("SCP");
    stepper.setCurrentPosition(message.long_input);
  }
  else if (message.command == "INF") {
    String info = getInfo();
    Serial.println(info);
  }
  else if (message.command == "IDN") {
    Serial.println("Ingrid Heuer, Agosto 2021");
  }
  else if (message.command == "PRT") {
    Print = true;
  }
  else if (message.command == "PRF") {
    Print = false;
  }
  else if (message.command == "DIR") {
    Serial.println(direc);
  }
  else if (message.command == "BLK"){
    Serial.println("BLK");
    stepper.setMaxSpeed(message.integer_input);
    stepper.move(message.long_input);
    stepper.runToPosition();
    direc = getDirection();
  }
  else if (message.command == "SBK"){
    backlash = message.long_input;
    Serial.println("SBK");
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
    stepper.move(60);
    direc = CCW;
  }
  else if (buttonsEnabled && leftButton.isOn()) {
    //Serial.println("-1");
    stepper.move(-60);
    direc = CW;
  }
  else if (homeButton.isOn()) {
    //Serial.println("Home button pressed");
    gohome();
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


Message parseData() {      // Separar los datos y armar el mensaje
  Message newMessage;
  char received[numChars] = {0};

  char * strtokIndx; //lo usa strtok() como índice

  strtokIndx = strtok(tempChars, ",");     // toma hasta la primer parte, el string
  strcpy(received, strtokIndx); // y lo copia en received
  newMessage.command = String(received); //lo asignamos al miembro command del mensaje

  strtokIndx = strtok(NULL, ","); // idem pero con el int
  newMessage.integer_input = atoi(strtokIndx);

  strtokIndx = strtok(NULL, ",");
  newMessage.long_input = atol(strtokIndx);  //idem pero con el long

  return newMessage;
}

String getInfo() {
  const String string1 = "Auto State: ",
               string2 = "Buttons State ",
               string3 = "BAUD rate: ",
               string4 = "Recorrido: ",
               string5 = "Start, Finish: ",
               string6 = "Vel. ida, Vel. vuelta: ",
               espacio = " ",
               endl = "\n";

  String info = string1 + state + endl
                + string2 + buttonsEnabled + endl
                + string3 + baudRate + endl
                + string4 + endl
                + string5 + recorrido.start + espacio + recorrido.finish + endl
                + string6 + recorrido.speed_ida + espacio + recorrido.speed_vuelta + endl;
  return info;
}

void blinkLeds(const int num_blinks, const long interval) {
  long time_ = millis();
  int blinks = 0;
  while (blinks < num_blinks) {
    while ((millis() - time_) < interval) {
      digitalWrite(redPin, HIGH);
      digitalWrite(greenPin, HIGH);
    }
    time_ = millis();
    while ((millis() - time_) < interval) {
      digitalWrite(redPin, LOW);
      digitalWrite(greenPin, LOW);
    }
    time_ = millis();
    blinks++;
  }
}


bool getDirection() {
  return (stepper.distanceToGo() > 0); //true-> CW, false-> CCW //Si frenó es 0, que es CCW ----> lo use solo en casos donde no estaria frenado
}

void updateDirection() {
  static bool old_direc;  //se inicializa a 0 q es igual a CW

  if (direc != old_direc) {
    Serial.println("Direction has changed!");
    directionHasChanged = true;
    old_direc = direc;
  }
  else {
    directionHasChanged = false;
  }
}

void adjustBacklash() {
  if (directionHasChanged) {
    long target = stepper.targetPosition();
    long current = stepper.currentPosition();
    if (direc == CW) {
      stepper.move(backlash); //movete -backlash- pasos positivos/CW
    }
    else if (direc == CCW) {
      stepper.move(-backlash); //movete -backlash- pasos negativos/CCW
    }
    stepper.runToPosition();
    stepper.setCurrentPosition(current);
    stepper.moveTo(target);
  }
}
