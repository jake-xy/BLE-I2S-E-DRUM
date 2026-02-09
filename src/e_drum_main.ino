#include <BLEMIDI_Transport.h>
#include <hardware/BLEMIDI_ESP32.h>

// -- configuration --
BLEMIDI_CREATE_INSTANCE("Clark_EDrum", MIDI);
const int PEAK_SAMPLE_TIME = 10; // time (ms) to find the strongest peak

// -- bluetooth variables --
bool isConnected = false;

// -- classes --
class DrumPad {
  protected:
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

    virtual void hitPad(int velocity) {
      // send the midi signal via bluetooth
      sendMIDI(velocity);

      // send signal to analog jack
      sendAnalog(velocity);
    }


  public:
    DrumPad(int p, int n) : pin(p), note(n) {
      pinMode(pin, INPUT);
    }

    DrumPad(int p, int n, int t, int debounce) : pin(p), note(n), threshold(t), maskTime(debounce) {
      pinMode(pin, INPUT);
    }

    virtual void listen() {
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

        // "hit" the pad
        hitPad(velocity);

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


class HiHatPad : public DrumPad {
  private:
    const int CLOSED_NOTE = 42;
    const int OPEN_NOTE = 46;
    const int PEDAL_NOTE = 44; // the "chick" sound when you close a hi-hat
    const int SPLASH_NOTE = 46; // the splash sound when an already hit closed hi-hat is released to open
    
    const int SPLASH_WINDOW = 200; // the time (in ms) window to trigger the splash
    unsigned long lastClosedHitTime = 0;

    int pedalPin;
    int padPin;
    bool pedalClosed = false;
    int prevVelocity = 127; // pedal has no piezo so sample the pad's last velocity

    void hitPad(int velocity) override {
      // send midi signal
      if (isConnected) {
        MIDI.sendNoteOn(pedalClosed ? CLOSED_NOTE : OPEN_NOTE, velocity, 1);
        Serial.printf("Sent! %s %s Pedal Hit! Velocity: %d\n", pedalClosed ? "Closed" : "Open", name, velocity);
      }
      else Serial.printf("%s %s Pedal Hit! Velocity: %d\n", pedalClosed ? "Closed" : "Open", name, velocity);

      // send analog signal
      sendAnalog(velocity); // unimplemented for now
      
      prevVelocity = velocity;
      lastClosedHitTime = millis();
    }


  public:
    HiHatPad(int padPin, int pedalPin) : DrumPad(padPin, OPEN_NOTE) {
      this->pedalPin = pedalPin;
      pinMode(padPin, INPUT);
      pinMode(pedalPin, INPUT_PULLUP);
    }

    HiHatPad(int padPin, int pedalPin, int t, int d) : DrumPad(padPin, OPEN_NOTE, t, d) {
      this->pedalPin = pedalPin;
      pinMode(padPin, INPUT);
      pinMode(pedalPin, INPUT_PULLUP);
    }

    void listen() override {
      // handle pedal logic first
      bool pedalPressed = digitalRead(pedalPin) == LOW;
      
      // detect press down of pedal
      if (pedalPressed && !pedalClosed) {
        if (isConnected) {
          Serial.printf("Sent! %s Pedal Hit! Velocity: %d\n", name, prevVelocity);
          MIDI.sendNoteOn(PEDAL_NOTE, prevVelocity, 1);
        }
        else Serial.printf("%s Pedal Hit! Velocity: %d\n", name, prevVelocity);

        pedalClosed = true;
      }

      // detect release of pedal
      if (!pedalPressed && pedalClosed) {
        unsigned long timeSinceHit = millis() - lastClosedHitTime;
        
        // released the pedal right after hitting
        if (timeSinceHit < SPLASH_WINDOW) {
          // send midi signal
          if (isConnected) {
            MIDI.sendNoteOn(SPLASH_NOTE, prevVelocity, 1);
            Serial.printf("Sent! %s Pedal Splashed! Velocity: %d\n", name, prevVelocity);
          }
          else Serial.printf("%s Pedal Splashed! Velocity: %d\n", name, prevVelocity);
        }

        pedalClosed = false;
      }

      // listen for the hit
      DrumPad::listen();
    }
};


// create pad instruments -------------------------------------------------------------------------------------------------------------------------------------------
DrumPad snarePad(34, 38);
DrumPad bassPad(35, 36, 300, 100); // pin, midi note, threshold, debounce time
DrumPad tomPad1(36, 48);
HiHatPad hiHatPad(32, 4);

void setup() {
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  snarePad.setName("snare pad");
  bassPad.setName("bass pad");
  hiHatPad.setName("hihat pad");

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
  hiHatPad.listen();
}