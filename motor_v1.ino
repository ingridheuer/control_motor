
/* Automatización de motor V1
   Ingrid Heuer
   Julio 2021
   Labo 7

   Automatización y control del electroimán del laboratorio de bajas temperaturas
*/


/*
 * La ultima vez cambiaste bocha de cosas: pasaste todas las funciones con comand a un if else grande (es mejor)
 * y hiciste una clase para los botones
 * Ahora tenes que: 
 * Todo lo que sea compartido entre serial y botones deberia ser una función aparte que podemosllamar desde los dos
 * Algunas funciones van a necesitar inputs y otras outputs para que todo esto funcione
 */

#include <AccelStepper.h> //librería para control de steppers
#include <PushButton.h> //mini lib que me arme para los botones

//Configuración general

//Nota: homeSwitch y endSwitch tienen que ser pines 2 y 3 si o si,
//porque son los únicos disponibles para interrupciones
const byte homeSwitch = 2,
           //endSwitch = 3,
           dir = 4,
           pulse = 5,
           greenPin = 10,
           redPin = 11;

//Creamos el objeto stepper de la clase AccelStepper
AccelStepper stepper(1, dir, pulse); // 1 -> driver de 2 cables

//Creamos los objetos de la clase PushButton

PushButton rightButton(6, false, 50);
PushButton leftButton(7, false, 50);
PushButton stopButton(8, true, 50);
PushButton homeButton(9, true, 50);

//=====================================
// Variables para movimiento automático
//=====================================

//Definimos los estados que puede tomar el programa cuando está en modo automático
enum states
{
  autoDisabled, goingToStart, waitingAtStart, goingToFinish, waitingAtFinish
};
states state = autoDisabled;

long Start; //donde empieza el recorrido
long Finish; //donde termina
int total_loops; //las que tiene que dar
int speed_ida;
int speed_vuelta;
long backlash; //el "juego" del motor al cambiar de dirección

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
    digitalWrite(redPin, HIGH);
  }
  else {
    digitalWrite(redPin, LOW);
  }
  }
*/
//======================

void setup() {
  Serial.begin(9600);

  stepper.setMaxSpeed(500);
  stepper.setAcceleration(200);

  pinMode(homeSwitch, INPUT_PULLUP);
  //pinMode(endSwitch, INPUT_PULLUP);
  pinMode(greenPin, OUTPUT);
  pinMode(redPin, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(homeSwitch), ISRhome, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(endSwitch), ISRend, CHANGE);
}


void loop() {

  //===== se ejecutan solo si le hablamos por serial
  readserial();
  if (newData == true) {
    strcpy(tempChars, receivedChars); //hacemos una copia temporal de lo que recibimos en tempChars para proteger los datos originales
    Message newMessage = parseData();
    newData = false;
    serialCommands(newMessage);
  }

  //============ se ejecuta en cada loop

  buttonCommands();
  automed();
  stepper.run();

  if (stepper.isRunning()) {
    digitalWrite(greenPin, HIGH);
    rightButton.enabled = false;
    leftButton.enabled = false;
  }
  else {
    digitalWrite(greenPin, LOW);
    rightButton.enabled = true;
    leftButton.enabled = true;
  }
}

//=========================
// Funciones de movimiento
//=========================

void rehome() {
    Serial.println("Homing");
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
        if (current_loop > total_loops) {
          //Serial.println("Termine las vueltas, cambio a DISABLED");
          state = autoDisabled;
          current_loop = 0;
        }
        else {
          //Serial.println("Faltan vueltas, cambio a WAITING AT START");
          state = waitingAtStart;
        }
      }
      break;

    case waitingAtStart:
      if (millis() - startMillis > 1000) {
        //Serial.println("Cambio a TOEND");
        stepper.moveTo(Finish);
        stepper.setMaxSpeed(speed_ida);
        state = goingToFinish;
      }
      break;

    case goingToFinish:
      if (stepper.distanceToGo() == 0) {
        //Serial.println("Cambio a WAITING_STOP");
        stopMillis = millis();
        state = waitingAtFinish;
      }
      break;

    case waitingAtFinish:
      if (millis() - stopMillis > 1000) {
        //Serial.println("Cambio a TOSTART");
        stepper.moveTo(Start);
        stepper.setMaxSpeed(speed_vuelta);
        state = goingToStart;
      }
      break;
  }
}

//====================================
// Funciones para comandos de LabVIEW
//====================================


void serialCommands(Message message) {
  if (message.command == "measure") {
    Serial.println(stepper.currentPosition());
  }
  else if (message.command == "stop") {
    Serial.println("Stopping");
    stepper.stop();
    state = autoDisabled;
  }
  else if (message.command == "setpoint") {
    Serial.print("Setpoint: ");
    Serial.println(message.long_input);
    stepper.setMaxSpeed(message.integer_input);
    stepper.moveTo(message.long_input);
  }
  else if (message.command == "setaccel") {
    Serial.print("Acccel set: ");
    Serial.println(message.integer_input);
    stepper.setAcceleration(message.integer_input);
  }
  else if (message.command == "vuelta") {
    Serial.print("Start set at: ");
    Serial.println(Start);
    Start = message.long_input;
    speed_vuelta = message.integer_input;
  }
  else if (message.command == "ida") {
    Serial.print("Finish set at: ");
    Serial.println(Finish);
    Finish = message.long_input;
    speed_ida = message.integer_input;
  }
  else if (message.command == "automed") {
    Serial.println("Automed initialized");
    total_loops = message.integer_input;
    stepper.moveTo(Start);
    state = goingToStart;
  }
  else if (message.command == "state") {
    Serial.println(state);
  }
  else if (message.command == "backlash") {
    Serial.println(backlash);
  }
  else if (message.command = "rehome"){
    rehome();
  }
  else {
    Serial.println("Invalid command");
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
  }

  if (homeButton.isOn()) {
    //Serial.println("Home button pressed");
    rehome();
  }

  if (rightButton.enabled && rightButton.isOn()) {
    //Serial.println("+1");
    stepper.move(1);
  }

  if (leftButton.enabled && leftButton.isOn()) {
    //Serial.println("-1");
    stepper.move(-1);
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
