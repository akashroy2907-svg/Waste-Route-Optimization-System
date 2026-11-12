
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Servo.h>

// OLED: 128 x 32 I2C SSD1306
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET
);

// Pin connections
const int trigPin = 9;
const int echoPin = 10;
const int rainPin = A1;
const int lidServoPin = 6;
const int sorterServoPin = 5;

// Servo objects
Servo lidServo;
Servo sorterServo;

// Adjust these angles to match your mechanism
const int LID_CLOSED = 0;
const int LID_OPEN = 90;

const int SORT_CENTER = 90;
const int SORT_WET = 30;  // 60 degrees left
const int SORT_DRY = 150; // 60 degrees right

// Calibrate using your rain sensor readings
const int WET_THRESHOLD = 900;

// Assumption: lower analog reading means wet
const bool LOWER_VALUE_MEANS_WET = true;

// Timing
const unsigned long LID_OPEN_TIME = 4000;
const unsigned long SORT_DELAY = 2500;
const unsigned long SORT_HOLD_TIME = 800;

// Distance settings
const float TRIGGER_DISTANCE = 5.0;
const float RESET_DISTANCE = 8.0;

enum BinState {
  WAITING,
  LID_OPENED,
  SORTING,
  FINISHED
};

BinState state = WAITING;

unsigned long lidOpenedAt = 0;
unsigned long sortStartedAt = 0;

bool sortedWet = false;
bool sortingDone = false;
bool sorterReturned = false;

float distanceCm = -1;
int rainValue = 0;
String wasteType = "Waiting";

float readDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duration =
      pulseIn(echoPin, HIGH, 30000UL);

  if (duration == 0) {
    return -1;
  }

  return duration * 0.0343 / 2.0;
}

bool isWetWaste(int value) {
  if (LOWER_VALUE_MEANS_WET) {
    return value < WET_THRESHOLD;
  } else {
    return value > WET_THRESHOLD;
  }
}

void showStatus(const char* line1,
                const char* line2,
                const char* line3,
                const char* line4) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println(line1);
  display.println(line2);
  display.println(line3);
  display.println(line4);

  display.display();
}

void setup() {
  Serial.begin(9600);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  lidServo.attach(lidServoPin);
  sorterServo.attach(sorterServoPin);

  lidServo.write(LID_CLOSED);
  sorterServo.write(SORT_CENTER);

  if (!display.begin(
        SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED initialization failed!");
    while (true) {}
  }

  showStatus(
    "SMART WASTE BIN",
    "Distance: Starting",
    "Waste: --",
    "Status: Ready"
  );

  delay(1000);
}

void loop() {
  // Sample sensors
  distanceCm = readDistance();
  rainValue = analogRead(rainPin);

  unsigned long now = millis();

  // Display distance and rain reading in Serial Monitor
  Serial.print("Distance: ");
  Serial.print(distanceCm);
  Serial.print(" cm | Rain AO: ");
  Serial.print(rainValue);
  Serial.print(" | State: ");
  Serial.println((int)state);

  // STATE 1: Wait for an object within 5 cm
  if (state == WAITING) {
    if (distanceCm >= 0 &&
        distanceCm < TRIGGER_DISTANCE) {

      lidServo.write(LID_OPEN);
      lidOpenedAt = now;

      sortingDone = false;
      sorterReturned = false;
      wasteType = "Checking";

      state = LID_OPENED;
    }
  }

  // STATE 2: Lid open; wait 2.5 seconds to sort
  if (state == LID_OPENED) {
    if (!sortingDone &&
        now - lidOpenedAt >= SORT_DELAY) {

      sortedWet = isWetWaste(rainValue);

      if (sortedWet) {
        sorterServo.write(SORT_WET);
        wasteType = "WET";
      } else {
        sorterServo.write(SORT_DRY);
        wasteType = "DRY";
      }

      sortStartedAt = now;
      sortingDone = true;
      state = SORTING;
    }
  }

  // STATE 3: Hold sorter briefly, then center it
  if (state == SORTING) {
    if (!sorterReturned &&
        now - sortStartedAt >= SORT_HOLD_TIME) {

      sorterServo.write(SORT_CENTER);
      sorterReturned = true;
    }

    // Close lid 4 seconds after it opened
    if (now - lidOpenedAt >= LID_OPEN_TIME) {
      lidServo.write(LID_CLOSED);
      state = FINISHED;
    }
  }

  // STATE 4: Prevent repeated triggers while object remains
  if (state == FINISHED) {
    if (distanceCm < 0 ||
        distanceCm >= RESET_DISTANCE) {
      state = WAITING;
      wasteType = "Waiting";
    }
  }

  // Update OLED
  char distanceLine[22];
  char rainLine[22];

  if (distanceCm < 0) {
    snprintf(distanceLine, sizeof(distanceLine),
             "Distance: No echo");
  } else {
    snprintf(distanceLine, sizeof(distanceLine),
             "Distance: %.1f cm", distanceCm);
  }

  snprintf(rainLine, sizeof(rainLine),
           "Rain AO: %d", rainValue);

  const char* statusLine;

  switch (state) {
    case WAITING:
      statusLine = "Status: Ready";
      break;
    case LID_OPENED:
      statusLine = "Status: Lid open";
      break;
    case SORTING:
      statusLine = "Status: Sorting";
      break;
    default:
      statusLine = "Status: Lid closed";
      break;
  }

  char wasteLine[22];
  snprintf(wasteLine, sizeof(wasteLine),
           "Waste: %s", wasteType.c_str());

  showStatus(
    "SMART WASTE BIN",
    distanceLine,
    wasteLine,
    statusLine
  );

  delay(50);
}
