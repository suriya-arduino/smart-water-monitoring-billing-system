#include <ESP8266WiFi.h>
#include <ESP_Mail_Client.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ====== Pin Configs ======
#define TRIGGER_PIN D6
#define ECHO_PIN D7
#define FLOW_SENSOR_PIN D5
#define RELAY_PIN D4
#define EMAIL_TRIGGER_BUTTON D8

// ====== WiFi Credentials ======
#define WIFI_SSID "ESP_TEST"
#define WIFI_PASSWORD "12345678"

// ====== SMTP Configs ======
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "suriyass1437@gmail.com"
#define AUTHOR_PASSWORD "fjjc twcw xbzm ndis"
#define RECIPIENT_EMAIL "suridoss708@gmail.com"
//#define RECIPIENT_EMAIL "schiranjeevi007007@gmail.com"
// ====== Water Tank & Billing Settings ======
const int tankHeightCM = 10;
const float calibrationFactor = 7.5;
const int FREE_USAGE_LIMIT = 1;
const int BILL_THRESHOLD = 50;


volatile int pulseCount = 0;
float totalLitres = 0.0;
int billAmount = 0;

unsigned long lastTime = 0;
bool emailSent = false;

LiquidCrystal_I2C lcd(0x27, 16, 2);
SMTPSession smtp;

void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

void setup() {

  Serial.begin(9600);

  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  pinMode(EMAIL_TRIGGER_BUTTON, INPUT_PULLUP); // Active LOW

  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, RISING);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Water Monitor");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
}

void loop() {
  // === Water Level Check ===
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  int distanceCM = duration * 0.034 / 2;
  int waterLevelCM = tankHeightCM - distanceCM;
  float waterPercentage = (float)waterLevelCM / tankHeightCM * 100.0;

  // Control Relay
  if (waterPercentage < 50.0) {
    digitalWrite(RELAY_PIN, HIGH);  // Pump ON
  } else {
    digitalWrite(RELAY_PIN, LOW);   // Pump OFF
  }

  // === Flow Sensor & Billing ===
  unsigned long currentTime = millis();
  if (currentTime - lastTime >= 1000) {
    detachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN));

    float flowRate = pulseCount / calibrationFactor;
    float litres = flowRate / 60.0;
    totalLitres += litres;

    if (totalLitres > FREE_USAGE_LIMIT) {
  float billableLitres = totalLitres - FREE_USAGE_LIMIT;

  if (billableLitres <= 5) {   // From 6L to 10L
   
    billAmount = billableLitres * 10;
  } else {
    
    billAmount = (5 * 10) + ((billableLitres - 5) * 15);    // First 11L at ₹10, rest at ₹15
  }
} else {
  billAmount = 0;
}

    // LCD Display
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Litres: ");
    lcd.print(totalLitres, 1);

    lcd.setCursor(0, 1);
    lcd.print("Bill: ");
    lcd.print(billAmount);

    Serial.print("Total Litres: ");
    Serial.print(totalLitres, 2);
    Serial.print(" L | Bill: ");
    Serial.println(billAmount);

    pulseCount = 0;
    lastTime = currentTime;

    attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, RISING);
  }

  // === Email Trigger ===
 /* bool buttonPressed = digitalRead(EMAIL_TRIGGER_BUTTON) == LOW;
  bool billTooHigh = billAmount >= BILL_THRESHOLD;

  if ((buttonPressed || billTooHigh) && !emailSent) {
    sendEmail();
    emailSent = true;
  }*/

 // === Email Trigger ===
  if (digitalRead(EMAIL_TRIGGER_BUTTON) == HIGH && !emailSent) {
    delay(50); // Debounce
    if (digitalRead(EMAIL_TRIGGER_BUTTON) == HIGH) {
      Serial.println("Button pressed! Sending email...");
      sendEmail();
      emailSent = true;
      delay(5000); // Wait before allowing another email
    }
  }
} // ← CLOSES the loop() function here ✅

void sendEmail() {
  ESP_Mail_Session session;
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;

  SMTP_Message message;
  message.sender.name = "Water Bill";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "Water Bill Payment Alert!";
  message.addRecipient("User", RECIPIENT_EMAIL);

  String msg = "Dear User,\n\n";
  msg += "This is an automated message from your water monitoring system:\n\n";
  msg += "Here is your latest water usage summary\n";
  msg += "----------------------------------------- \n";
  msg += "Total Water Used: " + String(totalLitres, 2) + " L\n";
  msg += "Free Usage Limit: " + String(FREE_USAGE_LIMIT) + " L\n";
  msg += "Free Usage Limit: 5 L\n";
  msg += "Rate Slab:\n";
  msg += "6L to 10L  -> ₹10/L\n";
  msg += "11L above -> ₹15/L\n";
  //msg += "Rate Per Litter: ₹10 \n";
  msg += "----------------------------------------- \n";
  msg += "Total Bill: ₹" + String(billAmount) + "\n\n";
  msg += "Thank you for monitoring your water usage responsibly. \n\n";
  msg += "Regards,\nWater Monitoring System\nGRT INSTITUTE OF ENGINEERING AND TECHNOLOGY \n\n";
  msg += "Please complete Your payment by using this link before due date \n";
  msg += "https://razorpay.me/@vlgeinstituteprivatelimited \n";


  message.text.content = msg.c_str();

  smtp.callback(smtpCallback);
  if (!smtp.connect(&session)) {
    Serial.println("SMTP connection failed.");
    return;
  }

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Email failed: " + smtp.errorReason());
  } else {
    Serial.println("Email sent successfully!");
  }

  smtp.closeSession();
}

void smtpCallback(SMTP_Status status) {
  Serial.println(status.info());
}