/**
 * MIT Emergency Ventilator Controller
 * 
 * MIT License:
 * 
 * Copyright (c) 2020 MIT
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * Alarms.h
 * Defines classes for managing alarms, displaying alarm information on the display,
 * and playing tones corresponding to different alarm levels.
 */

#ifndef Alarms_h
#define Alarms_h

#include "Arduino.h"

#include "Buttons.h"
#include "Display.h"
#include "pitches.h"


namespace alarms {


using display::Display;


// Alarm levels in order of increasing priority
enum AlarmLevel {
  NO_ALARM,
  NOTIFY,
  EMERGENCY,
  OFF_LEVEL,
  NUM_LEVELS
};


// Container for notes elements
struct Note {
  int note;
  int duration;
  int pause;
};


// Notifiation notes
static const Note kNotifyNotes[] = {
  {NOTE_B4, 200, 100},
  {NOTE_B4, 200, 2000}
};

// Emergency notes
static const Note kEmergencyNotes[] = {
  {NOTE_G4, 300, 200},
  {NOTE_G4, 300, 200},
  {NOTE_G4, 300, 400},
  {NOTE_G4, 200, 100},
  {NOTE_G5, 200, 1500}
};

// Notifiation notes
static const Note kOffNotes[] = {
  {NOTE_G4, 200, 200}
};


/**
 * Tone
 * A sequence of notes that can be played.
 */
class Tone {
public:
  Tone(): length_(0) {}

  Tone(const Note notes[], const int& notes_length, const int* pin);

  // Play the tone, if any
  void play();

  // Stop playing
  inline void stop() { playing_ = false; }

private:
  Note* notes_;
  int length_;
  int* pin_;
  bool playing_ = false;
  int tone_step_;
  unsigned long tone_timer_ = 0;
};


/**
 * Beeper
 * Represents the alarm speaker/buzzer. Handles playing of tones and snoozing.
 */
class Beeper {

  // Time during which alarms are silenced, in milliseconds
  static const unsigned long kSnoozeTime = 2 * 60 * 1000UL;

public:
  Beeper(const int& beeper_pin, const int& snooze_pin):
    beeper_pin_(beeper_pin), 
    snooze_button_(snooze_pin) {
      const int notify_notes_length = sizeof(kNotifyNotes) / sizeof(kNotifyNotes[0]);
      tones_[NOTIFY] = Tone(kNotifyNotes, notify_notes_length, &beeper_pin_);

      const int emergency_notes_length = sizeof(kEmergencyNotes) / sizeof(kEmergencyNotes[0]);
      tones_[EMERGENCY] = Tone(kEmergencyNotes, emergency_notes_length, &beeper_pin_);

      const int off_notes_length = sizeof(kOffNotes) / sizeof(kOffNotes[0]);
      tones_[OFF_LEVEL] = Tone(kOffNotes, off_notes_length, &beeper_pin_);
    }

  // Setup during arduino setup()
  void begin();

  // Update during arduino loop()
  void update(const AlarmLevel& alarm_level);

  // Get snooze time remaining
  unsigned long getRemainingSnoozeTime();

private:
  const int beeper_pin_;
  buttons::DebouncedButton snooze_button_;
  Tone tones_[NUM_LEVELS];

  unsigned long snooze_time_ = 0;
  unsigned long timeRemaining_ = 0;
  bool snoozed_ = false;

  bool snoozeButtonPressed() const;

  void toggleSnooze();

  void play(const AlarmLevel& alarm_level);

  void stop();

};


/** 
 * Alarm
 * Keeps track of the state of a specific alarm.
 */
class Alarm {
public:
  Alarm() {};
  
  Alarm(const String& default_text, const int& min_bad_to_trigger,
        const int& min_good_to_clear, const AlarmLevel& alarm_level);

  // Reset to default state
  void reset();

