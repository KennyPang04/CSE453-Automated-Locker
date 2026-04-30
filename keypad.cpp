#include "keypad.h"

// Pin Declarations
const int rowPins[4] = {8, 9, 10, 11};
const int colPins[3] = {3, 4, 5};
const int otherPins[4] = {1, 2, 3, 4, 5};    // Make sure this follows the same order as the list below !!!

// Keypad Declarations
const int debounceDelay = 25;
struct Button {
  char key;
  bool lastStable, lastRead;
  uint32_t lastChange; 

  Button(char k) {
    key = k;
    lastStable = HIGH, lastRead = HIGH;
    lastChange = 0;
  }
};
// Number Button List for Scan (4x3)
Button mainList[4][3] = {
  {Button('1'), Button('2'), Button('3')},
  {Button('4'), Button('5'), Button('6')},
  {Button('7'), Button('8'), Button('9')},
  {Button('L'), Button('0'), Button('U')}
};
// Other buttons that are individually checked (side column button + reset button)
Button sideList[4] = {
    Button('A'),        // Change Password
    Button('B'),        // Backspace
    Button('C'),        // Clear 
    Button('D'),        // Reset Password
    Button('E')         // Power Button
};

// Queue instance 
QueueHandle_t keypadQueue;

// Simple debounce for key presses that can be used universally
char simpleKeyDebounce(Button &b, int pin) {
  // Read the current time (ms) and pin state
  uint32_t currentTime = millis();
  int currentRead = digitalRead(pin);

  // Check if the state of button changed & update time
  if (currentRead != b.lastRead) { b.lastChange = currentTime; }
  // Check if the time between last button change is greater than debounce target
  if ((currentTime - b.lastChange) > debounceDelay) {
    // Check if the state of button changed since last stable & update it
    if (currentRead != b.lastStable) {
      b.lastStable = currentRead;
      // If and only if it is low, then the button was pressed
      if (b.lastStable == LOW) {
        Serial.println(b.key);
        return b.key;
      }
    }
  }
  // Update the lastRead of the button
  b.lastRead = currentRead;
  return '\0';
}

// Initialize Keypad Pins
void initKeypad() {
  // set up row pins as OUTPUT
  for (int i = 0; i < (sizeof(rowPins)/sizeof(int)); i++){
      pinMode(rowPins[i], OUTPUT);        // row pins send signals
      digitalWrite(rowPins[i], HIGH);     // start with rows OFF
  }
  
  // set up column pins as INPUT_PULLUP
  for (int i = 0; i < (sizeof(colPins)/sizeof(int)); i++){
      pinMode(colPins[i], INPUT_PULLUP);  // column pins read signals
  }

  // set up individual button pins as INPUT_PULLUP 
  // as they will be read seperately
  for (int i = 0; i < (sizeof(otherPins)/sizeof(int)); i++){
      pinMode(colPins[i], INPUT_PULLUP);  // column pins read signals
  }
}

void scanKeypad(void *pvParameters) {
  // For each row pin
  for (int i = 0; i < 4; i++) {
    digitalWrite(rowPins[i], LOW);
    for (int j = 0; j < 3; j++) {
      int symbol = simpleKeyDebounce(buttonList[i][j], colPins[j]);
      if (symbol != '\0') { xQueueSend(queue, &keys[row][col].key, 0); }
    }
    digitalWrite(rowPins[i], HIGH);
  }
  vTaskDelay(pdMS_TO_TICKS(5));
}

void otherKeypad(void *pvParameters) {
  // for each button in list
  for (int i = 0; i < 4; i++) {
    int symbol = simpleKeyDebounce(sideList[i], otherPins[i]);
    if (symbol != '\0') { xQueueSend(queue, &sideList, 0); }
  }
  vTaskDelay(pdMS_TO_TICKS(5));
}