
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
 * Las variables de comando e inputs no necesitan ser globales! Van a ser locales y usadas solo si entra un comando
 * Entonces, las funciones llamadas por comandos no tienen que tener el "if .... do", sino que las vamos a llamar desde el if else
 * grande de serialCommands
 * Todo lo que sea compartido entre serial y botones deberia ser una función aparte que podemosllamar desde los dos
 * Algunas funciones van a necesitar inputs y otras outputs para que todo esto funcione
 * Quizas hayaq que agregar algun static
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
unsigned long debounceDelay = 50;

PushButton rightButton(6, false, debounceDelay);
PushButton leftButton(7, false, debounceDelay);
PushButton stopButton(8, true, debounceDelay);
PushButton homeButton(9, true, debounceDelay);

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
int current_loop; //vueltas que dio hasta ahora //podría ser static
int total_loops; //las que tiene que dar
int speed_ida;
int speed_vuelta;
long backlash; //el "juego" del motor al cambiar de dirección

// Millis = tiempo en milisegundos desde que arranca el programa
// Lo usamos para que el motor espere al final del recorrido antes de cambiar de dirección
// Nota: unsigned long porque son valores grandes, siempre positivos y si hubiera overflow queremos que
// se resetee a 0

unsigned long startMillis; //tiempo al principío del recorrido
unsigned long stopMillis; //tiempo al final

//===========================
// Variables para leer Serial
//===========================

const byte numChars = 32;
char receivedChars[numChars];
char tempChars[numChars];

// Variables para guardar el comando
char message[numChars] = {0};
int integer_input = 0;
long long_input = 0;
String command;

// Si entraron datos por serial o no
bool newData = false;

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
    parseData();
    newData = false;
    serialCommands(command, integer_input, long_input);
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
  if ((command == "rehome")) {
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
}

void automed() {

  switch (state) {

    case autoDisabled:
      break;

    case goingToStart:
      if (stepper.distanceToGo() == 0) {
        startMillis = millis();
        current_loop++;
        if (current_loop > total_loops) {
          Serial.println("Termine las vueltas, cambio a DISABLED");
          state = autoDisabled;
        }
        else {
          Serial.println("Faltan vueltas, cambio a WAITING AT START");
          state = waitingAtStart;
        }
      }
      break;

    case waitingAtStart:
      if (millis() - startMillis > 1000) {
        Serial.println("Cambio a TOEND");
        stepper.moveTo(Finish);
        stepper.setMaxSpeed(speed_ida);
        state = goingToFinish;
      }
      break;

    case goingToFinish:
      if (stepper.distanceToGo() == 0) {
        Serial.println("Cambio a WAITING_STOP");
        stopMillis = millis();
        state = waitingAtFinish;
      }
      break;

    case waitingAtFinish:
      if (millis() - stopMillis > 1000) {
        Serial.println("Cambio a TOSTART");
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


void serialCommands(String command, int integer_input, long long_input) {
  if (command == "measure") {
    Serial.println(stepper.currentPosition());
  }
  else if (command == "stop") {
    Serial.println("Stopping");
    stepper.stop();
    state = autoDisabled;
  }
  else if (command == "setpoint") {
    Serial.print("Setpoint: ");
    Serial.println(long_input);
    stepper.moveTo(long_input);
  }
  else if (command == "setspeed") {
    Serial.print("Speed set: ");
    Serial.println(integer_input);
    stepper.setMaxSpeed(integer_input);
  }
  else if (command == "setaccel") {
    Serial.print("Acccel set: ");
    Serial.println(integer_input);
    stepper.setAcceleration(integer_input);
  }
  else if (command == "setstart") {
    Serial.print("Start set at: ");
    Serial.println(Start);
    Start = long_input;
  }
  else if (command == "setfinish") {
    Serial.print("Finish set at: ");
    Serial.println(Finish);
    Finish = long_input;
  }
  else if (command == "automed") {
    Serial.println("Automed initialized");
    total_loops = integer_input;
    current_loop = 0;
    stepper.moveTo(Start);
    state = goingToStart;
  }
  else if (command == "state") {
    Serial.println(state);
  }
  else if (command == "backlash") {
    Serial.println(backlash);
  }
  else if (command == "speed_ida") {
    speed_ida = integer_input;
    Serial.print("Speed ida set at: ");
    Serial.println(speed_ida);
  }
  else if (command == "speed_vuelta") {
    speed_vuelta = integer_input;
    Serial.print("Speed vuelta set at: ");
    Serial.println(speed_vuelta);
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
    Serial.println("Stop button pressed");
    stepper.stop();
  }

  if (homeButton.isOn()) {
    Serial.println("Home button pressed");
    rehome();
  }

  if (rightButton.enabled && rightButton.isOn()) {
    Serial.println("+1");
    stepper.move(1);
  }

  if (leftButton.enabled && leftButton.isOn()) {
    Serial.println("-1");
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


void parseData() {      // split the data into its parts

  char * strtokIndx; // this is used by strtok() as an index

  strtokIndx = strtok(tempChars, ",");     // get the first part - the string
  strcpy(message, strtokIndx); // copy it to message
  command = String(message);

  strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
  integer_input = atoi(strtokIndx);     // convertimos a un int

  strtokIndx = strtok(NULL, ",");
  long_input = atol(strtokIndx);  //convertimos a un long

}
