/* Automatización de motor V1
   Ingrid Heuer
   Julio 2021
   Labo 7

   Automatización y control del electroimán del laboratorio de bajas temperaturas
*/

//Incluyo librerías y archivos de clases
#include <AccelStepper.h> //librería para control de steppers
#include <digitalPinFast.h> //librería para mejorar la velocidad de pines digitales
#include <PushButton.h> //clase que armamos controlar los botones

//Defino los pines como constantes que ocupan 1 byte de memoria
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

//Creo objetos: motor, botones y leds. El primer argumento indica que estamos usando un driver para controlar el motor.
AccelStepper stepper(1, pulsePin, dirPin);

//Elegí un delay de 50ms para filtrar el ruido de los botones y microswitches
const long debounceDelay = 50;

//Inicializamos los objetos pulsadores (ver documentacion de clase PushButton)
PushButton rightButton(rightPin, false, debounceDelay);
PushButton leftButton(leftPin, false, debounceDelay);
PushButton stopButton(stopPin, true, debounceDelay);
PushButton homeButton(homePin, true, debounceDelay);
PushButton homeMicroswitch(homeSwitch, false, debounceDelay);
PushButton endMicroswitch(endSwitch, false, debounceDelay);

//Inicializamos los objetos de pin rápido
digitalPinFast greenLed(greenPin);
digitalPinFast redLed(redPin);

//==================
// Defino variables
//==================

//Algunas flags para habilitar y deshabilitar pulsadores o indicar estados del programa
volatile bool rightButtonEnabled = true; //volatile porque los interrupts la pueden modificar
volatile bool leftButtonEnabled = true;
volatile bool interrupted = false;
bool homing = false;

//La tasa de comunicación por USB
long baudRate = 115200;

//=====================================
// Variables para movimiento automático
//=====================================

//Estados que puede tomar el programa cuando está en modo automático
enum states
{
  autoDisabled, goingToStart, waitingStart, goingToFinish, waitingFinish, canceled
};
volatile states state = autoDisabled;   //volatile porque los interrupts la pueden modificar

//Armamos un tipo de variable Recorrido para contener todos los parámetros del recorrido automático
typedef struct {
  long start;
  long finish;
  int total_loops;
  int speed_ida;
  int speed_vuelta;
} Recorrido;

Recorrido recorrido;
const long wait = 1000; //el tiempo de espera en cada punta

//===================================
//Estados que puede tomar el programa cuando está en modo automático por pasos
enum states2
{
  stepping, waiting, stepDisabled
};
volatile states2 state2 = stepDisabled;

long total_lenght = 0;
long step_lenght = 330;

//==========================
//Variables para calibración
//==========================
//Variable para guardar el juego angular
long backlash = 0;

//Variables para registrar la dirección
bool directionHasChanged = false;
bool skipNext = false;

enum direccion {IZQ, DER, DET};
direccion direc = DER;

//===========================
// Variables para leer Serial
//===========================

// Si entraron datos por serial o no
bool newData = false;

//Acá se van a ir guardando los datos a medida que los recibe y antes de interpretarlos
const byte numChars = 32;
char receivedChars[numChars];
char tempChars[numChars];

//Armamos un tipo de variable Message para contener todos los parámetros del mensaje una vez que lo interpreta
typedef struct {
  String command;
  int integer_input;
  long long_input;
} Message;

//=========================
// Rutinas de interrupción
//=========================

//Definimos las rutinas de servicio de interrupción que se van a ejecutar cuando se activan los microswitches

//Microswitch de home
void ISRhome() {
  if (!digitalRead(homeSwitch)) {
    if (!homing && (state != waitingStart)) {
      stepper.stop();
      state2 = stepDisabled;
    }
    redLed.digitalWriteFast(HIGH);
    leftButtonEnabled = false;
    interrupted = true;
  }
  else {
    redLed.digitalWriteFast(LOW);
    leftButtonEnabled = true;
    interrupted = false;
  }
}

