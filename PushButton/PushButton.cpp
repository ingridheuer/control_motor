/*
   PushButton.cpp - mini lib para controlar los pulsadores del gabinete
   Ingrid Heuer, Julio 2021
*/

#include "Arduino.h"
#include "PushButton.h"

PushButton::PushButton(byte pin, bool stateChange, unsigned long debounceDelay)
{
  _pin = pin;
  _debounceDelay = debounceDelay;
  _stateChange = stateChange;
  _state = HIGH;
}

void PushButton::setPin()
{
  pinMode(_pin, INPUT_PULLUP);
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

void PushButton::readAndDebounce_stateChange() {

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

  if (_stateChange) {
    readAndDebounce_stateChange();
    return _toggled;
  }
  else {
    readAndDebounce();
    return (_state == LOW);
  }
}
