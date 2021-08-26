/* Automatización de motor V1
   Ingrid Heuer, Marco Crivaro Nicolini
   Julio 2021
   Labo 7

   Automatización y control del electroimán del laboratorio de bajas temperaturas
*/

#include <AccelStepper.h> //librería para control de steppers

//Defino los pins que voy a usar y variables de posición
//Nota: home_switch y end_switch tienen que ser pines 2 y 3 si o si,
//porque son los únicos disponibles para interrupciones
const byte home_switch = 2,
           //end_switch = 3,
           dir = 4,
           pulse = 5,
           greenpin = 10,
           redpin = 11;

long backlash; //el "juego" del motor al cambiar de dirección

typedef struct {
  const byte pin;
  int currentState;
  unsigned long lastDebounceTime;
} pushButton;

pushButton rightButton = {6, HIGH, 0};
pushButton leftButton = {7, HIGH, 0};
pushButton homeButton = {8, HIGH, 0};
pushButton stopButton = {9, HIGH, 0};

long debounceDelay = 1000;


//Creamos el objeto stepper
AccelStepper stepper(1, dir, pulse); // 1 -> driver de 2 cables

//=====================================
// Variables para movimiento automático
//=====================================

//Definimos los estados que puede tomar el programa cuando está en modo automático
enum states
{
  DISABLED, INIT, WAITING_START, TOSTART, TOEND, WAITING_STOP
};

//Creamos una variable estado, que solo puede tomar los valores de "estados"
//La inicializamos como DISABLED
states state = DISABLED;

long Start; //donde empieza el recorrido
long Stop; //donde termina
int current_loops; //vueltas que dio hasta ahora
int total_loops; //las que tiene que dar
int speed_ida;
int speed_vuelta;

// Millis = tiempo en milisegundos desde que arranca el programa
// Lo usamos para que el motor espere al final del recorrido antes de cambiar de dirección
// Nota: unsigned long porque son valores grandes, siempre positivos y si hubiera overflow queremos que
// se resetee a 0
unsigned long currentMillis; //tiempo actual
unsigned long startMillis; //tiempo al principío del recorrido
unsigned long stopMillis; //tiempo al final
const int wait = 1000;  //tiempo que tiene que esperar

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
  if (!digitalRead(home_switch)) {
    stepper.stop();
    state = DISABLED;
    digitalWrite(redpin, HIGH);
  }
  else {
    digitalWrite(redpin, LOW);
  }
}

/*
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
*/
//======================

void setup() {
  Serial.begin(9600);

  stepper.setMaxSpeed(500);
  stepper.setAcceleration(200);

  pinMode(home_switch, INPUT_PULLUP);
  //pinMode(end_switch, INPUT_PULLUP);

  pinMode(rightButton.pin, INPUT_PULLUP);
  pinMode(leftButton.pin, INPUT_PULLUP);
  pinMode(stopButton.pin, INPUT_PULLUP);
  pinMode(homeButton.pin, INPUT_PULLUP);

  pinMode(greenpin, OUTPUT);
  pinMode(redpin, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(home_switch), ISRhome, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(end_switch), ISRend, CHANGE);
}


void loop() {

  currentMillis = millis();
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
    if (command == "speed_vuelta") {
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

    if (command == "stop") {
      Serial.println("Stopping");
      stepper.stop();
      state = DISABLED;
    }

    if (command == "state") {
      Serial.println(state);
    }
  }

  //============ se ejecuta en cada loop

  /*
    test(rightButton);
    test(leftButton);
    test(homeButton);
    test(stopButton);
  */

  /*
    buttonStateDebounce(rightButton);
    buttonStateDebounce(leftButton);
    buttonStateDebounce(homeButton);
    buttonStateDebounce(stopButton);
  */

  rightButton.currentState = buttonStateDebounce(rightButton);
  leftButton.currentState = buttonStateDebounce(leftButton);
  homeButton.currentState = buttonStateDebounce(homeButton);
  stopButton.currentState = buttonStateDebounce(stopButton);


  buttonCommands();
  automed();
  stepper.run();

  if (stepper.isRunning()) {
    digitalWrite(greenpin, HIGH);
  }
  else
  {
    digitalWrite(greenpin, LOW);
  }

}