//Microswitch de fin de recorrido
void ISRend() {
  if (!digitalRead(endSwitch)) {
    stepper.stop();
    state = canceled;
    state2 = stepDisabled;
    redLed.digitalWriteFast(HIGH);
    rightButtonEnabled = false;
    interrupted = true;
  }
  else {
    redLed.digitalWriteFast(LOW);
    rightButtonEnabled = true;
    interrupted = false;
  }
}

//=============
// Setup y loop
//=============

void setup() {
  Serial.begin(baudRate); //abrimos el puerto USB

  stepper.setMaxSpeed(400); //configuramos velocidad y aceleración maximas
  stepper.setAcceleration(150); 

  //configuramos todos los pines
  greenLed.pinModeFast(OUTPUT);
  redLed.pinModeFast(OUTPUT);

  homeButton.setPin();
  stopButton.setPin();
  leftButton.setPin();
  rightButton.setPin();
  homeMicroswitch.setPin();
  endMicroswitch.setPin();

  //agregamos rutinas de interrupción
  attachInterrupt(digitalPinToInterrupt(homeSwitch), ISRhome, CHANGE);
  attachInterrupt(digitalPinToInterrupt(endSwitch), ISRend, CHANGE);

  //indicamos encendido prendiendo los leds, 3 veces en un intervalo de 300 ms
  blinkLeds(3, 300);
}

void loop() {

  readserial(); //se fija si llegaron mensajes
  if (newData == true) {
    strcpy(tempChars, receivedChars);
    Message newMessage = parseData();   //interpreta los datos y los guarda en un mensaje
    newData = false;                    
    serialCommands(newMessage);         //ejecuta la función que corresponde al mensaje
  }

  buttonCommands();                     //lee estado de pulsadores y ejecuta funciones asociadas
  automed();                            //modos automáticos
  autostep();

  updateDirection();                    //actualiza la dirección
  adjustBacklash();                     //corrige backlash si es necesario

  stepper.run();                        //da un paso

  //esta parte del código maneja el led verde que se enciende si el motor está activo
  //y se encarga de desactivar o activar pulsadores si es necesario
  static bool ledOn;
  if (stepper.isRunning() && !ledOn) {
    greenLed.digitalWriteFast(HIGH);
    ledOn = true;
  }
  else {
    if (ledOn) {
      greenLed.digitalWriteFast(LOW);
      ledOn = false;
    }
    if ((!interrupted) && (state == autoDisabled)) {
      rightButtonEnabled = true;
      leftButtonEnabled = true;
    }
  }
}

//===========
// Funciones
//===========

//Funcion de homing automático
void gohome() {
  homing = true;
  greenLed.digitalWriteFast(HIGH);

  //avanza de a 133 pasos (se traducen en aprox. 2 grados en nuestro dispositivo) hasta activar el microswitch
  //ajustar este valor si hace falta
  while (!homeMicroswitch.isOn()) {
    stepper.move(-133);
    stepper.run();
  }
  
  //se activó, define esa posición como 0
  stepper.setCurrentPosition(0); 

  //retrocede de a 20 pasos hasta que desactiva el microswitch
  //ajustar este valor si hace falta
  while (homeMicroswitch.isOn()) {
    stepper.move(20);
    stepper.run();
  }

  //guardamos los pasos que dió como backlash y redefinimos la posición de referencia
  backlash = stepper.currentPosition();
  stepper.setCurrentPosition(0);
  greenLed.digitalWriteFast(LOW);

  //Para que no interfiera con la correccion de backlash
  if (direc == IZQ) {
    skipNext = true;
  }
  direc = DER;
  homing = false;
}

//Función de recorrido de ida y vuelta automático
//Ver diagrama de estados en sección control y automatización del informe (para el laboratorio)
void automed() {

  //Variables para registrar el tiempo y la cantidad de vueltas
  static unsigned long startMillis;
  static unsigned long stopMillis;
  static int current_loop;

  switch (state) {

    case autoDisabled:
      //si está deshabilitado no hace nada
      break;

    case canceled:
      //si se cancela resetea contadores y pulsadores antes de pasar a disabled
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
          state = autoDisabled;
          current_loop = 0;
          rightButtonEnabled = true;
          leftButtonEnabled = true;
        }
        else {
          state = waitingStart;
        }
      }
      break;

    case waitingStart:
      if (millis() - startMillis > wait) {
        stepper.moveTo(recorrido.finish);
        direc = getDirection();
        stepper.setMaxSpeed(recorrido.speed_ida);
        state = goingToFinish;
      }
      break;

    case goingToFinish:
      if (stepper.distanceToGo() == 0) {
        stopMillis = millis();
        state = waitingFinish;
      }
      break;

    case waitingFinish:
      if (millis() - stopMillis > wait) {
        stepper.moveTo(recorrido.start);
        direc = getDirection();
        stepper.setMaxSpeed(recorrido.speed_vuelta);
        state = goingToStart;
      }
      break;
  }
}

