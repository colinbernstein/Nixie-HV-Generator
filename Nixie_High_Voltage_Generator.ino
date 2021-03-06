const byte neonBulb = 2, A = 4, B = 7, C = 8, D = 10, gatePin = 9, backlightPWMOut = 11, relay = 12, togglePin = A0, upButton = A1, downButton = A2,
           digitSelectPin = A3, backlightPWMIn = A4, voltageReadPin = A5, N[] = {3, 5, 6}, clean[] = {3, 8, 9, 4, 0, 5, 7, 2, 6, 1};

unsigned long timePressed, timeRefresh, lastLEDRefresh, lastCPP;
word setVoltage, currentVoltage;
unsigned char dutyCycle = 5;
byte currDigitValue[3], currentDigit;
boolean outputEngaged = false, holding = false;

void setup() {
  pinMode(A, OUTPUT);
  pinMode(B, OUTPUT);
  pinMode(C, OUTPUT);
  pinMode(D, OUTPUT);
  for (int a = 0; a < 3; a++)
    pinMode(N[a], OUTPUT);
  pinMode(relay, OUTPUT);
  pinMode(neonBulb, OUTPUT);
  pinMode(backlightPWMOut, OUTPUT);
  pinMode(gatePin, OUTPUT);
  TCCR1B = TCCR1B & B11111000 | B00000001;
  pinMode(backlightPWMIn, INPUT);
  pinMode(voltageReadPin, INPUT);
  pinMode(togglePin, INPUT_PULLUP);
  pinMode(digitSelectPin, INPUT_PULLUP);
  pinMode(upButton, INPUT_PULLUP);
  pinMode(downButton, INPUT_PULLUP);
  analogWrite(gatePin, 0);
}

void loop() {
  multPlex();
  checkButton();
  analogWrite(backlightPWMOut, analogRead(backlightPWMIn) / 4);
  if (millis() - lastCPP >= 300000)
    cathodePoisoningPrevention();
}

void multPlex() {
  switch (currentDigit) {
    case 0:
      binOut(millis() / 300 % 2 == 0 ? setVoltage / 100 % 10 : -1, 0);
      binOut(setVoltage / 10 % 10, 1);
      binOut(setVoltage % 10, 2);
      break;
    case 1:
      binOut(setVoltage / 100 % 10, 0);
      binOut(millis() / 300 % 2 == 0 ? setVoltage / 10 % 10 : -1, 1);
      binOut(setVoltage % 10, 2);
      break;
    case 2:
      binOut(setVoltage / 100 % 10, 0);
      binOut(setVoltage / 10 % 10, 1);
      binOut(millis() / 300 % 2 == 0 ? setVoltage % 10 : -1, 2);
      break;
    case 3:
      if (millis() - timeRefresh >= 100) {
        timeRefresh = millis();
        currentVoltage = adjustPulsation();
        if (currentVoltage >= 100)
          refresh(currentVoltage / 100 % 10, 0);
        else
          refresh(-1, 0);
        if (currentVoltage >= 10)
          refresh(currentVoltage / 10 % 10, 1);
        else
          refresh(-1, 1);
        refresh(currentVoltage % 10, 2);
      }
      binOut(currDigitValue[0], 0);
      binOut(currDigitValue[1], 1);
      binOut(currDigitValue[2], 2);
      break;
  }
}

void refresh(byte number, byte stage) {
  currDigitValue[stage] = number;
}

void binOut(byte number, byte stage) {
  digitalWrite(stage == 0 ? N[2] : N[stage - 1], LOW);
  if (number == 255) {
    digitalWrite(D, HIGH);
    digitalWrite(C, HIGH);
    digitalWrite(B, HIGH);
    digitalWrite(A, HIGH);
  } else {
    digitalWrite(A, bitRead(number, 0));
    digitalWrite(B, bitRead(number, 1));
    digitalWrite(C, bitRead(number, 2));
    digitalWrite(D, bitRead(number, 3));
  }
  digitalWrite(N[stage], HIGH);
  delay(4);
}

