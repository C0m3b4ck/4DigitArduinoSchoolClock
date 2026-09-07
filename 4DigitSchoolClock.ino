/* **********************************************************************
 * Four-Digit Seven-Segment Word, Clock, and Lesson Display
 *
 * Buttons:
 *
 *   D10 = DOWN
 *   D3  = UP
 *   D13 = SUBMIT
 *
 * Button wiring:
 *
 *   Arduino pin -> button -> GND
 *
 *
 * Startup:
 *
 *   1. Select day of week with UP/DOWN
 *   2. Press SUBMIT
 *   3. Select hour with UP/DOWN
 *   4. Press SUBMIT
 *   5. Select minute with UP/DOWN
 *   6. Press SUBMIT
 *
 *
 * Normal operation:
 *
 *   SUBMIT cycles through:
 *
 *     1. Current time
 *     2. Next lesson name
 *     3. Next lesson start time
 *
 *   UP/DOWN enter edit mode to change day, hour, minute.
 *   In edit mode, UP/DOWN adjust values, SUBMIT cycles
 *   through day -> hour -> minute -> confirm.
 ********************************************************************** */


/* ***************************************************
 *                 74HC595 Connections
 *************************************************** */

const byte dataPin  = 12;
const byte latchPin = 11;
const byte clockPin = 9;


/* ***************************************************
 *                    Button Pins
 *************************************************** */

const byte upButton     = 3;
const byte submitButton = 13;
const byte downButton   = 10;


/* ***************************************************
 *                    Digit Pins
 *************************************************** */

// Physical order: rightmost to leftmost digit
const byte controlDigits[4] = {
  7,   // D4, rightmost digit
  6,   // D3
  5,   // D2
  4    // D1, leftmost digit
};


/* ***************************************************
 *                  Display Settings
 *************************************************** */

byte brightness = 40;


/* ***************************************************
 *                 Segment Mapping
 ***************************************************

   74HC595 wiring:

     Q7 -> g
     Q6 -> c
     Q5 -> decimal point
     Q4 -> d
     Q3 -> e
     Q2 -> b
     Q1 -> f
     Q0 -> a

   Segment values:

     a  = 0x01
     f  = 0x02
     b  = 0x04
     e  = 0x08
     d  = 0x10
     DP = 0x20
     c  = 0x40
     g  = 0x80
 *************************************************** */


/* ***************************************************
 *                 Display Storage
 *************************************************** */

byte displayDigits[4];


/* ***************************************************
 *                    Day Names
 *************************************************** */

const char *dayNames[7] = {
  "SUN",
  "MON",
  "TUE",
  "WED",
  "THU",
  "FRI",
  "SAT"
};


/* ***************************************************
 *                   Lesson Schedule
 ***************************************************

   Day numbers:

     0 = Sunday
     1 = Monday
     2 = Tuesday
     3 = Wednesday
     4 = Thursday
     5 = Friday
     6 = Saturday

   Lesson start times are stored as minutes since midnight.

   Edit these tables for your timetable.
 *************************************************** */

const int lessonTimeStarts[5][4] = {
  {480, 540, 600, 660},  // Monday:    8:00,  9:00, 10:00, 11:00
  {480, 540, 600, 660},  // Tuesday:   8:00,  9:00, 10:00, 11:00
  {480, 540, 600, 660},  // Wednesday: 8:00,  9:00, 10:00, 11:00
  {480, 540, 600, 660},  // Thursday:  8:00,  9:00, 10:00, 11:00
  {480, 540, 600, 660}   // Friday:    8:00,  9:00, 10:00, 11:00
};

const char *lessonNames[5][4] = {
  {"MATM", "FIZY", "CHEM", "HIST"},  // Monday
  {"GEOG", "PREP", "JNIE", "POL "},  // Tuesday
  {"CHEM", "MATM", "FIZY", "HIST"},  // Wednesday
  {"GEOG", "PREP", "MATM", "CHEM"},  // Thursday
  {"HIST", "FIZY", "GEOG", "PREP"}   // Friday
};


/* ***************************************************
 *                 Clock Variables
 *************************************************** */

byte selectedDay;
byte selectedHour;
byte selectedMinute;

unsigned long clockStartMillis;

bool setupMode = true;
byte setupStage = 0;


/* ***************************************************
 *                 Display Modes
 *************************************************** */

