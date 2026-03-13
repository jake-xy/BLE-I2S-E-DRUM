#include <BLEMIDI_Transport.h>
#include <hardware/BLEMIDI_ESP32.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>

// -- configuration --
BLEMIDI_CREATE_INSTANCE("EDrum_Ng_Mama_Mo", MIDI);
const int PEAK_SAMPLE_TIME = 10; // time (ms) to find the strongest peak
// control pins connected to S0, S1, S2, S3 pin of the mux
const int selectPins[4] = {19, 18, 5, 17};
// the analog input pin connected to SIG pin of the mux
const int sigPin = 32;
// set address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2); // connect to SDA(data) - pin 21, SCL(clock) - pin 22
// sd card module pin out
const int mosiPin = 23;
const int misoPin = 19;
const int sckPin = 18;
const int csPin = 5;
// navigation/buttons pinout
const int upButtonPin = 33;
const int okButtonPin = 27;
const int downButtonPin = 14;
const int hihatPedalPin = 4; // button to "close"/"open" the hi-hat
const int kickPedalPin = 15; // optional to use e-edal to trigger kick drum
// audio output pin (PCM5102a DAC) sck - gnd, gnd - gnd, vin - vin
const int bckPin = 26;
const int dinPin = 13;
const int lckPin = 25;

Preferences prefs;

// -- bluetooth variables --
bool isConnected = false;

// -- classes --
struct PadSettings {
  int sensitivity; // 0 - 200 (mapped inversely with the threshold value)
  int minVelocity;
  int maskTime;
  int maxThresh;
};

class DrumPad {
  protected:
    String name = "pad";
    String configKey = "pad";

    // settings
    int muxChannel;
    int note;
    int threshold = 100; // minimum signal to trigger a hit
    int maskTime = 25;  // debounce time (ms) to prevent double triggers
    int minVel = 45;
    int maxThresh = 1250;
    int sensitivity = 180;
    const int MIN_THRESH_FLOOR = 10;

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

    void selectMuxChannel(int channel) {
      for (int i = 0; i < 4; i++) {
        // read the bits of the channel number and set the pins accordingly
        int bitValue = bitRead(channel, i);
        digitalWrite(selectPins[i], bitValue);
      }
      // a few microsecond delay for the multiplexer switch to physically connect
      delayMicroseconds(50);

      // dummy read to flush the internal capacitros of the mcu
      analogRead(sigPin);
      analogRead(sigPin);
    }

    void updateThresholdFromSensitivity() {
      threshold = map(sensitivity, 0, 200, maxThresh, MIN_THRESH_FLOOR);

      if (threshold < MIN_THRESH_FLOOR) threshold = MIN_THRESH_FLOOR;
      if (threshold > maxThresh) threshold = maxThresh;
    }


  public:
    DrumPad(int c, int n) : muxChannel(c), note(n) {
      // pinMode(muxChannel, INPUT);
    }

    DrumPad(int c, int n, int t, int debounce) : muxChannel(c), note(n), threshold(t), maskTime(debounce) {
      // pinMode(muxChannel, INPUT);
    }

    virtual void listen() {
      // select the channel on the multiplexer
      selectMuxChannel(muxChannel);
      
      // read the analog value from the selected channel
      int sensorValue = analogRead(sigPin);
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
        int velocity = map(currentPeak, threshold, maxThresh, minVel, 127);
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
      if (maskTime < 5) maskTime = 5;
    }

    void setName(String name) {
      this->name = name;
    }

    void setMinVel(int vel) {
      minVel = vel;
      if (minVel > 127) minVel = 127;
    }

    void setMaxThreshold(int threshold) {
      maxThresh = threshold;
      if (maxThresh < threshold+10) maxThresh = threshold + 10;
      else if (maxThresh > 4095) maxThresh = 4095;
    }

    void setSensitivity(int newSens) {
      sensitivity = newSens;
      if (sensitivity < 10) sensitivity = 10;
      else if (sensitivity > 200) sensitivity = 200;
      updateThresholdFromSensitivity();
    }

    int getSensitivity() {
      return sensitivity;
    }
    
    int getMinVelocity() {
      return minVel;
    }

    int getMaskTime() {
      return maskTime;
    }

    int getMaxThresh() {
      return maxThresh;
    }