  // Set the ON value of this alarm, but only turn ON if `bad == true` for at least 
  // `min_bad_to_trigger_` consecutive calls with different `seq` and OFF if `bad == false` 
  // for at least `min_good_to_clear_` consecutive calls with different `seq`.   
  void setCondition(const bool& bad, const unsigned long& seq);

  // Set the alarm text (trim or pad to footer width)
  void setText(const String& text);

  // Check if this alarm is on
  inline const bool& isON() const { return on_; }

  // Get the text of this alarm
  inline String text() const { return text_; }

  // Get the alarm level of this alarm
  inline AlarmLevel alarmLevel() const { return alarm_level_; }

private:
  String text_;
  AlarmLevel alarm_level_;
  int min_bad_to_trigger_;
  int min_good_to_clear_;
  bool on_ = false;
  unsigned long consecutive_bad_ = 0;
  unsigned long consecutive_good_ = 0;
  unsigned long last_bad_seq_ = 0;
  unsigned long last_good_seq_ = 0;
};


/**
 * AlarmManager
 * Manages multple alarms on the same screen space.
 * If there is one alarm on, its text blinks in a designated portion of the screen,
 * if there are more, each one blinks for `kDisplayTime` milliseconds at a time.
 */
class AlarmManager {

  // Time each alarm is displayed if multiple, in milliseconds
  static const unsigned long kDisplayTime = 2 * 1000UL;

  // Indices for the different alarms
  enum Indices {
    HIGH_PRESSU = 0,
    LOW_PRESSUR,
    BAD_PLATEAU,
    UNMET_VOLUM,
    NO_TIDAL_PR,
    OVER_CURREN,
    MECH_FAILUR,
    NOT_CONFIRM_TV,
    NOT_CONFIRM_RR,
    NOT_CONFIRM_IE,
    NOT_CONFIRM_AC,
    TURNING_OFF,
    NUM_ALARMS 
  };

public:
  AlarmManager(const int& beeper_pin, const int& snooze_pin, const int& led_pin,
               Display* displ, unsigned long const* cycle_count):
      displ_(displ),
      beeper_(beeper_pin, snooze_pin),
      led_pin_(led_pin),
      led_pulse_(500, 0.5),
      cycle_count_(cycle_count) {
    alarms_[HIGH_PRESSU] = Alarm("HIGH PRESSURE       ", 1, 2, EMERGENCY);
    alarms_[LOW_PRESSUR] = Alarm("LOW PRES DISCONNECT?", 1, 1, EMERGENCY);
    alarms_[BAD_PLATEAU] = Alarm("HIGH RESIST PRES    ", 1, 1, NOTIFY);
    alarms_[UNMET_VOLUM] = Alarm("UNMET TIDAL VOLUME  ", 1, 1, EMERGENCY);
    alarms_[NO_TIDAL_PR] = Alarm("NO TIDAL PRESSURE   ", 2, 1, EMERGENCY);
    alarms_[OVER_CURREN] = Alarm("OVER CURRENT FAULT  ", 1, 2, EMERGENCY);
    alarms_[MECH_FAILUR] = Alarm("MECHANICAL FAILURE  ", 1, 1, EMERGENCY);
    alarms_[NOT_CONFIRM_TV] = Alarm("CONFIRM?            ", 1, 1, NOTIFY);
    alarms_[NOT_CONFIRM_RR] = Alarm("CONFIRM?            ", 1, 1, NOTIFY);
    alarms_[NOT_CONFIRM_IE] = Alarm("CONFIRM?            ", 1, 1, NOTIFY);
    alarms_[NOT_CONFIRM_AC] = Alarm("CONFIRM?            ", 1, 1, NOTIFY);
    alarms_[TURNING_OFF] = Alarm("TURNING OFF         ", 1, 1, OFF_LEVEL);
  }

  // Setup during arduino setup()
  void begin();

  // Update alarms, should be called every loop
  void update();

  // Clear all alarms
  void allOff();