//=========================
// Funciones de movimiento
//=========================

void rehome() {
  if ((command == "rehome")) {
    Serial.println("Homing");
    digitalWrite(greenpin, HIGH);

    while (digitalRead(home_switch)) {
      stepper.move(-1);
      stepper.run();
    }

    stepper.setCurrentPosition(0); //para medir el backlash

    while (!digitalRead(home_switch)) {
      stepper.move(1);
      stepper.run();
    }

    backlash = stepper.currentPosition(); //cuantos pasos dió desde que pisó el switch hasta que lo soltó
    stepper.setCurrentPosition(0);
    digitalWrite(greenpin, LOW);
  }
}

void automed() {
  switch (state) {

    case DISABLED: //default
      break;

    case INIT:
      if (stepper.distanceToGo() == 0) {
        Serial.println("Cambio a WAITING_START");
        startMillis = currentMillis;
        state = WAITING_START;
      }
      break;

    case WAITING_START:
      if (currentMillis - startMillis >= wait) {
        Serial.println("Cambio a TOEND");
        stepper.moveTo(Stop);
        stepper.setMaxSpeed(speed_ida);
        state = TOEND;
      }
      break;

    case TOEND:
      if (stepper.distanceToGo() == 0) {
        Serial.println("Cambio a WAITING_STOP");
        stopMillis = currentMillis;
        state = WAITING_STOP;
      }
      break;

    case WAITING_STOP:
      if (currentMillis - stopMillis >= wait) {
        Serial.println("Cambio a TOSTART");
        stepper.moveTo(Start);
        stepper.setMaxSpeed(speed_vuelta);
        state = TOSTART;
      }
      break;

    case TOSTART: //si terminó suma una vuelta al contador. si dio todas las vueltas pasa a DISABLED, sino vuelve a empezar
      if (stepper.distanceToGo() == 0) {
        current_loops++;
        if (current_loops == total_loops) {
          Serial.println("Termine las vueltas, cambio a DISABLED");
          state = DISABLED;
        }
        else {
          Serial.println("Faltan vueltas, cambio a INIT");
          state = INIT;
        }
      }
      break;
  }
}

//====================================
// Funciones para comandos de LabVIEW
//====================================


void measure() {
  if (command == "measure") {
    Serial.println(stepper.currentPosition());
  }
}

void setpoint() {
  if (command == "setpoint") {
    stepper.moveTo(long_input);
    Serial.print("Setpoint: ");
    Serial.println(long_input);
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
    Start = long_input;
    Serial.print("Start set at: ");
    Serial.println(Start);
  }
}

void set_stop() {
  if (command == "setstop") {
    Stop = long_input;
    Serial.print("Stop set at: ");
    Serial.println(Stop);
  }
}

//=============
// Botones
//=============

int buttonStateDebounce(pushButton button) {
  int reading = digitalRead(button.pin);

  if ((reading != button.currentState) && (millis() - button.lastDebounceTime >= debounceDelay)) {
    button.lastDebounceTime = millis();
    button.currentState = reading;

    if (reading == LOW) {
      Serial.println("Pressed");
    }
    if (reading == HIGH) {
      Serial.println("Released");
    }
  }
  return button.currentState;
}



void buttonCommands() {
  if (stopButton.toggled) {
    Serial.println("Stop button pressed");
    stepper.stop();
    state = DISABLED;
    //stopButton.toggled = 0;
  }
  if (homeButton.toggled) {
    Serial.println("Home button pressed");
    rehome();
    //homeButton.toggled = 0;
  }
  if (!rightButton.currentState) {
    Serial.println("+1");
    stepper.move(1);
  }
  if (!leftButton.currentState) {
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