    void loadOrSaveConfig(String key) {
      configKey = key;

      PadSettings cfg = {sensitivity, minVel, maskTime, maxThresh};

      prefs.begin("edrum", false);

      // check if a saved record exists
      if (prefs.getBytesLength(configKey.c_str()) == sizeof(PadSettings)) {
        prefs.getBytes(configKey.c_str(), &cfg, sizeof(PadSettings));

        sensitivity = cfg.sensitivity;
        minVel = cfg.minVelocity;
        maskTime = cfg.maskTime;
        maxThresh = cfg.maxThresh;

        Serial.println("Loaded saved config for: " + configKey);
      }
      // no record yet, so save
      else {
        prefs.putBytes(configKey.c_str(), &cfg, sizeof(PadSettings));
        Serial.println("Saved defaults for: " + configKey);
      }

      prefs.end();

      // calculate threshold based on sensitivity
      updateThresholdFromSensitivity();
    }

    void saveNewConfig() {
      PadSettings cfg = {sensitivity, minVel, maskTime, maxThresh};

      prefs.begin("edrum", false);
      prefs.putBytes(configKey.c_str(), &cfg, sizeof(PadSettings));
      prefs.end();

      updateThresholdFromSensitivity();
      Serial.println("Updated config for: " + configKey);
    }
};


class HiHatPad : public DrumPad {
  private:
    static const int CLOSED_NOTE = 42;
    static const int OPEN_NOTE = 46;
    static const int PEDAL_NOTE = 44; // the "chick" sound when you close a hi-hat
    static const int SPLASH_NOTE = 46; // the splash sound when an already hit closed hi-hat is released to open
    
    const int SPLASH_WINDOW = 200; // the time (in ms) window to trigger the splash
    unsigned long lastClosedHitTime = 0;
    unsigned long lastPedalTriggerTime = 0; // implementation of debounce time

    int pedalPin;
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
    HiHatPad(int c, int pedalPin) : DrumPad(c, OPEN_NOTE) {
      this->pedalPin = pedalPin;
      // pinMode(c, INPUT);
      pinMode(pedalPin, INPUT_PULLUP);
    }

    HiHatPad(int c, int pedalPin, int t, int d) : DrumPad(c, OPEN_NOTE, t, d) {
      this->pedalPin = pedalPin;
      // pinMode(c, INPUT);
      pinMode(pedalPin, INPUT_PULLUP);
    }

    void listen() override {
      // handle pedal logic first
      bool pressingPedal = digitalRead(pedalPin) == LOW;
      
      // detect press down of pedal
      if (pressingPedal && !pedalClosed && (millis() - lastPedalTriggerTime > maskTime)) {
        if (isConnected) {
          Serial.printf("Sent! %s Pedal Hit! Velocity: %d\n", name, prevVelocity);
          MIDI.sendNoteOn(PEDAL_NOTE, prevVelocity, 1);
        }
        else Serial.printf("%s Pedal Hit! Velocity: %d\n", name, prevVelocity);
        
        lastPedalTriggerTime = millis();
        pedalClosed = true;
      }

      // detect release of pedal
      if (!pressingPedal && pedalClosed && (millis() - lastPedalTriggerTime > maskTime)) {
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

        lastPedalTriggerTime = millis();
        pedalClosed = false;
      }

      // listen for the hit
      DrumPad::listen();
    }
};


class BassPad : public DrumPad {
  private:
    int pedalPin;
    bool pedalReleased = true;
  
  public:
    BassPad(int c, int pedalPin, int midiNote) : DrumPad(c, midiNote) {
      this->pedalPin = pedalPin;
      pinMode(pedalPin, INPUT_PULLUP);
    }

    BassPad(int c, int pedalPin, int midiNote, int t, int d) : DrumPad(c, midiNote, t, d) {
      this->pedalPin = pedalPin;
      // pinMode(c, INPUT);
      pinMode(pedalPin, INPUT_PULLUP);
    }

    void listen() override {
      // handle pedal logic first
      bool pressingPedal = digitalRead(pedalPin) == LOW;

      if (pedalReleased && pressingPedal && (millis() - lastTriggerTime > maskTime)) {
        // "hit" the pad
        hitPad(127);
        // update states
        pedalReleased = false;
        lastTriggerTime = millis();
      }
      else if (!pedalReleased && !pressingPedal) {
        pedalReleased = true;
      }

      // listen for hit on the actual drum pad
      DrumPad::listen();
    }
};

