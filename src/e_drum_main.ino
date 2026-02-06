#include <BLEMIDI_Transport.h>
#include <hardware/BLEMIDI_ESP32.h>

// -- configuration --
BLEMIDI_CREATE_INSTANCE("Clark_EDrum", MIDI);
const int THRESHOLD = 100;      // minimum signal to trigger a hit
const int PEAK_SAMPLE_TIME = 10; // time (ms) to find the strongest peak
const int MASK_TIME = 50;       // debounce time (ms) to prevent double triggers

const int SNARE_PIN = 34;
const int SNARE_NOTE = 38;

// -- state variables --
unsigned long lastTriggerTime = 0;
bool isWaitingForPeak = false;
int currentPeak = 0;
unsigned long peakStartTime = 0;

// -- bluetooth variables --
bool isConnected = false;

void setup() {
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Setup callbacks to know when Android connects
  BLEMIDI.setHandleConnected([]() { 
    isConnected = true; 
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("Bluetooth Connected!");
  });
  
  BLEMIDI.setHandleDisconnected([]() { 
    isConnected = false;
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("Disconnected.");
  });

  MIDI.begin(MIDI_CHANNEL_OMNI);
  Serial.println("Waiting for Bluetooth MIDI connection...");
}

void loop() {
  MIDI.read();
  int sensorValue = analogRead(SNARE_PIN);
  unsigned long now = millis();

  // detect initial hit
  if (sensorValue > THRESHOLD && !isWaitingForPeak && (now - lastTriggerTime > MASK_TIME)) {
    isWaitingForPeak = true;
    peakStartTime = now;
    currentPeak = sensorValue;
  }

  // sampling for the peak velocity
  if (isWaitingForPeak) {
    if (sensorValue > currentPeak) {
      currentPeak = sensorValue;
    }

    // after peak period, send the MIDI signal
    if (now - peakStartTime >= PEAK_SAMPLE_TIME) {
      // map the 12-bit analog read (0-4095) to MIDI velocity (0-127)
      int velocity = map(currentPeak, THRESHOLD, 4095, 20, 127);
      if (velocity > 127) velocity = 127;

      // send the midi signal via bluetooth
      if (isConnected) {
        MIDI.sendNoteOn(SNARE_NOTE, velocity, 1);
        // delay(5); 
        // MIDI.sendNoteOff(SNARE_NOTE, 0, 1);
        Serial.printf("Sent! Hit! Velocity: %d\n", velocity);
      }
      else {
        Serial.printf("Hit! Velocity: %d\n", velocity);
      }

      // send midi to analog jack


      isWaitingForPeak = false;
      lastTriggerTime = now;
      currentPeak = 0;
    }
  }
}