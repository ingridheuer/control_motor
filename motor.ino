/* Automatización de motor V1
   Ingrid Heuer
   Julio 2021
   Labo 7

   Automatización y control del electroimán del laboratorio de bajas temperaturas
*/

//Incluyo librerías y archivos de clases
#include <AccelStepper.h> //librería para control de steppers
#include <digitalPinFast.h> //librería para mejorar la velocidad de pines digitales
#include <PushButton.h> //clase que arme controlar los botones (ver código en Arduino/libraries/PushButton)

//Defino los pines como constantes
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

//Creo objetos: motor, botones y leds
AccelStepper stepper(1, pulsePin, dirPin); // 1 = driver

//Elegí un delay de 50ms para todos los botones/switches, pero se pueden configurar por separado si hace falta
const long debounceDelay = 50;
PushButton rightButton(rightPin, false, debounceDelay);
PushButton leftButton(leftPin, false, debounceDelay);
PushButton stopButton(stopPin, true, debounceDelay);
PushButton homeButton(homePin, true, debounceDelay);
PushButton homeMicroswitch(homeSwitch, false, debounceDelay);
PushButton endMicroswitch(endSwitch, false, debounceDelay);

//================================FAST
digitalPinFast greenLed(greenPin);
digitalPinFast redLed(redPin);

//==========================================================
// Variables globales (son accesibles para todo el programa)
//==========================================================
//volatile bool buttonsEnabled = true;  //volatile porque los interrupts la pueden modificar
volatile bool rightButtonEnabled = true;
volatile bool leftButtonEnabled = true;
volatile bool interrupted = false;

long baudRate = 115200;

unsigned long startTime;
unsigned long endTime;
unsigned long execTime;
bool Print = false;
bool Print2 = false;
bool homing = false;

//=====================================
// Variables para movimiento automático
//=====================================

//Estados que puede tomar el programa cuando está en modo automático
enum states
{
  autoDisabled, goingToStart, waitingStart, goingToFinish, waitingFinish, canceled
};
volatile states state = autoDisabled;   //volatile porque los interrupts la pueden modificar

typedef struct {
  long start;
  long finish;
  int total_loops;
  int speed_ida;
  int speed_vuelta;
} Recorrido;

Recorrido recorrido;
const long wait = 1000;

//===================================
enum states2
{
  stepping, waiting, stepDisabled
};
volatile states2 state2 = stepDisabled;

long total_lenght = 0;
long step_lenght = 330;

//=====================================
//Variables para calibración y backlash
//=====================================
long backlash = 0;

bool directionHasChanged = false;
bool skipNext = false;

enum direccion {IZQ, DER, DET};
direccion direc = DER;

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
    if (!homing && (state != waitingStart)) {
      stepper.stop();
      state2 = stepDisabled;
    }
    //================================FAST
    redLed.digitalWriteFast(HIGH);
    //digitalWrite(redPin,HIGH);
    leftButtonEnabled = false;
    interrupted = true;
  }
  else {
    redLed.digitalWriteFast(LOW);
    //digitalWrite(redPin,LOW);
    leftButtonEnabled = true;
    interrupted = false;
  }
}


void ISRend() {
  if (!digitalRead(endSwitch)) {
    stepper.stop();
    state = canceled;
    state2 = stepDisabled;
    redLed.digitalWriteFast(HIGH);
    //digitalWrite(redPin,HIGH);
    rightButtonEnabled = false;
    interrupted = true;
  }
  else {
    redLed.digitalWriteFast(LOW);
    //digitalWrite(redPin,LOW);
    rightButtonEnabled = true;
    interrupted = false;
  }
}
//==============================================================

void setup() {
  Serial.begin(baudRate);

  stepper.setMaxSpeed(400);  //Con la reduccion de 1:60 y el microswitch de 400 esto es 1 RPM en la plataforma
  stepper.setAcceleration(150);  //Esto hay que calibrar y elegirlo

  greenLed.pinModeFast(OUTPUT);
  redLed.pinModeFast(OUTPUT);
  //pinMode(redPin,OUTPUT);
  //pinMode(greenPin,OUTPUT);

  homeButton.setPin();
  stopButton.setPin();
  leftButton.setPin();
  rightButton.setPin();
  homeMicroswitch.setPin();
  endMicroswitch.setPin();

  attachInterrupt(digitalPinToInterrupt(homeSwitch), ISRhome, CHANGE);
  attachInterrupt(digitalPinToInterrupt(endSwitch), ISRend, CHANGE);

  blinkLeds(3, 300);

}

