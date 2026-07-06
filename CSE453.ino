#include <Arduino_FreeRTOS.h>
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>
#include <EEPROM.h>
#include <queue.h>
#include <string.h>

// Pin Declarations
const int powerPin = 8;
const int rowPins[4] = {22,24,26,28};
const int colPins[3] = {32,34,36};
const int changePin=40, backPin=42, clearPin=44;
const int relay1=2, relay2=4;

// Keypad Declarations
const char keys[4][3] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'L', '0', 'U'}
};
bool currentState[4][4] = {{false}};			      // is button pressed right now?
bool lastState[4][4] = {{false}}; 				      // was button pressed before?
unsigned long lastDebounceTime[4][4] = {{0}};	  // when did button state last change?
const int debounceDelay = 25;         			    // ms 
bool firstRead = true;

// Other Key Declarations
struct Btn { 
    int pin; 
    char key;
    bool lastStable, lastRead; 
    uint32_t lastChange, lastFire; 
};
Btn btns[] = {
  {clearPin, 'C', HIGH, HIGH, 0, 0},
  {backPin, 'B', HIGH, HIGH, 0, 0},
  {changePin, 'A', HIGH, HIGH, 0, 0},
};

// Object Declarations
Adafruit_7segment matrix;
QueueHandle_t queue;

// Variable Declarations
char password[] = "1234";
char current[] = "____";
int index = 0;
unsigned long lastKeyPressTime = 0;
bool lockState = true;

void initKeypad() {
  // set up row pins
  for (int i = 0; i < 4; i++){
      pinMode(rowPins[i], OUTPUT);        // row pins send signals
      digitalWrite(rowPins[i], HIGH);     // start with rows OFF
  }
  
  // set up column pins
  for (int i = 0; i < 4; i++){
      pinMode(colPins[i], INPUT_PULLUP);  // column pins read signals
  }

  pinMode(changePin, INPUT_PULLUP);
  pinMode(backPin, INPUT_PULLUP);
  pinMode(clearPin, INPUT_PULLUP);
}