enum DisplayMode {
  SHOW_CURRENT_TIME,
  SHOW_NEXT_LESSON,
  SHOW_NEXT_LESSON_TIME
};

DisplayMode displayMode = SHOW_CURRENT_TIME;


/* ***************************************************
 *                 Wide W Patterns
 *************************************************** */

const byte W_LEFT  = 0x18;  // d + e
const byte W_RIGHT = 0x50;  // d + c


/* ***************************************************
 *                Button Debouncing
 *************************************************** */

const unsigned long debounceTime = 40;


/*
 * This function detects one press only.
 *
 * Buttons use INPUT_PULLUP:
 *
 *   HIGH = released
 *   LOW  = pressed
 *
 * The function waits for the button to be released before
 * returning another press event. This prevents one long press
 * from being counted many times.
 */
bool buttonPressed(byte pin) {
  static bool lastUpState = HIGH;
  static bool lastSubmitState = HIGH;
  static bool lastDownState = HIGH;

  static unsigned long upChangedAt = 0;
  static unsigned long submitChangedAt = 0;
  static unsigned long downChangedAt = 0;

  bool *lastState;
  unsigned long *changedAt;

  if (pin == upButton) {
    lastState = &lastUpState;
    changedAt = &upChangedAt;
  }
  else if (pin == submitButton) {
    lastState = &lastSubmitState;
    changedAt = &submitChangedAt;
  }
  else {
    lastState = &lastDownState;
    changedAt = &downChangedAt;
  }

  bool reading = digitalRead(pin);

  if (reading != *lastState) {
    *changedAt = millis();
    *lastState = reading;
  }

  if (millis() - *changedAt >= debounceTime) {
    if (reading == LOW) {

      /*
       * Keep refreshing the display while waiting for release.
       * Otherwise the display would appear frozen during a long press.
       */
      while (digitalRead(pin) == LOW) {
        displaySegments();
      }

      delay(25);
      return true;
    }
  }

  return false;
}


/* ***************************************************
 *                Letter Segment Patterns
 *************************************************** */

byte letterPattern(char letter) {
  switch (letter) {

    case 'A':
      return 0xCF;

    /*
     * Lowercase-style b:
     * c + d + e + f + g
     */
    case 'B':
      return 0xDA;

    case 'C':
      return 0x1B;

    case 'D':
      return 0xDC;

    case 'E':
      return 0x9B;

    case 'F':
      return 0x8B;

    case 'G':
      return 0xDB;

    case 'H':
      return 0xCE;

    case 'I':
      return 0x44;

    case 'J':
      return 0x5C;

    case 'L':
      return 0x1A;

    // Approximation of M
    case 'M':
      return 0xCE;

    // Approximation of N
    case 'N':
      return 0x8C;

    case 'O':
      return 0x5F;

    case 'P':
      return 0x8F;

    // Lowercase-style r: segments e + g
    case 'R':
      return 0x88;

    case 'S':
      return 0xD3;

    case 'T':
      return 0x9A;

    case 'U':
      return 0x5E;

    // Approximation of Y
    case 'Y':
      return 0xD6;

    case 'Z':
      return 0x9D;

    case '-':
      return 0x80;

    case ' ':
    default:
      return 0x00;
  }
}


/* ***************************************************
 *                Number Segment Patterns
 *************************************************** */

byte numberPattern(byte number) {
  switch (number) {
    case 0:
      return 0x5F;

    case 1:
      return 0x44;

    case 2:
      return 0x9D;

    case 3:
      return 0xD5;

    case 4:
      return 0xC6;

    case 5:
      return 0xD3;

    case 6:
      return 0xDB;

    case 7:
      return 0x45;

    case 8:
      return 0xDF;

    case 9:
      return 0xC7;

    default:
      return 0x00;
  }
}


/* ***************************************************
 *                    Load Text
 *************************************************** */

void loadWord(const char *word) {
  byte logicalDigits[4];
  byte usedDigits = 0;

  /*
   * Build the text from left to right.
   *
   * Normal letters use one digit.
   * W uses two digits.
   */
  for (byte i = 0; word[i] != '\0' && usedDigits < 4; i++) {

    if (word[i] == 'W') {
      if (usedDigits + 2 > 4) {
        break;
      }

      logicalDigits[usedDigits++] = W_LEFT;
      logicalDigits[usedDigits++] = W_RIGHT;
    }
    else {
      logicalDigits[usedDigits++] =
        letterPattern(word[i]);
    }
  }

  // Blank unused positions
  while (usedDigits < 4) {
    logicalDigits[usedDigits++] = 0x00;
  }

  /*
   * Convert left-to-right logical order to the physical
   * right-to-left digit order.
   */
  for (byte i = 0; i < 4; i++) {
    displayDigits[3 - i] = logicalDigits[i];
  }
}