//============================================================
void loop() {

  //startTime = micros();
  readserial();
  if (newData == true) {
    strcpy(tempChars, receivedChars);
    Message newMessage = parseData();
    newData = false;
    serialCommands(newMessage);
  }

  buttonCommands();
  automed();
  autostep();

  updateDirection();
  adjustBacklash();

  stepper.run();

  static bool ledOn;
  if (stepper.isRunning() && !ledOn) {
    greenLed.digitalWriteFast(HIGH);
    //digitalWrite(greenPin,HIGH);
    ledOn = true;
  }
  else {
    if (ledOn) {
      greenLed.digitalWriteFast(LOW);
      //digitalWrite(greenPin,LOW);
      ledOn = false;
    }
    if ((!interrupted) && (state == autoDisabled)) {
      rightButtonEnabled = true;
      leftButtonEnabled = true;
    }
  }

  /*
    endTime = micros();
    execTime = endTime - startTime;
    if (Print) {
      Serial.print(execTime);
      if (Print2) {
        Serial.print("\t");
        Serial.print(stepper.currentPosition());
      }
      Serial.println(",0,1000");
    }
  */
}
//=================================================================================

//=========================
// Funciones de movimiento
//=========================

void gohome() {
  homing = true;
  //stepper.setAcceleration(400);
  //digitalWrite(greenPin,HIGH);
  greenLed.digitalWriteFast(HIGH);
  //Serial.println("Start homing");

  while (!homeMicroswitch.isOn()) {
    stepper.move(-133); //probar con distintos valores aca
    stepper.run();
  }

  //Serial.println("Step back");
  stepper.setCurrentPosition(0); //para medir el backlash

  while (homeMicroswitch.isOn()) {
    stepper.move(20); //y aca
    stepper.run();
  }

  backlash = stepper.currentPosition();
  stepper.setCurrentPosition(0);
  greenLed.digitalWriteFast(LOW);
  //digitalWrite(greenPin,LOW);

  //Para que no interfiera con la correccion de backlash
  if (direc == IZQ) {
    skipNext = true;
  }
  direc = DER;
  //stepper.setAcceleration(200);
  homing = false;
}

