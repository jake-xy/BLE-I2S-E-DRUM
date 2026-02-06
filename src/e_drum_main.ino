#include <BLEMIDI_Transport.h>
#include <hardware/BLEMIDI_ESP32.h>

// -- configuration --
BLEMIDI_CREATE_INSTANCE("Clark_EDrum", MIDI);
const int PEAK_SAMPLE_TIME = 10; // time (ms) to find the strongest peak

// -- bluetooth variables --
bool isConnected = false;

// -- classes --
class DrumPad {
  private:
    String name = "pad";

    // settings
    int pin;
    int note;
    int threshold = 100; // minimum signal to trigger a hit
    int maskTime = 50;  // debounce time (ms) to prevent double triggers

    // state variables
    unsigned long lastTriggerTime = 0;
    bool isWaitingForPeak = false;
    int currentPeak = 0;
    unsigned long peakStartTime = 0;

    void sendMIDI(int velocity) {
      if (!isConnected) {
        Serial.printf("%s Hit! Velocity: %d\n", name, velocity);
        return;
      }

      MIDI.sendNoteOn(note, velocity, 1);
      // delay(5); 
      // MIDI.sendNoteOff(SNARE_NOTE, 0, 1);
      Serial.printf("Sent! %s Hit! Velocity: %d\n", name, velocity);
    }

    void sendAnalog(int velocity) {

    }


  public:
    DrumPad(int p, int n) : pin(p), note(n) {
      pinMode(pin, INPUT);
    }

    DrumPad(int p, int n, int t, int debounce) : pin(p), note(n), threshold(t), maskTime(debounce) {
      pinMode(pin, INPUT);
    }

    void listen() {
      int sensorValue = analogRead(pin);
      unsigned long now = millis();

      // detect initial hit
      if (sensorValue > threshold && !isWaitingForPeak && (now - lastTriggerTime > maskTime)) {
        isWaitingForPeak = true;
        peakStartTime = now;
        currentPeak = sensorValue;
      }

      // sampling for the peak velocity
      if (!isWaitingForPeak) return;

      if (sensorValue > currentPeak) {
        currentPeak = sensorValue;
      }

      // after peak period, send the MIDI signal
      if (now - peakStartTime >= PEAK_SAMPLE_TIME) {
        // map the 12-bit analog read (0-4095) to MIDI velocity (0-127)
        int velocity = map(currentPeak, threshold, 4095, 20, 127);
        if (velocity > 127) velocity = 127;

        // send the midi signal via bluetooth
        sendMIDI(velocity);

        // send signal to analog jack
        sendAnalog(velocity);

        isWaitingForPeak = false;
        lastTriggerTime = now;
        currentPeak = 0;
      }
    }

    void setThreshold(int threshold) {
      this->threshold = threshold;
    }

    void setDebounceTime(int time) {
      maskTime = time;
    }

    void setName(String name) {
      this->name = name;
    }
};


// create pad instruments -------------------------------------------------------------------------------------------------------------------------------------------
DrumPad snarePad(34, 38);
DrumPad bassPad(35, 36, 300, 100); // pin, midi note, threshold, debounce time
DrumPad hihatClosePad(32, 42, 50, 80);
DrumPad hihatOpenPad(33, 46, 50, 80);
DrumPad tomPad1(36, 48);

void setup() {
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  snarePad.setName("snare pad");
  bassPad.setName("bass pad");
  hihatClosePad.setName("hihat pad");

  // setup callbacks to know when Android connects
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
  snarePad.listen();
  bassPad.listen();
  hihatClosePad.listen();
  // hihatOpenPad.listen();
  // tomPad1.listen();
}