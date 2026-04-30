#ifndef KEYPAD_H
#define KEYPAD_H

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <queue.h>

// Shared queue
extern QueueHandle_t keypadQueue;

// Setup + Tasks Subroutine
void initKeypad();
void scanKeypad(void *pvParameters);
void otherKeypad(void *pvParameters);

#endif