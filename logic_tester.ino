#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Permanent wiring:
// ZIF pin 1 -> D2
// ZIF pin 2 -> D3
// ZIF pin 3 -> D4
// ZIF pin14 -> 5V
// ZIF pin7  -> GND
// LED on D6 -> 1k -> LED -> GND

#define PIN_IC1 2   // IC pin 1
#define PIN_IC2 3   // IC pin 2
#define PIN_IC3 4   // IC pin 3

#define LED_PIN 6   // Universal LED output
#define MODE_PIN 7  // HIGH = manual, LOW = auto

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Truth tables
const byte TBL_7400_NAND[4] = {1,1,1,0};
const byte TBL_7408_AND [4] = {0,0,0,1};
const byte TBL_7432_OR  [4] = {0,1,1,1};
const byte TBL_7486_XOR [4] = {0,1,1,0};
const byte TBL_7402_NOR [4] = {1,0,0,0};

enum IcType {
  IC_UNKNOWN = 0,
  IC_7400,
  IC_7402,
  IC_7404,
  IC_7408,
  IC_7432,
  IC_7486
};

IcType currentIC = IC_UNKNOWN;


// -----------------------------------------------------

bool patternMatch(const byte a[4], const byte b[4]) {
  for (int i = 0; i < 4; i++)
    if (a[i] != b[i]) return false;
  return true;
}

// ----------- Detect 7404 NOT gate (pin1=A, pin2=Y) -----------
bool detect7404() {
  pinMode(PIN_IC1, OUTPUT); // A
  pinMode(PIN_IC2, INPUT);  // Y

  digitalWrite(PIN_IC1, LOW);
  delay(2);
  int y0 = digitalRead(PIN_IC2);

  digitalWrite(PIN_IC1, HIGH);
  delay(2);
  int y1 = digitalRead(PIN_IC2);

  return (y0 == HIGH && y1 == LOW);
}

// ----------- Test 2-input IC with dynamic mapping -----------
void test2InputIC(byte out[4], int A_PIN, int B_PIN, int Y_PIN) {
  const byte pattern[4][2] = {
    {0,0},{0,1},{1,0},{1,1}
  };

  pinMode(A_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);
  pinMode(Y_PIN, INPUT);

  for (int i=0; i<4; i++) {
    digitalWrite(A_PIN, pattern[i][0]);
    digitalWrite(B_PIN, pattern[i][1]);
    delay(2);
    out[i] = digitalRead(Y_PIN);

    // Mirror output to LED
    digitalWrite(LED_PIN, out[i]);
  }
}j

// ----------- Detect which IC -----------
IcType detectIC() {
  byte out[4];

  // 1) Try NOT gate (7404)
  if (detect7404()) return IC_7404;

  // 2) Standard pinout: A=pin1, B=pin2, Y=pin3
  test2InputIC(out, PIN_IC1, PIN_IC2, PIN_IC3);

  if (patternMatch(out, TBL_7400_NAND)) return IC_7400;
  if (patternMatch(out, TBL_7408_AND )) return IC_7408;
  if (patternMatch(out, TBL_7432_OR  )) return IC_7432;
  if (patternMatch(out, TBL_7486_XOR )) return IC_7486;

  // 3) Special mapping for 7402:
  // A=pin2, B=pin3, Y=pin1
  test2InputIC(out, PIN_IC2, PIN_IC3, PIN_IC1);

  if (patternMatch(out, TBL_7402_NOR)) return IC_7402;

  return IC_UNKNOWN;
}

// ----------- Expected output Y for IC -----------
byte expectedY(IcType ic, byte A, byte B) {
  if (ic == IC_7404) return (A==0?1:0);

  byte idx = (A<<1) | B;

  switch(ic) {
    case IC_7400: return TBL_7400_NAND[idx];
    case IC_7402: return TBL_7402_NOR [idx];
    case IC_7408: return TBL_7408_AND [idx];
    case IC_7432: return TBL_7432_OR  [idx];
    case IC_7486: return TBL_7486_XOR [idx];
    default: return 0;
  }
}

