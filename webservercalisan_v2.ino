// Updated code for webservercalisan_v2.ino

#include <Arduino.h>

// Constants
const int buttonPin = 2; // Pin where the button is connected

// Variables
int buttonState = 0; // Variable to hold the button state

void setup() {
  // Initialize the button pin as an input
  pinMode(buttonPin, INPUT);
  
  // Start the serial communication
  Serial.begin(9600);
}

void loop() {
  // Read the state of the button
  buttonState = digitalRead(buttonPin);
  
  // Check if the button is pressed
  if (buttonState == HIGH) {
    Serial.println("Button pressed!");
  }
  
  delay(100); // Small delay to debounce the button
}