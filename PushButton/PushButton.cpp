/*
   PushButton.cpp - mini lib para controlar los pulsadores del gabinete
   Ingrid Heuer, Julio 2021
*/

#include "Arduino.h"
#include "PushButton.h"

PushButton::PushButton(byte pin, bool singlePush, unsigned long debounceDelay)
{
  pinMode(pin, INPUT_PULLUP);
  _pin = pin;
  _debounceDelay = debounceDelay;
  _singlePush = singlePush;
}

void PushButton::readAndDebounce() {

  byte reading = digitalRead(_pin);

  if (reading != _previousState) {
    _lastDebounceTime = millis();
  }

  if ((millis() - _lastDebounceTime) >= _debounceDelay) {
    _state = reading;
  }

  _previousState = reading;
}

void PushButton::readAndDebounce_singlePush() {

  byte reading = digitalRead(_pin);

  if (reading != _previousState) {
    _lastDebounceTime = millis();
  }

  if ((_state != reading) && ((millis() - _lastDebounceTime) >= _debounceDelay)) {
    _state = reading;

    if (_state == LOW) {
      _toggled = true;
    }
  }
  else {
    _toggled = false;
  }

  _previousState = reading;
}

bool PushButton::isOn() {

  if (_singlePush) {
    readAndDebounce_singlePush();
    return _toggled;
  }
  else {
    readAndDebounce();
    return (_state == LOW);
  }
}
