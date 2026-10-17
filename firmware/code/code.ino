#include <Wire.h>
#include <RTClib.h>
#include <esp_task_wdt.h>

// ───── CAPTEURS ─────
#define PIN_CAPTEUR 27
#define PIN_PLUIE   34
#define PIN_RELAIS  26

#define SEUIL_HAUT  900
#define SEUIL_PLUIE 1500

RTC_DS3231 rtc;

// ───── MOTEUR ─────
#define ENA 25
#define IN1 32
#define IN2 33
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8

// ───── ENCODEUR ─────
#define ENC_A 35
#define ENC_B 13
volatile long encoderCount = 0;

// ───── FDC ─────
#define FDC_HAUT 18
#define FDC_BAS  19

// ───── RTC SCHEDULE ─────
const int HEURE_DEMARRAGE  = 22;
const int MINUTE_DEMARRAGE = 55;
bool dejaExecute = false;

// ───── ANTI-BLOCAGE ─────
long lastEncoder = 0;
unsigned long lastMoveTime = 0;
#define TEMPS_BLOCAGE 3000

// ───── ETAT MOTEUR ─────
bool sensAvant = true;
String etatMoteur = "STOP";
int vitesseMoteur = 0;

// ───── ISR ENCODEUR ─────
void IRAM_ATTR encoderISR() {
  if (digitalRead(ENC_B) == HIGH) encoderCount++;
  else encoderCount--;
}

// ───── LECTURES ─────
int lireCapteurEau() {
  int s = 0;
  for (int i = 0; i < 10; i++) { s += analogRead(PIN_CAPTEUR); delay(5); }
  return s / 10;
}

int lirePluie() {
  int s = 0;
  for (int i = 0; i < 10; i++) { s += analogRead(PIN_PLUIE); delay(5); }
  return s / 10;
}

// ───── MOTEUR CONTROL ─────
void motorAvant(int v) {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  ledcWrite(ENA, v);
  etatMoteur = "MONTEE";
  vitesseMoteur = v;
}

void motorArriere(int v) {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  ledcWrite(ENA, v);
  etatMoteur = "DESCENTE";
  vitesseMoteur = v;
}

void motorStop() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  ledcWrite(ENA, 0);
  etatMoteur = "STOP";
  vitesseMoteur = 0;
}

// ───── OBSTACLE CHECK ─────
bool obstacleDetecte() {
  if (encoderCount != lastEncoder) {
    lastEncoder = encoderCount;
    lastMoveTime = millis();
    return false;
  }
  return (millis() - lastMoveTime > TEMPS_BLOCAGE);
}

// ───── URGENCE ─────
void arretUrgence() {
  motorStop();
  digitalWrite(PIN_RELAIS, LOW);
  Serial.println("!!! ARRET URGENCE !!!");
  ESP.restart();
}

// ───── NETTOYAGE ─────
void nettoyerPanneau() {
  Serial.println(">>> NETTOYAGE DEMARRE");
  digitalWrite(PIN_RELAIS, HIGH);

  int cycles = 0;
  int etat = 0;

  lastEncoder = encoderCount;
  lastMoveTime = millis();
  motorAvant(150);

  while (cycles < 3) {   // ✅ 3 cycles FIX
    esp_task_wdt_reset();

    if (etat == 0 || etat == 2) {
      if (obstacleDetecte()) {
        Serial.println("OBSTACLE DETECTE");
        arretUrgence();
      }
    }

    switch (etat) {

      case 0: // MONTEE
        if (digitalRead(FDC_HAUT) == HIGH) {
          motorStop();
          Serial.println("FDC HAUT atteint");
          lastEncoder = encoderCount;
          lastMoveTime = millis();
          etat = 1;
        }
        break;

      case 1: // attente FDC haut relâché
        if (digitalRead(FDC_HAUT) == LOW) {
          delay(200);
          motorArriere(150);
          lastEncoder = encoderCount;
          lastMoveTime = millis();
          etat = 2;
          Serial.println("Descente demarre");
        }
        break;

      case 2: // DESCENTE
        if (digitalRead(FDC_BAS) == HIGH) {
          motorStop();
          Serial.println("FDC BAS atteint");
          lastEncoder = encoderCount;
          lastMoveTime = millis();
          etat = 3;
        }
        break;

      case 3: // fin cycle
        if (digitalRead(FDC_BAS) == LOW) {
          cycles++;
          Serial.print("Cycle termine: ");
          Serial.println(cycles);

          delay(200);

          if (cycles < 3) {   // ✅ 3 cycles FIX
            motorAvant(150);
            lastEncoder = encoderCount;
            lastMoveTime = millis();
          }

          etat = 0;
        }
        break;
    }

    delay(10);
  }

  motorStop();
  digitalWrite(PIN_RELAIS, LOW);
  Serial.println(">>> NETTOYAGE TERMINE");
}

// ───── SETUP ─────
void setup() {
  Serial.begin(115200);

  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 15000,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  ledcAttach(ENA, PWM_FREQ, PWM_RESOLUTION);

  pinMode(ENC_A, INPUT);
  pinMode(ENC_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_A), encoderISR, RISING);

  pinMode(FDC_HAUT, INPUT_PULLDOWN);
  pinMode(FDC_BAS, INPUT_PULLDOWN);

  pinMode(PIN_RELAIS, OUTPUT);
  digitalWrite(PIN_RELAIS, LOW);

  Wire.begin(21, 22);

  if (!rtc.begin()) {
    Serial.println("RTC ERROR");
    ESP.restart();
  }

  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  Serial.println("SYSTEME PRET");
}

// ───── LOOP ─────
void loop() {
  esp_task_wdt_reset();

  DateTime now = rtc.now();
  int niveau = lireCapteurEau();
  int pluieVal = lirePluie();
  bool pluie = pluieVal < SEUIL_PLUIE;

  if (now.hour() == HEURE_DEMARRAGE &&
      now.minute() == MINUTE_DEMARRAGE &&
      !dejaExecute &&
      niveau > SEUIL_HAUT &&
      !pluie) {

    nettoyerPanneau();
    dejaExecute = true;
  }

  if (now.minute() != MINUTE_DEMARRAGE) {
    dejaExecute = false;
  }

  static unsigned long tLog = 0;
  if (millis() - tLog > 1000) {
    tLog = millis();

    Serial.println("------------");
    Serial.println(now.timestamp(DateTime::TIMESTAMP_FULL));
    Serial.print("Eau: "); Serial.print(niveau);
    Serial.print(" | Pluie: "); Serial.print(pluieVal);
    Serial.print(" | Meteo: "); Serial.println(pluie ? "PLUIE" : "SEC");
    Serial.print("Moteur: "); Serial.print(etatMoteur);
    Serial.print(" | Vitesse: "); Serial.print(vitesseMoteur);
    Serial.print(" | Encoder: "); Serial.println(encoderCount);
  }
}
