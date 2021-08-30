/*
   PushButton.h -Mini lib para controlar los pulsadores del gabinete
   Ingrid Heuer, Julio 2021
*/

#ifndef PushButton_h
#define PushButton_h

#include "Arduino.h"

class PushButton
{
  public:
    PushButton(byte pin, bool singlePush, unsigned long debounceDelay);
    bool enabled = true;
    void readAndDebounce();
    void readAndDebounce_singlePush();
    bool isOn();

  private:
    byte _pin;
    byte _state = HIGH;
    byte _previousState;
    unsigned long _lastDebounceTime = 0;
    unsigned long _debounceDelay;
    bool _singlePush;
    bool _toggled = false;
};

#endif