/* ***************************************************
 *                    Load Time
 *************************************************** */

void loadTime(byte hour, byte minute) {
  byte logicalDigits[4];

  logicalDigits[0] = numberPattern(hour / 10);
  logicalDigits[1] = numberPattern(hour % 10);
  logicalDigits[2] = numberPattern(minute / 10);
  logicalDigits[3] = numberPattern(minute % 10);

  // Decimal point after the hour: HH.MM
  logicalDigits[1] |= 0x20;

  for (byte i = 0; i < 4; i++) {
    displayDigits[3 - i] = logicalDigits[i];
  }
}


/* ***************************************************
 *                 Get Current Time
 *************************************************** */

void getCurrentTime(
  byte &day,
  byte &hour,
  byte &minute
) {
  unsigned long elapsedMinutes =
    (millis() - clockStartMillis) / 60000UL;

  unsigned long totalMinutes =
    ((unsigned long)selectedHour * 60UL) +
    selectedMinute +
    elapsedMinutes;

  unsigned long totalDays =
    totalMinutes / 1440UL;

  day =
    (selectedDay + totalDays) % 7;

  unsigned long minuteOfDay =
    totalMinutes % 1440UL;

  hour =
    minuteOfDay / 60UL;

  minute =
    minuteOfDay % 60UL;
}


/* ***************************************************
 *                 Find Next Lesson
 *************************************************** */

int findNextLesson(
  byte currentDay,
  byte currentHour,
  byte currentMinute
) {
  unsigned long currentWeekMinute =
    ((unsigned long)currentDay * 1440UL) +
    ((unsigned long)currentHour * 60UL) +
    currentMinute;

  unsigned long bestDifference = 100000UL;
  int bestLesson = -1;

  for (byte d = 0; d < 5; d++) {
    for (byte l = 0; l < 4; l++) {
      unsigned long lessonWeekMinute =
        ((unsigned long)(d + 1) * 1440UL) +
        (unsigned long)lessonTimeStarts[d][l];

      unsigned long difference;

      if (lessonWeekMinute >= currentWeekMinute) {
        difference =
          lessonWeekMinute - currentWeekMinute;
      }
      else {
        difference =
          (7UL * 1440UL) -
          currentWeekMinute +
          lessonWeekMinute;
      }

      if (difference < bestDifference) {
        bestDifference = difference;
        bestLesson = d * 4 + l;
      }
    }
  }

  return bestLesson;
}


/* ***************************************************
 *              Refresh the Display
 *************************************************** */

void displaySegments() {
  for (byte x = 0; x < 4; x++) {

    // Turn all digits off
    for (byte j = 0; j < 4; j++) {
      digitalWrite(controlDigits[j], LOW);
    }

    // Send segment data
    digitalWrite(latchPin, LOW);

    shiftOut(
      dataPin,
      clockPin,
      MSBFIRST,
      displayDigits[x]
    );

    digitalWrite(latchPin, HIGH);

    // Turn on one digit
    digitalWrite(controlDigits[x], HIGH);

    delay(1);
  }

  // Turn all digits off after the refresh
  for (byte j = 0; j < 4; j++) {
    digitalWrite(controlDigits[j], LOW);
  }
}


/* ***************************************************
 *              Display Current Time
 *************************************************** */

void showCurrentTime() {
  byte day;
  byte hour;
  byte minute;

  getCurrentTime(day, hour, minute);
  loadTime(hour, minute);
}


/* ***************************************************
 *              Display Next Lesson
 *************************************************** */

void showNextLesson() {
  byte day;
  byte hour;
  byte minute;

  getCurrentTime(day, hour, minute);

  int nextLesson =
    findNextLesson(day, hour, minute);

  if (nextLesson >= 0) {
    byte dayIdx = nextLesson / 4;
    byte lesIdx = nextLesson % 4;
    loadWord(lessonNames[dayIdx][lesIdx]);
  }
  else {
    loadWord("MATH");
  }
}


/* ***************************************************
 *            Display Next Lesson Time
 *************************************************** */

