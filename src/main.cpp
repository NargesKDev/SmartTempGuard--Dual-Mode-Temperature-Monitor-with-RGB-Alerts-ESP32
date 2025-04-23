#include <Arduino.h>

#define TMP_PIN 32
#define BUTTON_PIN 34

// RGB LED 1 (Temperature Status)
#define RED1_PIN 25
#define GREEN1_PIN 2
#define BLUE1_PIN 4

// RGB LED 2 (Mode Indicator)
#define RED2_PIN 21
#define GREEN2_PIN 14
#define BLUE2_PIN 12

#define SAMPLES 50
#define INTERVAL 1000
#define MILLIVOLTS_PER_CELSIUS 10
#define VOLTAGE_OFFSET -100

enum SystemMode
{
    MONITOR_MODE,
    SET_MODE
};
SystemMode currentMode = MONITOR_MODE;

float thresholdTemp = 25.0;
bool awaitingInput = false;

QueueHandle_t inputQueue;

// LED Control
void setRGB(int rPin, int gPin, int bPin, int r, int g, int b)
{
    ledcWrite(rPin, r);
    ledcWrite(gPin, g);
    ledcWrite(bPin, b);
}

// Temperature Reading
float readTemperature()
{
    float millivolts = 0;
    for (int i = 0; i < SAMPLES; ++i)
    {
        delay(INTERVAL / SAMPLES);
        millivolts += analogReadMilliVolts(TMP_PIN);
    }
    millivolts /= SAMPLES;
    return (millivolts - VOLTAGE_OFFSET) / MILLIVOLTS_PER_CELSIUS;
}

// Temp Task
void TempTask(void *pvParams)
{
    while (1)
    {
        if (currentMode == MONITOR_MODE)
        {
            float temp = readTemperature();
            Serial.printf("Current Temperature: %.2f °C, Threshold: %.2f °C\n", temp, thresholdTemp);

            if (temp < thresholdTemp - 1.5)
                setRGB(0, 1, 2, 0, 0, 255); // Blue - cold
            else if (temp > thresholdTemp + 1.5)
                setRGB(0, 1, 2, 255, 0, 0); // Red - hot
            else
                setRGB(0, 1, 2, 0, 255, 0); // Green - comfort
        }
        else
        {
            setRGB(0, 1, 2, 0, 0, 0); // OFF in SET mode
        }

        // Check the queue for new threshold value
        if (xQueueReceive(inputQueue, &thresholdTemp, 0) == pdTRUE)
        {
            Serial.printf("Threshold updated to: %.2f °C from queue\n", thresholdTemp);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Mode Indicator LED2
void ModeLedTask(void *pvParams)
{
    while (1)
    {
        if (currentMode == MONITOR_MODE)
            setRGB(3, 4, 5, 100, 100, 200); // Yellow
        else
            setRGB(3, 4, 5, 10, 150, 10); // Cyan
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Button Task
void ButtonTask(void *pvParams)
{
    pinMode(BUTTON_PIN, INPUT);
    bool lastState = HIGH;

    while (1)
    {
        bool currentState = digitalRead(BUTTON_PIN);
        if (lastState == HIGH && currentState == LOW)
        {
            currentMode = (currentMode == MONITOR_MODE) ? SET_MODE : MONITOR_MODE;
            Serial.println(currentMode == SET_MODE ? "SET MODE: Type new threshold in Serial..." : "MONITOR MODE");

            if (currentMode == SET_MODE)
            {
                awaitingInput = true;
            }

            vTaskDelay(pdMS_TO_TICKS(300)); // Debounce
        }
        lastState = currentState;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Serial Input Handler
void SerialTask(void *pvParams)
{
    String input = "";
    while (1)
    {
        if (awaitingInput && Serial.available())
        {
            input = Serial.readStringUntil('\n');
            float newThreshold = input.toFloat();
            if (newThreshold > -100 && newThreshold < 150)
            {
                xQueueSend(inputQueue, &newThreshold, portMAX_DELAY); // Send the new threshold to the queue
                Serial.printf("Threshold updated to: %.2f °C\n", newThreshold);
            }
            else
            {
                Serial.println("Invalid input. Try again.");
            }

            awaitingInput = false;
            currentMode = MONITOR_MODE;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Setup PWM Channels
void setupPWM()
{
    for (int ch = 0; ch < 6; ch++)
    {
        ledcSetup(ch, 5000, 8);
    }
    ledcAttachPin(RED1_PIN, 0);
    ledcAttachPin(GREEN1_PIN, 1);
    ledcAttachPin(BLUE1_PIN, 2);
    ledcAttachPin(RED2_PIN, 3);
    ledcAttachPin(GREEN2_PIN, 4);
    ledcAttachPin(BLUE2_PIN, 5);
}

void setup()
{
    Serial.begin(115200);
    setupPWM();

    // Create the queue to hold a single float (threshold value)
    inputQueue = xQueueCreate(1, sizeof(float));

    xTaskCreate(TempTask, "Temperature", 2048, NULL, 1, NULL);
    xTaskCreate(ButtonTask, "Button", 2048, NULL, 2, NULL);
    xTaskCreate(ModeLedTask, "ModeLED", 1024, NULL, 1, NULL);
    xTaskCreate(SerialTask, "SerialInput", 2048, NULL, 1, NULL);
}

void loop()
{
    // Nothing here. Tasks handle everything.
}