// ----------- Print IC name -----------
void printICName(IcType ic) {
  lcd.clear();
  lcd.setCursor(0,0);
  switch(ic) {
    case IC_7400: lcd.print("7400 NAND"); break;
    case IC_7402: lcd.print("7402 NOR "); break;
    case IC_7404: lcd.print("7404 NOT "); break;
    case IC_7408: lcd.print("7408 AND "); break;
    case IC_7432: lcd.print("7432 OR  "); break;
    case IC_7486: lcd.print("7486 XOR "); break;
    default: lcd.print("UNKNOWN IC");
  }
}

// -----------------------------------------------------
// AUTO MODE
// -----------------------------------------------------
void autoMode() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("AUTO TESTING");
  delay(200);

  currentIC = detectIC();

  if (currentIC == IC_UNKNOWN) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("IC FAULTY");
    lcd.setCursor(0,1);
    lcd.print("OR UNKNOWN");
    digitalWrite(LED_PIN, LOW);
    delay(2000);
    return;
  }

  printICName(currentIC);
  lcd.setCursor(0,1);
  lcd.print("IC GOOD");

  delay(2000);
}

// -----------------------------------------------------
// MANUAL MODE
// -----------------------------------------------------
void manualMode() {

  // Detect IC first time only
  if (currentIC == IC_UNKNOWN) {
    currentIC = detectIC();
  }

  const byte pattern[4][2] = {{0,0},{0,1},{1,0},{1,1}};
  bool all_ok = true;

  for (int i=0; i<4; i++) {
    byte A = pattern[i][0];
    byte B = pattern[i][1];
    byte Y = 0, expected = 0;

    if (currentIC == IC_7404) {
      // 7404: pin1=A, pin2=Y
      pinMode(PIN_IC1, OUTPUT);
      pinMode(PIN_IC2, INPUT);
      digitalWrite(PIN_IC1, A);
      delay(5);
      Y = digitalRead(PIN_IC2);
    }
    else if (currentIC == IC_7402) {
      // 7402: pin2=A, pin3=B, pin1=Y
      pinMode(PIN_IC2, OUTPUT);
      pinMode(PIN_IC3, OUTPUT);
      pinMode(PIN_IC1, INPUT);
      digitalWrite(PIN_IC2, A);
      digitalWrite(PIN_IC3, B);
      delay(5);
      Y = digitalRead(PIN_IC1);
    }
    else {
      // Standard ICs
      pinMode(PIN_IC1, OUTPUT);
      pinMode(PIN_IC2, OUTPUT);
      pinMode(PIN_IC3, INPUT);
      digitalWrite(PIN_IC1, A);
      digitalWrite(PIN_IC2, B);
      delay(5);
      Y = digitalRead(PIN_IC3);
    }

    // LED follows measured output
    digitalWrite(LED_PIN, Y);

    expected = expectedY(currentIC, A, B);

    if (Y != expected) all_ok = false;

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("MANUAL MODE");

    lcd.setCursor(0,1);
    lcd.print("A="); lcd.print(A);
    lcd.print(" B="); lcd.print(B);
    lcd.print(" Y="); lcd.print(Y);

    delay(2000);
  }

  printICName(currentIC);
  lcd.setCursor(0,1);
  if (all_ok) lcd.print("MANUAL OK");
  else        lcd.print("FAIL");

  digitalWrite(LED_PIN, LOW);
  delay(2000);
}


// -----------------------------------------------------

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(MODE_PIN, INPUT_PULLUP);

  lcd.begin();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("UNIV IC TESTER");
  delay(1500);
}

void loop() {
  if (digitalRead(MODE_PIN) == HIGH)
    manualMode();
  else
    autoMode();
}