// create pad instruments -------------------------------------------------------------------------------------------------------------------------------------------
HiHatPad hiHatPad(0, hihatPedalPin, 400, 25); // mux channel, pedal pin, threshold, debounce time
DrumPad snarePad(1, 38, 40, 25); // mux channel, midi note, threshold, debounce time
DrumPad tomPad1(2, 48, 60, 25);
DrumPad tomPad2(3, 45);
DrumPad tomPad3(4, 43, 20, 25);
BassPad bassPad(5, kickPedalPin, 36);
DrumPad crashPad(6, 49, 400, 25);
DrumPad ridePad(7, 53, 400, 25);


// lcd menu display -------------------------------------------------------------------------------------------------------------------------------------------------
enum DecimalPlace {ONES, TENTHS, HUNDREDTHS, THOUSANDTHS};
String cfgNames[] = {"cfg_snarePad", "cfg_hiHatPad", "cfg_tomPad1", "cfg_tomPad2", "cfg_tomPad3","cfg_bassPad", "cfg_crashPad", "cfg_ridePad"};

class LCD_Menu {
  private:
    unsigned long timeSinceBlink = 0;
    unsigned long timeSinceLastAction = 0;
    bool blinkBit = false;
    bool isIdle = false;
    
    int currentMenu = MAIN_MENU;
    int prevMenu = -1;
    int menuIndex = 0; // specifies which index is being selected
    int menuRow = 0; // specifies which row is selected (1 or 2 from the physical display)
    int prevMenuIndex = 0; // to save state for convenience of use (UI/UX baby)
    int prevMenuRow = 0;
    bool pressedUp = false;
    bool pressedDown = false;
    bool pressedOk = false;
    unsigned long pressedOkLT = 0;

    String selectedKit = "none"; // to be implemented
    DecimalPlace selectedDP = ONES;
    String editingValueStr = "";
    int editingValue = -1;

    DrumPad* getPadByMenuIndex(int index) {
      switch (index) {
        case 1: return &hiHatPad;
        case 2: return &snarePad;
        case 3: return &tomPad1;
        case 4: return &tomPad2;
        case 5: return &tomPad3;
        case 6: return &bassPad;
        case 7: return &crashPad;
        case 8: return &ridePad;
        default: return nullptr;
      }
    }

    int getOffsetFromDP(DecimalPlace dp) {
      switch (selectedDP) {
        case ONES: return 0;
        case TENTHS: return 1;
        case HUNDREDTHS: return 2;
        case THOUSANDTHS: return 3;
      }
      return -1;
    }

    public:
      inline static const int MAIN_MENU = 0;
      inline static const int KIT_SELECT = 1;
      inline static const int PAD_EDIT = 2;
      inline static const int VALUE_EDIT = 3;

      inline static String menu[][9] = {
        {
          "- KIT: CLASSIC",
          "- SET: HI-HAT",
          "- SET: SNARE",
          "- SET: TOM-TOM 1",
          "- SET: TOM-TOM 2",
          "- SET: TOM-TOM 3",
          "- SET: BASS",
          "- SET: CRASH",
          "- SET: RIDE",
        },
        {
          "- BLUES",
          "- BROOKLYN"
        },
        {
          "-SENSITIVITY",
          "-MIN VELOCITY",
          "-MASK TIME",
          "-MAX THRESH"
        },
        {
          "SENSITIVITY: 200",
          "NEW VALUE  : 095"
        }
      };

    LCD_Menu() {

    }

    void begin() {
      lcd.init();
      lcd.backlight();

      pinMode(upButtonPin, INPUT_PULLUP);
      pinMode(okButtonPin, INPUT_PULLUP);
      pinMode(downButtonPin, INPUT_PULLUP);

      changeText(menu[0][0], 0, 0);
      changeText(menu[0][1], 1, 0);
    }

    void changeText(String text, int row, int col) {
      lcd.setCursor(0, row);
      lcd.print("                ");
      lcd.setCursor(col, row);
      lcd.print(text);
    }