void showNextLessonTime() {
  byte day;
  byte hour;
  byte minute;

  getCurrentTime(day, hour, minute);

  int nextLesson =
    findNextLesson(day, hour, minute);

  if (nextLesson >= 0) {
    byte dayIdx = nextLesson / 4;
    byte lesIdx = nextLesson % 4;
    int startMinute = lessonTimeStarts[dayIdx][lesIdx];
    loadTime(startMinute / 60, startMinute % 60);
  }
  else {
    loadTime(8, 0);
  }
}


/* ***************************************************
 *             Display Setup Value
 *************************************************** */

void displaySetupValue(byte setupStage) {
  if (setupStage == 0) {
    // Day of week, such as MON
    loadWord(dayNames[selectedDay]);
  }
  else if (setupStage == 1) {
    // Selected hour
    loadTime(selectedHour, 0);
  }
  else {
    // Selected minute
    loadTime(0, selectedMinute);
  }
}


/* ***************************************************
 *                       Setup
 *************************************************** */

void setup() {
  pinMode(dataPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);

  for (byte i = 0; i < 4; i++) {
    pinMode(controlDigits[i], OUTPUT);
    digitalWrite(controlDigits[i], LOW);
  }

  // Buttons are connected to GND
  pinMode(upButton, INPUT_PULLUP);
  pinMode(submitButton, INPUT_PULLUP);
  pinMode(downButton, INPUT_PULLUP);

  selectedDay = 1;
  selectedHour = 8;
  selectedMinute = 0;

  setupMode = true;
  setupStage = 0;

  while (setupMode) {
    displaySetupValue(setupStage);
    displaySegments();

    if (buttonPressed(upButton)) {
      if (setupStage == 0) {
        selectedDay = (selectedDay + 1) % 7;
      }
      else if (setupStage == 1) {
        selectedHour = (selectedHour + 1) % 24;
      }
      else {
        selectedMinute = (selectedMinute + 1) % 60;
      }
    }

    if (buttonPressed(downButton)) {
      if (setupStage == 0) {
        selectedDay = (selectedDay + 6) % 7;
      }
      else if (setupStage == 1) {
        selectedHour = (selectedHour + 23) % 24;
      }
      else {
        selectedMinute = (selectedMinute + 59) % 60;
      }
    }

    if (buttonPressed(submitButton)) {
      loadWord("HHHH");
      unsigned long start = millis();
      while (millis() - start < 500) {
        displaySegments();
      }

      if (setupStage < 2) {
        setupStage++;
      }
      else {
        clockStartMillis = millis();
        setupMode = false;
      }
    }
  }
}


/* ***************************************************
 *                        Loop
 *************************************************** */

void loop() {
  if (setupMode) {
    displaySetupValue(setupStage);

    if (buttonPressed(upButton)) {
      if (setupStage == 0) {
        selectedDay = (selectedDay + 1) % 7;
      }
      else if (setupStage == 1) {
        selectedHour = (selectedHour + 1) % 24;
      }
      else {
        selectedMinute = (selectedMinute + 1) % 60;
      }
    }

    if (buttonPressed(downButton)) {
      if (setupStage == 0) {
        selectedDay = (selectedDay + 6) % 7;
      }
      else if (setupStage == 1) {
        selectedHour = (selectedHour + 23) % 24;
      }
      else {
        selectedMinute = (selectedMinute + 59) % 60;
      }
    }

    if (buttonPressed(submitButton)) {
      loadWord("HHHH");
      unsigned long start = millis();
      while (millis() - start < 500) {
        displaySegments();
      }

      if (setupStage < 2) {
        setupStage++;
      }
      else {
        clockStartMillis = millis();
        setupMode = false;
        displayMode = SHOW_CURRENT_TIME;
      }
    }
  }
  else {
    switch (displayMode) {
      case SHOW_CURRENT_TIME:
        showCurrentTime();
        break;
      case SHOW_NEXT_LESSON:
        showNextLesson();
        break;
      case SHOW_NEXT_LESSON_TIME:
        showNextLessonTime();
        break;
    }

    if (buttonPressed(submitButton)) {
      displayMode = (DisplayMode)((displayMode + 1) % 3);
    }

    if (buttonPressed(upButton) || buttonPressed(downButton)) {
      setupMode = true;
      setupStage = 0;
    }
  }

  displaySegments();
}