  // Pressure too high alarm
  inline void highPressure(const bool& value) {
    alarms_[HIGH_PRESSU].setCondition(value, *cycle_count_);
  }

  // Pressure too low alarm
  inline void lowPressure(const bool& value) {
    alarms_[LOW_PRESSUR].setCondition(value, *cycle_count_);
  }

  // Bad plateau alarm
  inline void badPlateau(const bool& value) {
    alarms_[BAD_PLATEAU].setCondition(value, *cycle_count_);
  }

  // Tidal volume not met alarm
  inline void unmetVolume(const bool& value) {
    alarms_[UNMET_VOLUM].setCondition(value, *cycle_count_);
  }

  // Tidal pressure not detected alarm
  inline void noTidalPres(const bool& value) {
    alarms_[NO_TIDAL_PR].setCondition(value, *cycle_count_);
  }

  // Current too high alarm
  inline void overCurrent(const bool& value) {
    alarms_[OVER_CURREN].setCondition(value, *cycle_count_);
  }

  // Mechanical Failure alarm
  inline void mechanicalFailure(const bool& value) {
    alarms_[MECH_FAILUR].setCondition(value, *cycle_count_);
  }

  // Setting not confirmed
  inline void unconfirmedChange(const bool& value, const String& message = "", const display::DisplayKey& key = 0) {
    Indices NOT_CONFIRM;
    if (key == display::DisplayKey::VOLUME)      { NOT_CONFIRM = NOT_CONFIRM_TV;}
    if (key == display::DisplayKey::BPM)         { NOT_CONFIRM = NOT_CONFIRM_RR;}
    if (key == display::DisplayKey::IE_RATIO)    { NOT_CONFIRM = NOT_CONFIRM_IE;}
    if (key == display::DisplayKey::AC_TRIGGER)  { NOT_CONFIRM = NOT_CONFIRM_AC;}
    if (value) {      
      alarms_[NOT_CONFIRM].setText(message);
    }
    alarms_[NOT_CONFIRM].setCondition(value, *cycle_count_);
  }

  inline void turningOFF(const bool& value) {
    alarms_[TURNING_OFF].setCondition(value, *cycle_count_);
  }

  // Get current state of each alarm
  inline const bool& getHighPressure()      { return alarms_[HIGH_PRESSU].isON(); }
  inline const bool& getLowPressure()       { return alarms_[LOW_PRESSUR].isON(); }
  inline const bool& getBadPlateau()        { return alarms_[BAD_PLATEAU].isON(); }
  inline const bool& getUnmetVolume()       { return alarms_[UNMET_VOLUM].isON(); }
  inline const bool& getNoTidalPres()       { return alarms_[NO_TIDAL_PR].isON(); }
  inline const bool& getOverCurrent()       { return alarms_[OVER_CURREN].isON(); }
  inline const bool& getMechanicalFailure() { return alarms_[MECH_FAILUR].isON(); }
  //inline const bool& getUnconfirmedChange() { return alarms_[NOT_CONFIRM].isON(); }
  inline const bool& getTurningOFF()        { return alarms_[TURNING_OFF].isON(); }

private:
  Display* displ_;
  Beeper beeper_;
  int led_pin_;
  utils::Pulse led_pulse_;
  Alarm alarms_[NUM_ALARMS];
  Alarm alarmsHeader_[NUM_ALARMS-4];
  Alarm alarmsFooter_[4];
  unsigned long const* cycle_count_;

  // Get number of alarms that are ON
  int numON() const;

  // Get number of knob confirm alarms that are ON
  int numON_Confirm() const;

  // Get number of nonconfirm alarms that are ON
  int numON_NonConfirm() const;

  // Get header text to display
  String getHeaderText() const;

  // Get footer text to display
  String getFooterText() const;

  // Get highest priority level of the alarms that are ON
  AlarmLevel getHighestLevel() const;
};


} // namespace alarms


#endif