    void navigateDisplay(int dir) {
      if (currentMenu != VALUE_EDIT) {
        // handle menu index and menu row
        int maxIndex = 0;
        for (int i = 0; i < sizeof(menu[currentMenu]) / sizeof(menu[currentMenu][0]); i++) {
          if (menu[currentMenu][i].length() == 0) break;
          maxIndex += 1;
        }
        maxIndex -= 1;

        if (menuIndex + dir < 0) return;
        else if (menuIndex + dir > maxIndex) return;
        menuIndex += dir;

        if (menuRow + dir == 1) menuRow = 1;
        else if (menuRow + dir == 0) menuRow = 0;

        // print the appropriate display
        if (menuRow == 1) {
          changeText(menu[currentMenu][menuIndex-1], 0, 0);
          changeText(menu[currentMenu][menuIndex], 1, 0);
        }
        else if (menuRow == 0) {
          changeText(menu[currentMenu][menuIndex], 0, 0);
          changeText(menu[currentMenu][menuIndex+1], 1, 0);
        }

        blinkBit = false; // for immediate animation

        return;
      }

      // -- handle value edit -----
      editingValue += dir*-1; // -1 to reverse direction (this orientation is more intuitive)
      if (editingValue < 0) editingValue = 9;
      else if (editingValue > 9) editingValue = 0;
      // actually modify the string
      int offset = getOffsetFromDP(selectedDP);
      editingValueStr[editingValueStr.length()-1 - offset] = editingValue + '0';
      // also update the screen
      menu[VALUE_EDIT][1][menu[VALUE_EDIT][1].length()-1 - offset] = editingValue + '0';
    }