void automed() {

  //se inicializan a 0 por default
  static unsigned long startMillis;
  static unsigned long stopMillis;
  static int current_loop;

  switch (state) {

    case autoDisabled:
      break;

    case canceled:
      current_loop = 0;
      rightButtonEnabled = true;
      leftButtonEnabled = true;
      state = autoDisabled;
      break;

    case goingToStart:
      if (stepper.distanceToGo() == 0) {
        startMillis = millis();
        current_loop++;
        if (current_loop > recorrido.total_loops) {
          //Serial.println("Termine las vueltas, cambio a disabled y reseteo loops");
          state = autoDisabled;
          current_loop = 0;
          rightButtonEnabled = true;
          leftButtonEnabled = true;
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
        direc = getDirection();
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
    state = canceled;
    state2 = stepDisabled;
    //rightButtonEnabled = false;
    //leftButtonEnabled = false;
  }
  else if (message.command == "SET") {
    Serial.println("SET");
    rightButtonEnabled = false;
    leftButtonEnabled = false;
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
    rightButtonEnabled = false;
    leftButtonEnabled = false;
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
    Serial.println("Ingrid Heuer, Agosto 2021. Ver tabla de comandos en /escritorio/ingridmarco/control_motor/comandos");
  }
  else if (message.command == "PRT") {
    Print = true;
  }
  else if (message.command == "PRF") {
    Print = false;
  }
  else if (message.command == "P2T") {
    Print2 = true;
  }
  else if (message.command == "P2F") {
    Print2 = false;
  }
  else if (message.command == "DIR") {
    Serial.println(direc);
  }
  else if (message.command == "BLK") {  //en lo posible no usar esto porque bloquea, es preferible usar REL
    Serial.println("BLK");
    stepper.setMaxSpeed(message.integer_input);
    stepper.move(message.long_input);
    direc = getDirection();
    stepper.runToPosition();
  }
  else if (message.command == "SBK") {
    backlash = message.long_input;
    Serial.println("SBK");
  }
  else if (message.command == "REL") {
    Serial.println("REL");
    stepper.setMaxSpeed(message.integer_input);
    stepper.move(message.long_input);
    direc = getDirection();
  }
  else if (message.command == "ANG") {
    const float microstep = 400;
    const float reduccion = 60;
    const float conv = 360 / (microstep * reduccion);
    float angulo = message.integer_input / conv;
    Serial.println(angulo);
  }
  else if (message.command == "STE") {
    Serial.println("STE");
    step_lenght = message.integer_input;
    total_lenght = message.long_input;
    stepper.move(step_lenght);
    direc = getDirection();
    state2 = stepping;
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
    state = canceled;
    state2 = stepDisabled;
    //rightButtonEnabled = false;
    //leftButtonEnabled = false;
  }
  else if (rightButtonEnabled && rightButton.isOn()) {
    //Serial.println("+1");
    stepper.move(133);
    direc = getDirection();
  }
  else if (leftButtonEnabled && leftButton.isOn()) {
    //Serial.println("-1");
    stepper.move(-133);
    direc = getDirection();
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

//==============================
// Misc
//==============================

String getInfo() {
  const String string1 = "Step state: ",
               string2 = "Buttons State ",
               string3 = "BAUD rate: ",
               string4 = "Recorrido: ",
               string5 = "Start, Finish: ",
               string6 = "Vel. ida, Vel. vuelta: ",
               espacio = " ",
               endl = "\n";

  String info = string1 + state2 + endl
                + string2 + rightButtonEnabled + leftButtonEnabled + endl
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
      redLed.digitalWriteFast(HIGH);
      greenLed.digitalWriteFast(HIGH);
      //digitalWrite(greenPin,HIGH);
      //digitalWrite(redPin,HIGH);
    }
    time_ = millis();
    while ((millis() - time_) < interval) {
      redLed.digitalWriteFast(LOW);
      greenLed.digitalWriteFast(LOW);
      //digitalWrite(greenPin,LOW);
      //digitalWrite(redPin,LOW);
    }
    time_ = millis();
    blinks++;
  }
}

//Para saber la direccion del proximo movimiento
//Hay que correrla si y solo si se cambia el target
direccion getDirection() {
  if (stepper.distanceToGo() > 0) {
    return DER;
  }
  else if (stepper.distanceToGo() < 0) {
    return IZQ;
  }
  else if (stepper.distanceToGo() == 0) {
    return DET;
  }
}

void updateDirection() {
  static direccion old_direc = DER;  //la primer correción no se va a hacer porque no tiene calibrado el backlash asi que no importa esto

  if ((direc != old_direc) && (direc != DET)) { //que no considere detenido como un cambio de dirección
    //Serial.println("Direction has changed!");
    directionHasChanged = true;
    old_direc = direc;
  }
  else {
    directionHasChanged = false;
  }
}

void adjustBacklash() {
  if (directionHasChanged) {
    if (skipNext) {
      skipNext = false;
    }
    else {
      //Serial.println("Adjusting backlash");
      long target = stepper.targetPosition();
      long current = stepper.currentPosition();
      if (direc == DER) {
        stepper.move(backlash); //movete -backlash- pasos positivos/derecha
      }
      else if (direc == IZQ) {
        stepper.move(-backlash); //movete -backlash- pasos negativos/izquierda
      }
      stepper.runToPosition();
      stepper.setCurrentPosition(current);
      stepper.moveTo(target);
      //Serial.println("Done");
    }
  }
}

void autostep() {

  //se inicializan a 0 por default
  static unsigned long startMillis;
  //static unsigned long stopMillis;
  static long current_step;

  switch (state2) {

    case stepDisabled:
      break;

    case stepping:
      if (stepper.distanceToGo() == 0) {
        startMillis = millis();
        current_step += step_lenght;
        if (current_step > total_lenght) {
          //Serial.println("Termine las vueltas, cambio a disabled y reseteo loops");
          state2 = stepDisabled;
          current_step = 0;
          //rightButtonEnabled = true;
          //leftButtonEnabled = true;
        }
        else {
          //Serial.println("Cambio a waiting start");
          state2 = waiting;
        }
      }
      break;

    case waiting:
      if (millis() - startMillis > wait) {
        //Serial.println("Cambio a going to finish");
        stepper.move(step_lenght);
        direc = getDirection();
        //direc = getDirection();
        //stepper.setMaxSpeed(recorrido.speed_ida);
        state2 = stepping;
      }
      break;
  }
}