//Función de recorrido automático de a pasos
void autostep() {

  //variables para registrar el tiempo y los pasos dados
  static unsigned long startMillis;
  static long current_step;

  switch (state2) {

    case stepDisabled:
      break;

    case stepping:
      if (stepper.distanceToGo() == 0) {
        startMillis = millis();
        current_step += step_lenght;
        if (current_step > total_lenght) {
          state2 = stepDisabled;
          current_step = 0;
        }
        else {
          state2 = waiting;
        }
      }
      break;

    case waiting:
      if (millis() - startMillis > wait) {
        stepper.move(step_lenght);
        direc = getDirection();
        state2 = stepping;
      }
      break;
  }
}


//======================================
// Funcion para interpretar los comandos
//======================================

//Esta funcion toma la variable mensaje y evalúa el comando contra todos los comandos definidos para ver que función tiene que correr
//Siempre envía una respuesta para cumplir el protocolo de command-response
//La tabla de comandos y que significan esta en comandos.txt
//Para agregar comandos nuevos simplemente agregar otra condicion, respetando el formato message.command = "COM" codigo del comando,seguido de lo que queremos que ejecute.
//Para recibir parametros numericos usar message.long_input y message.integer_input. Cuidado con el tamaño de los numeros que guardan en cada variable.

void serialCommands(Message message) {
  if (message.command == "MES") {
    //Medir posicion
    Serial.println(stepper.currentPosition()); 
  }
  else if (message.command == "STP") {
    //Stop
    Serial.println("STP");
    stepper.stop();
    state = canceled;
    state2 = stepDisabled;
  }
  else if (message.command == "SET") {
    //Setpoint o posicion objetivo
    Serial.println("SET");
    rightButtonEnabled = false;
    leftButtonEnabled = false;
    stepper.setMaxSpeed(message.integer_input);
    stepper.moveTo(message.long_input);
    direc = getDirection();
  }
  else if (message.command == "ACC") {
    //Aceleracion
    Serial.println("ACC");
    stepper.setAcceleration(message.integer_input);
  }
  else if (message.command == "VUE") {
    //Posicion de partida y velocidad de vuelta
    Serial.println("VUE");
    recorrido.start = message.long_input;
    recorrido.speed_vuelta = message.integer_input;
  }
  else if (message.command == "IDA") {
    //Posicion de llegada y velocidad de ida
    Serial.println("IDA");
    recorrido.finish = message.long_input;
    recorrido.speed_ida = message.integer_input;
  }
  else if (message.command == "AUT") {
    //Configura e inicializa el movimiento automatico
    Serial.println("AUT");
    recorrido.total_loops = message.integer_input;
    stepper.moveTo(recorrido.start);
    direc = getDirection();
    rightButtonEnabled = false;
    leftButtonEnabled = false;
    state = goingToStart;
  }
  else if (message.command == "BCK") {
    //Imprime el backlash
    Serial.println(backlash);
  }
  else if (message.command == "HOM") {
    //Homing
    Serial.println("HOM");
    gohome();
  }
  else if (message.command == "SCP") {
    //Set current position (Definir manualmente la posicion)
    Serial.println("SCP");
    stepper.setCurrentPosition(message.long_input);
  }
  else if (message.command == "INF") {
    //Info de la configuracion actual, **usar SOLO con el serial monitor del IDE**
    String info = getInfo();
    Serial.println(info);
  }
  else if (message.command == "IDN") {
    Serial.println("Ver tabla de comandos en /escritorio/ingridmarco/control_motor/comandos"); //Mensaje para el laboratorio
  }
  else if (message.command == "DIR") {
    //Imprime la direccion actual
    Serial.println(direc);
  }
  else if (message.command == "BLK") {
    //Se mueve hasta long_input con velocidad integer_input BLOQUEANDO
    //en lo posible no usar esto, es preferible usar REL
    Serial.println("BLK");
    stepper.setMaxSpeed(message.integer_input);
    stepper.move(message.long_input);
    direc = getDirection();
    stepper.runToPosition();
  }
  else if (message.command == "SBK") {
    //Definir manualmente el backlash
    backlash = message.long_input;
    Serial.println("SBK");
  }
  else if (message.command == "REL") {
    //Se mueve long_input pasos relativos a la posicion actual, con velocidad integer_input
    Serial.println("REL");
    stepper.setMaxSpeed(message.integer_input);
    stepper.move(message.long_input);
    direc = getDirection();
  }
  else if (message.command == "STE") {
    //Configura e inicializa el movimiento automatico por pasos
    Serial.println("STE");
    step_lenght = message.integer_input;
    total_lenght = message.long_input;
    stepper.move(step_lenght);
    direc = getDirection();
    state2 = stepping;
  }
  else {
    //si el comando no coincide con ninguno, se responde NAK (not acknowleged)
    Serial.println("NAK");
  }
}