    void run() {
      bool pressingUp = digitalRead(upButtonPin) == LOW;
      bool pressingOk = digitalRead(okButtonPin) == LOW;
      bool pressingDown = digitalRead(downButtonPin) == LOW;

      if (isIdle) {
        // handle any action to get out of idle
        if (pressingUp || pressingDown || pressingOk) {
          isIdle = false;
          lcd.backlight();
          timeSinceLastAction = millis();
        }
        return;
      }
      else if (!isIdle && millis() - timeSinceLastAction >= 15000) { // 15 seconds to idle
        lcd.noBacklight();
        isIdle = true;
      }

      // -- up button --------------------------
      if (pressingUp && !pressedUp) {
        pressedUp = true;
      }
      else if (!pressingUp && pressedUp) {
        // do press up logic
        navigateDisplay(-1);
        pressedUp = false;
        timeSinceLastAction = millis();
      }

      // -- ok/select button ----------------------------
      if (pressingOk) {
        if (!pressedOk) {
          pressedOkLT = millis();
          pressedOk = true;
        }
        // handle "return" action (return is long press of ok/select button)
        else if (pressedOk && prevMenu != -1 && millis() - pressedOkLT >= 1000) {
          if (millis() - pressedOkLT < 1025) {
            changeText("   RELEASE TO   ", 0, 0); // clear the screen to let user know their long press is acknowledged
            changeText("     RETURN     ", 1, 0);
          }
        }
      }
      // button is released
      else if (!pressingOk && pressedOk) {
        // after pressing for a short period
        if (millis() - pressedOkLT < 1000) {
          // handle ok logic
          if (currentMenu == MAIN_MENU) {
            // display appropriate values if entering pad_edit screen
            if (menuIndex != 0) {
              DrumPad *pad = getPadByMenuIndex(menuIndex);
              String value;
              // sensitivity
              value = String(pad->getSensitivity());
              switch (value.length()) {
                case 1: value = "00" + value; break;
                case 2: value = "0" + value; break;
              } // clamps string value to 3 decimal places
              menu[PAD_EDIT][0] = "-SENSITIVITY " + value;
              
              // minimum velocity
              value = String(pad->getMinVelocity());
              menu[PAD_EDIT][1] = "-MIN VELOCITY " + (value.length() == 1 ? "0"+value : value); // clamps string value to 2 decimal places
              
              // mask time (debounce time)
              value = String(pad->getMaskTime());
              switch (value.length()) {
                case 1: value = "00" + value; break;
                case 2: value = "0" + value; break;
              } // clamps string value to 3 decimal places
              menu[PAD_EDIT][2] = "-MASK TIME " + value;
              
              // maximum threshold
              value = String(pad->getMaxThresh());
              switch (value.length()) {
                case 1: value = "000" + value; break;
                case 2: value = "00" + value; break;
                case 3: value = "0" + value; break;
              } // clamps string value to 4 decimal places
              menu[PAD_EDIT][3] = "-MAX THRESH " + value;
            }

            // actually switch screen
            prevMenu = currentMenu;
            prevMenuIndex = menuIndex;
            prevMenuRow = menuRow;
            currentMenu = menuIndex == 0 ? KIT_SELECT : PAD_EDIT;
            menuIndex = 0;
            menuRow = 0;
            navigateDisplay(0);
          }
          else if (currentMenu == KIT_SELECT) {
            // do kit selection here
          }
          else if (currentMenu == PAD_EDIT) {
            // display appropriate value when entering value edit screen
            DrumPad *pad = getPadByMenuIndex(prevMenuIndex);
            switch (menuIndex) {
              case 0:
                editingValueStr = String(pad->getSensitivity());
                switch (editingValueStr.length()) { // forces to 3 decimal places
                  case 1: editingValueStr = "00" + editingValueStr; break;
                  case 2: editingValueStr = "0" + editingValueStr; break;
                }
                menu[VALUE_EDIT][0] = "SENSITIVITY: " + editingValueStr;
                menu[VALUE_EDIT][1] = "NEW VALUE  : " + editingValueStr;
                break;
              case 1:
                editingValueStr = String(pad->getMinVelocity());
                editingValueStr = (editingValueStr.length() == 1 ? "0" : "") + editingValueStr; // forces 2 decimal places
                menu[VALUE_EDIT][0] = "MIN VELOCITY: " + editingValueStr;
                menu[VALUE_EDIT][1] = "NEW VALUE   : " + editingValueStr;
                break;
              case 2:
                editingValueStr = String(pad->getMaskTime());
                switch (editingValueStr.length()) { // forces to 3 decimal places
                  case 1: editingValueStr = "00" + editingValueStr; break;
                  case 2: editingValueStr = "0" + editingValueStr; break;
                }
                menu[VALUE_EDIT][0] = "MASK TIME : " + editingValueStr;
                menu[VALUE_EDIT][1] = "NEW VALUE : " + editingValueStr;
                break;
              case 3:
                editingValueStr = String(pad->getMaxThresh());
                switch (editingValueStr.length()) { // forces to 3 decimal places
                  case 1: editingValueStr = "000" + editingValueStr; break;
                  case 2: editingValueStr = "00" + editingValueStr; break;
                  case 3: editingValueStr = "0" + editingValueStr; break;
                }
                menu[VALUE_EDIT][0] = "MAX THRESH: " + editingValueStr;
                menu[VALUE_EDIT][1] = "NEW VALUE : " + editingValueStr;
                break;
            }

            // switch menu
            currentMenu = VALUE_EDIT;
            selectedDP = ONES;
            editingValue = editingValueStr[editingValueStr.length()-1 - getOffsetFromDP(selectedDP)] - '0';

            changeText(menu[currentMenu][0], 0, 0);
            changeText(menu[currentMenu][1], 1, 0);
          }
          else if (currentMenu == VALUE_EDIT) {
            // change decimal places
            switch (selectedDP) {
              case ONES: selectedDP = TENTHS; break;
              case TENTHS:
                selectedDP = editingValueStr.length() < 3 ? ONES : HUNDREDTHS;
                break;
              case HUNDREDTHS: 
                selectedDP = editingValueStr.length() < 4 ? ONES : THOUSANDTHS; 
                break;
              case THOUSANDTHS: selectedDP = ONES; break;
            }

            editingValue = editingValueStr[editingValueStr.length()-1 - getOffsetFromDP(selectedDP)] - '0';

            // update display
            changeText(menu[currentMenu][0], 0, 0);
            changeText(menu[currentMenu][1], 1, 0);
          }
        }
        // button is released after a long press
        else if (millis() - pressedOkLT >= 1000){
          // going back from value edit menu
          if (currentMenu == VALUE_EDIT)  {
            // save the edited value here
            DrumPad *pad = getPadByMenuIndex(prevMenuIndex);
            switch (menuIndex) {
              case 0: 
                pad->setSensitivity(editingValueStr.toInt());
                menu[PAD_EDIT][0] = "-SENSITIVITY " + String(pad->getSensitivity());
                break;
              case 1: 
                pad->setMinVel(editingValueStr.toInt()); 
                menu[PAD_EDIT][1] = "-MIN VELOCITY " + String(pad->getMinVelocity());
                break;
              case 2: 
                pad->setDebounceTime(editingValueStr.toInt()); 
                menu[PAD_EDIT][2] = "-MASK TIME " + String(pad->getMaskTime());
                break;
              case 3: 
                pad->setMaxThreshold(editingValueStr.toInt()); 
                menu[PAD_EDIT][3] = "-MAX THRESH " + String(pad->getMaxThresh());
                break;
            }

            // actually change screen or "go back"
            currentMenu = PAD_EDIT;
            navigateDisplay(0);
          }
          else if (currentMenu != MAIN_MENU){
            // if returning from pad edit screen
            if (currentMenu == PAD_EDIT) {
              // save the modified values to the config
              DrumPad *pad = getPadByMenuIndex(prevMenuIndex);
              pad->saveNewConfig();
              Serial.println("I am being saved");
            }
            currentMenu = prevMenu;
            menuIndex = prevMenuIndex;
            menuRow = prevMenuRow;
  
            prevMenu = -1;
            prevMenuIndex = -1;
            prevMenuRow = -1;
  
            navigateDisplay(0);
          }
        }
        timeSinceLastAction = millis();
        pressedOkLT = millis();
        pressedOk = false;
      }

      // -- down button ----------------------------
      if (pressingDown && !pressedDown) {
        pressedDown = true;
      }
      else if (!pressingDown && pressedDown) {
        // do press down logic
        navigateDisplay(1);
        pressedDown = false;
        timeSinceLastAction = millis();
      }

      // -- handle selection blinking ----------------------
      if (!pressingOk && millis() - timeSinceBlink > (currentMenu == VALUE_EDIT ? 250 : 500)) {
        if (currentMenu != VALUE_EDIT) {
          lcd.setCursor(0, menuRow);
          lcd.print(blinkBit ? '-' : '>');
        }
        else {
          // blink the place holder
          int offset = getOffsetFromDP(selectedDP);
          
          // get selected col
          int col = menu[VALUE_EDIT][1].length()-1-offset;
          lcd.setCursor(col, 1);
          lcd.print(blinkBit ? editingValueStr[editingValueStr.length()-1-offset] : ' ');
        }
        
        blinkBit = !blinkBit;
        timeSinceBlink = millis();
      }
    }
};