void scanKeypad() {
  while (true) {
    for (int row = 0; row < 4; row++){
      digitalWrite(rowPins[row], LOW);    // turn this row ON
      delayMicroseconds(10);              // wait for signal to settle

      // check each column in this row
      for (int col = 0; col < 4; col++){
        bool reading = !digitalRead(colPins[col]);  // read button (LOW = pressed)
        
        // did state change?
        if (reading != currentState[row][col]){
          lastDebounceTime[row][col] = millis();  // save time of change
          currentState[row][col] = reading;        // save new state
        }
        
        // has enough time passed?
        if ((millis() - lastDebounceTime[row][col]) > debounceDelay) {
            
          // is this a new press?
          if (reading != lastState[row][col]) {
            lastState[row][col] = reading;  // update last state

            // button was pressed
            if (reading){
              if (!firstRead) {
                digitalWrite(rowPins[row], HIGH);  // turn row back OFF
                //Serial.println(keys[row][col]);
                xQueueSend(queue, &keys[row][col], 0);
              } else {
                // This first read is to prevent an extra '4'
                // that is sent
                firstRead = false;
              }
            }
          }
        }
      }
      digitalWrite(rowPins[row], HIGH);  // turn this row OFF
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void otherKeypad() {
  while (true) {
    uint32_t now = millis();
    for(auto &b: btns) {
      int currentRead = digitalRead(b.pin);
      if (currentRead != b.lastRead) { b.lastChange = now; }
      if ((now - b.lastChange) > debounceDelay) {
        if (currentRead != b.lastStable) {
          b.lastStable = currentRead;
          if (b.lastStable == LOW) {
            // Serial.println(b.key);
            xQueueSend(queue, &b.key, 0);
          }
        }
      }
      b.lastRead = currentRead;
    }
  }
}

void updateDisplay() {
  // Clear the display
  matrix.clear();
  // Logic to update display
  // Serial.println(index);
  matrix.println(current);

  if (index >= 2) { matrix.writeDigitAscii(0, '-'); } 
  if (index >= 3) { matrix.writeDigitAscii(1, '-'); } 
  if (index == 4) { 
    // I have no clue why index 3 is 2 
    matrix.writeDigitAscii(3, '-'); 
    // time = millis();                    // Implmenent 4th '-' with timeout later
  } 
  
  // Confirm writing to display
  matrix.writeDisplay();
}

void inputHandler() {
  char input;

  while (true) {
    // Get an character from queue
    if (xQueueReceive(queue, &input, portMAX_DELAY)==pdTRUE) {
      lastKeyPressTime = millis();
      Serial.println(input);
      // Serial.println(lockState);
      // Check if it is a number input
      if (input >= '0' && input <= '9') {
        // Update current string password
        if (index < 4) {
          current[index] = input;
          index++;
        }
        // keep index at 4
        if (index >= 4) {
          index = 4;
        }

        updateDisplay();
      }
      // If L or U check if the password matches internal password
      if (input == 'U') {
        if (strcmp(current, password) == 0) {
          digitalWrite(relay1, HIGH);
          digitalWrite(relay2, LOW);
          lockState = false;

          delay(5000);
        } else {
          current[0] = '_';
          current[1] = '_';
          current[2] = '_';
          current[3] = '_';
          index = 0;
          updateDisplay();
        }
      }
      if (input == 'L') {
        digitalWrite(relay1, LOW);
        digitalWrite(relay2, HIGH);
        lockState = true;
        delay(5000);
      }
      // For other keys, manage seperately
      if (input == 'A') { // Change password 
        // Check if there is a valid password & if it is unlocked
        if (index == 4 && !lockState) {
          Serial.println("Test"); 
          // Set the current password to the new one
          password[0] = current[0];
          password[1] = current[1];
          password[2] = current[2];
          password[3] = current[3];
          // Write new password to EEPROM 
          EEPROM.write(0, password[0]);
          EEPROM.write(4, password[1]);
          EEPROM.write(8, password[2]);
          EEPROM.write(12, password[3]);
          // Clear the current password and reset index
          current[0] = '_';
          current[1] = '_';
          current[2] = '_';
          current[3] = '_';
          index = 0;
          // Update display
          updateDisplay();
        }
      }
      if (input == 'B') { // Backspace
        index--;
        if (index <= 0) {
          index = 0;
        }
        Serial.println(index);
        current[index] = '_';
        Serial.println(current);
        updateDisplay();
      }
      if (input == 'C') { // Clear
        current[0] = '_';
        current[1] = '_';
        current[2] = '_';
        current[3] = '_';
        index = 0;
        updateDisplay();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void timeoutTask() {
  while (true) {
    // After a minute of no key presses
    //Serial.println((millis() - lastKeyPressTime));
    if ((millis() - lastKeyPressTime) > 60000) {
      // Serial Write
      Serial.println("Shutting Down");
      // Make sure system is locked
      lockState = true;
      // TODO: add gpio pins for actuator to go up 
      digitalWrite(relay1, LOW);
      digitalWrite(relay2, HIGH);
      // Make sure we write password to system
      EEPROM.write(0, password[0]);
      EEPROM.write(4, password[1]);
      EEPROM.write(8, password[2]);
      EEPROM.write(12, password[3]);
      // Make sure there is enough time for EEPROM and for actuator to lock
      delay(8000);
      // Turn off system
      digitalWrite(powerPin, LOW);
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void setup() {
  // First thing to do is to set pin
  // to Power Arduino
  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, HIGH);
  // Hard reset password 
  // EEPROM.write(0, '0');
  // EEPROM.write(4, '0');
  // EEPROM.write(8, '0');
  // EEPROM.write(12, '0');
  // Get password from EEPROM
  password[0] = EEPROM.read(0);
  password[1] = EEPROM.read(4);
  password[2] = EEPROM.read(8);
  password[3] = EEPROM.read(12);

  // Enable Serial for Debugging & Print
  Serial.begin(115200);
  Serial.println(password);

  // Pinmode for Actuator Control
  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);

  // Setup 7-segment display with I2C
  // Communication by Adafruit
  matrix = Adafruit_7segment();
  matrix.begin(0x70); 
  updateDisplay();

  // Initialize Keypad I/O
  initKeypad();
  // Create Queue
  queue = xQueueCreate(10, sizeof(char));
  // Setup tasks with FreeRTOS
  xTaskCreate(
    scanKeypad,       // Function
    "KeypadV1",       // Name of Task
    512,              // Stack Size
    NULL,             // Parameter
    1,                // Priority Number
    NULL              // Handle of Task Creation
  );
  xTaskCreate(
    otherKeypad,      // Function
    "KeypadV2",       // Name of Task
    512,              // Stack Size
    NULL,             // Parameter
    1,                // Priority Number
    NULL              // Handle of Task Creation
  );
  xTaskCreate(
    inputHandler,     // Function
    "Input Handler",  // Name of Task
    512,              // Stack Size
    NULL,             // Parameter
    1,                // Priority Number
    NULL              // Handle of Task Creation
  );
  xTaskCreate(
    timeoutTask,        // Function
    "Turn off Handler", // Name of Task
    512,              // Stack Size
    NULL,             // Parameter
    1,                // Priority Number
    NULL              // Handle of Task Creation
  );
}
void loop() { }
