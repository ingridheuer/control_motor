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
    void readAndDebounce();
    void readAndDebounce_singlePush();
    bool isOn();

  private:
    byte _pin;
    unsigned long _debounceDelay;
    bool _singlePush;
    byte _state = HIGH;
    byte _previousState = HIGH;
    unsigned long _lastDebounceTime = 0;
    bool _toggled = false;
};

#endif