void checkButton() {
  if (!holding) {
    if (!digitalRead(togglePin)) {
      timePressed = millis();
      holding = true;
      outputEngaged = !outputEngaged;
      currentDigit = 3;
      digitalWrite(neonBulb, outputEngaged);
      digitalWrite(relay, outputEngaged);
    }
    else if (!outputEngaged) {
      if (!digitalRead(digitSelectPin)) {
        timePressed = millis();
        holding = true;
        currentDigit = currentDigit == 3 ? 0 : currentDigit + 1;
        analogWrite(gatePin, currentDigit == 3 ? dutyCycle : 0);
        switch (currentDigit) {
          case 1: refresh(setVoltage / 100 % 10, 0);
          case 2: refresh(setVoltage / 10 % 10, 1);
        }
      }
      else if (!digitalRead(upButton)) {
        timePressed = millis();
        holding = true;
        switch (currentDigit) {
          case 0:  if (setVoltage >= 300)
              setVoltage -= 300;
            else setVoltage += 100; break;
          case 1:
            if (setVoltage / 100 % 10 == 3) {
              if (setVoltage / 10 % 10 == 5)
                setVoltage -= 50;
              else setVoltage += 10; break;
            } else {
              if (setVoltage / 10 % 10 == 9)
                setVoltage -= 90;
              else setVoltage += 10; break;
            }
          case 2:
            if (setVoltage < 350)
              if (setVoltage % 10 == 9)
                setVoltage -= 9;
              else  setVoltage += 1; break;
        }
      }
      else if (!digitalRead(downButton)) {
        timePressed = millis();
        holding = true;
        switch (currentDigit) {
          case 0:  if (setVoltage < 100)
              setVoltage += setVoltage <= 50 ? 300 : 200;
            else setVoltage -= 100; break;
          case 1:
            if (setVoltage / 100 % 10 == 3) {
              if (setVoltage / 10 % 10 == 0)
                setVoltage += 50;
              else setVoltage -= 10; break;
            } else {
              if (setVoltage / 10 % 10 == 0)
                setVoltage += 90;
              else setVoltage -= 10; break;
            }
          case 2:
            if (setVoltage < 350)
              if (setVoltage % 10 == 0)
                setVoltage += 9;
              else  setVoltage -= 1; break;
        }
      }
    }
  }
  else if (digitalRead(digitSelectPin) && digitalRead(togglePin) && digitalRead(upButton) && digitalRead(downButton) && millis() - timePressed >= 50)
    holding = false;
}

word adjustPulsation() {
  word currentVoltage = getCurrentVoltage();
  if (currentVoltage >= 375) {
    emergencyShutdown();
    return 0;
  }
  short difference = currentVoltage - setVoltage;
  if (difference < 5)
    dutyCycle++;
  else if (difference > 5)
    dutyCycle--;
  if (dutyCycle < 5)
    dutyCycle = 5;
  else if (dutyCycle > 237)
    dutyCycle = 237;
  analogWrite(gatePin, dutyCycle);
  return currentVoltage;
}

void emergencyShutdown() {
  analogWrite(gatePin, 0);
  digitalWrite(gatePin, LOW);
  outputEngaged = false;
  digitalWrite(relay, LOW);
  digitalWrite(neonBulb, LOW);
  digitalWrite(A, LOW);
  digitalWrite(B, LOW);
  digitalWrite(C, LOW);
  digitalWrite(D, LOW);
  for (byte i = 0; i < 3; i++)
    digitalWrite(N[i], LOW);
  for (byte i = 0; i < 5; i++) {
    digitalWrite(neonBulb, HIGH);
    delay(100);
    digitalWrite(neonBulb, LOW);
    delay(100);
  }
  setVoltage = 0;
  dutyCycle = 0;
  currentDigit = 0;
}

word getCurrentVoltage() {
  return analogRead(voltageReadPin) / 1.955;
}

void cathodePoisoningPrevention() {
  lastCPP = millis();
  for (byte a = 0; a < 3; a++)
    digitalWrite(N[a], LOW);
  for (byte c = 0; c < 3; c++) {
    digitalWrite(N[c], HIGH);
    for (byte d = 0; d < 2; d++) {
      for (byte n = 0; n <= 9; n++) {
        digitalWrite(A, bitRead(clean[n], 0));
        digitalWrite(B, bitRead(clean[n], 1));
        digitalWrite(C, bitRead(clean[n], 2));
        digitalWrite(D, bitRead(clean[n], 3));
        delay(100);
      }
      for (byte n = 8; n > 0; n--) {
        digitalWrite(A, bitRead(clean[n], 0));
        digitalWrite(B, bitRead(clean[n], 1));
        digitalWrite(C, bitRead(clean[n], 2));
        digitalWrite(D, bitRead(clean[n], 3));
        delay(100);
      }
    }
    digitalWrite(N[c], LOW);
  }
  digitalWrite(A, HIGH);
  digitalWrite(B, HIGH);
  digitalWrite(C, HIGH);
  digitalWrite(D, HIGH);
  for (byte a = 0; a < 3; a++)
    digitalWrite(N[a], LOW);
}