LCD_Menu lcdMenu;

void setup() {
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // set the control pins as outputs
  for (int i = 0; i < 4; i++) {
    pinMode(selectPins[i], OUTPUT);
    digitalWrite(selectPins[i], LOW);
  }

  snarePad.setName("snare pad");
  hiHatPad.setName("hihat pad");
  tomPad1.setName("tom pad 1");
  tomPad2.setName("tom pad 2");
  tomPad3.setName("tom pad 3");
  bassPad.setName("bass pad");
  crashPad.setName("crash pad");
  ridePad.setName("ride pad");

  hiHatPad.setSensitivity(140);
  crashPad.setSensitivity(140);
  ridePad.setSensitivity(140);

  snarePad.loadOrSaveConfig("cfg_snarePad");
  hiHatPad.loadOrSaveConfig("cfg_hiHatPad");
  tomPad1.loadOrSaveConfig("cfg_tomPad1");
  tomPad2.loadOrSaveConfig("cfg_tomPad2");
  tomPad3.loadOrSaveConfig("cfg_tomPad3");
  bassPad.loadOrSaveConfig("cfg_bassPad");
  crashPad.loadOrSaveConfig("cfg_crashPad");
  ridePad.loadOrSaveConfig("cfg_ridePad");

  // setup callbacks to know when Android connects
  BLEMIDI.setHandleConnected([]() { 
    isConnected = true;
    LCD_Menu::menu[0][0] = "- KIT: BLTH MODE";
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

  // lcd menu
  lcdMenu.begin();
}

void loop() {
  MIDI.read();
  snarePad.listen();
  bassPad.listen();
  hiHatPad.listen();
  tomPad1.listen();
  tomPad2.listen();
  tomPad3.listen();
  crashPad.listen();
  ridePad.listen();

  lcdMenu.run();
}