//=============
// Pulsadores
//=============

//Esta función lee el estado de los botones ayudandose con los métodos de la clase PushButton y define que acciones
//tienen que ejecutarse si se activan
void buttonCommands() {
  if (stopButton.isOn()) {
    stepper.stop();
    state = canceled;
    state2 = stepDisabled;
  }
  else if (rightButtonEnabled && rightButton.isOn()) {
    stepper.move(133);
    direc = getDirection();
  }
  else if (leftButtonEnabled && leftButton.isOn()) {
    stepper.move(-133);
    direc = getDirection();
  }
  else if (homeButton.isOn()) {
    gohome();
  }
}


//=========================
// Comunicación por Serial
//=========================

//Esta función lee el puerto USB y va guardando los datos en un array
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

// Esta función detecta las comas que separan cada parámetro y guarda cada parte en una variable del tipo mensaje
// convierte los datos guardados, que son caracteres, a variables numéricas del tipo integer
Message parseData() {      
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

//==================================
//Correccion de direccion y backlash
//==================================

//Esta funcion calcula la dirección actual del motor
//Hay que correrla cada vez que se usa un comando que pueda cambiar la posición objetivo
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

//Esta función se fija si la dirección cambió y avisa al resto del programa si hay que corregirla o no
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

//Si la dirección cambió, esta función hace que el motor de los pasos necesarios para corregir el backlash
//antes de ir a la posición objetivo que se pidió
//una vez que corrigió, redefine la posición actual y la de objetivo
void adjustBacklash() {
  if (directionHasChanged) {
    if (skipNext) {
      skipNext = false;
    }
    else {
      long target = stepper.targetPosition();
      long current = stepper.currentPosition();
      if (direc == DER) {
        stepper.move(backlash); //mueve -backlash- pasos positivos/derecha
      }
      else if (direc == IZQ) {
        stepper.move(-backlash); //mueve -backlash- pasos negativos/izquierda
      }
      stepper.runToPosition();
      stepper.setCurrentPosition(current);
      stepper.moveTo(target);
    }
  }
}


//=========
// Extras
//=========

//Esta funcion prende y apaga los leds del gabinete
//tomando como argumentos el numero de veces y el intervalo de tiempo
void blinkLeds(const int num_blinks, const long interval) {
  long time_ = millis();
  int blinks = 0;
  while (blinks < num_blinks) {
    while ((millis() - time_) < interval) {
      redLed.digitalWriteFast(HIGH);
      greenLed.digitalWriteFast(HIGH);
    }
    time_ = millis();
    while ((millis() - time_) < interval) {
      redLed.digitalWriteFast(LOW);
      greenLed.digitalWriteFast(LOW);
    }
    time_ = millis();
    blinks++;
  }
}

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




