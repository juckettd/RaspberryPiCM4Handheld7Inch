#include <XInput.h>

uint8_t smallRumbleCounter = 0;
uint8_t bigRumbleCounter = 0;

void setup() {
  pinMode(1, INPUT_PULLUP);  
  pinMode(2, INPUT_PULLUP);  
  pinMode(3, INPUT_PULLUP);  
  pinMode(4, INPUT_PULLUP);  
  pinMode(5, INPUT_PULLUP);  
  pinMode(6, INPUT_PULLUP);  
  pinMode(7, INPUT_PULLUP);  
  pinMode(8, INPUT_PULLUP);  
  pinMode(9, INPUT_PULLUP);  
  pinMode(10, INPUT_PULLUP);  
  pinMode(11, INPUT_PULLUP);  
  pinMode(12, INPUT_PULLUP);  
  pinMode(16, INPUT_PULLUP);  
  pinMode(17, INPUT_PULLUP);  
  pinMode(18, INPUT_PULLUP);  
  pinMode(19, INPUT_PULLUP);  

  pinMode(22, OUTPUT);
  pinMode(23, OUTPUT);

  analogWriteFrequency(22, 187500);
  analogWriteFrequency(23, 187500);

  XInput.begin();

  XInput.setTriggerRange(0, 1);
  XInput.setJoystickRange(300, 740);
}


void loop() {
  boolean upPad = !digitalRead(3);
  boolean downPad = !digitalRead(2);
  boolean leftPad = !digitalRead(4);
  boolean rightPad = !digitalRead(5);

  boolean buttonA = !digitalRead(9);
  boolean buttonB = !digitalRead(18);
  boolean buttonX = !digitalRead(12);
  boolean buttonY = !digitalRead(19);
  boolean buttonLB = !digitalRead(7);
  boolean buttonRB = !digitalRead(16);
  boolean buttonL3 = !digitalRead(1);
  boolean buttonR3 = !digitalRead(8);
  boolean buttonStart = !digitalRead(11);
  boolean buttonSelect = !digitalRead(10);

  int buttonLT = !digitalRead(6);
  int buttonRT = !digitalRead(17);

  int leftStickX = analogRead(6);
  int leftStickY = analogRead(7);

  int rightStickX = analogRead(0);
  int rightStickY = analogRead(1);

  uint8_t bigRumble = XInput.getRumbleLeft();
  uint8_t smallRumble = XInput.getRumbleRight();
  uint8_t bigRumbleSet = 0;
  uint8_t smallRumbleSet = 0;

  XInput.setDpad(upPad, downPad, leftPad, rightPad);
  XInput.setJoystick(JOY_LEFT, leftStickX, leftStickY);
  XInput.setJoystick(JOY_RIGHT, rightStickX, rightStickY);

  XInput.setTrigger(TRIGGER_LEFT, buttonLT);
  XInput.setTrigger(TRIGGER_RIGHT, buttonRT);
    
  XInput.setButton(BUTTON_A, buttonA);
  XInput.setButton(BUTTON_B, buttonB);
  XInput.setButton(BUTTON_X, buttonX);
  XInput.setButton(BUTTON_Y, buttonY);
  XInput.setButton(BUTTON_LB, buttonLB);
  XInput.setButton(BUTTON_RB, buttonRB);
  XInput.setButton(BUTTON_L3, buttonL3);
  XInput.setButton(BUTTON_R3, buttonR3);
  XInput.setButton(BUTTON_START, buttonStart);
  XInput.setButton(BUTTON_BACK, buttonSelect);

  // Lower the rumble values for the motors in the handheld
  if (bigRumble > 25)
  {
    bigRumbleCounter = 0;
    bigRumbleSet = bigRumble / 8 + 45;
  }
  else
  {
    if (bigRumbleCounter > 10)
      bigRumbleSet = 0;
    else
      bigRumbleCounter = bigRumbleCounter + 1;
  }

  if (smallRumble > 25)
  {
    smallRumbleCounter = 0;
    smallRumbleSet = smallRumble / 8 + 45;
  }
  else
  {
    if (smallRumbleCounter > 10)
      smallRumbleSet = 0;
    else
      smallRumbleCounter = smallRumbleCounter + 1;
  }

  analogWrite(23, bigRumbleSet);
  analogWrite(22, smallRumbleSet);
  
  delay(5);
}
