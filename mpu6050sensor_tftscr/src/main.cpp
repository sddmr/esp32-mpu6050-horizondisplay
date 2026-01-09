#include <SPI.h>
#include <Wire.h>
#include <MPU6050.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4

#define TOUCH_CS 5
#define TOUCH_IRQ 27

#define X_MIN 3657
#define X_MAX 676
#define Y_MIN 3455
#define Y_MAX 569

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
MPU6050 mpu;

enum ScreenState {
  SCREEN_MENU,
  SCREEN_IMU
};
ScreenState currentScreen = SCREEN_MENU;

struct Button {
  int x, y, w, h;
  const char* label;
};
Button btnIMU = {60, 100, 200, 50, "IMU VERILERI"};
Button btnBack = {10, 10, 80, 35, "GERI"};

float lastRoll = 0;
float lastPitch = 0;

void drawButton(Button b, uint16_t bgColor = ILI9341_BLUE) {
  tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, bgColor);
  tft.drawRoundRect(b.x, b.y, b.w, b.h, 8, ILI9341_WHITE);
  tft.drawRoundRect(b.x+1, b.y+1, b.w-2, b.h-2, 7, ILI9341_CYAN);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  int textW = strlen(b.label) * 12;
  tft.setCursor(b.x + (b.w - textW)/2, b.y + (b.h - 16)/2);
  tft.println(b.label);
}

bool isPressed(Button b, int x, int y) {
  return (x > b.x && x < b.x + b.w &&
          y > b.y && y < b.y + b.h);
}

void drawMenu() {
  tft.fillScreen(ILI9341_BLACK);
  
  for(int i = 0; i < 60; i++) {
    uint16_t color = tft.color565(0, 50 - i/2, 80 - i);
    tft.drawFastHLine(0, i, 320, color);
  }
  
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.setCursor(70, 25);
  tft.println("  ANA MENU");
  
  
  drawButton(btnIMU);
  

}

void drawStaticHorizon() {
  int cx = 160;
  int cy = 140;
  int radius = 75;
  
  tft.drawCircle(cx, cy, radius + 2, ILI9341_WHITE);
  tft.drawCircle(cx, cy, radius + 3, ILI9341_DARKGREY);
  
  tft.fillTriangle(cx, cy - radius - 10, cx - 5, cy - radius - 2, cx + 5, cy - radius - 2, ILI9341_YELLOW);
  
  tft.fillRect(cx - radius - 10, cy - 1, 15, 3, ILI9341_WHITE);
  tft.fillRect(cx + radius - 5, cy - 1, 15, 3, ILI9341_WHITE);
  
  for(int angle = -60; angle <= 60; angle += 30) {
    if(angle == 0) continue;
    float rad = angle * PI / 180.0;
    int x1 = cx + (radius + 5) * sin(rad);
    int y1 = cy - (radius + 5) * cos(rad);
    int x2 = cx + (radius + 12) * sin(rad);
    int y2 = cy - (radius + 12) * cos(rad);
    tft.drawLine(x1, y1, x2, y2, ILI9341_WHITE);
  }
  
  tft.fillRect(cx - 40, cy - 1, 35, 3, ILI9341_RED);
  tft.fillRect(cx + 5, cy - 1, 35, 3, ILI9341_RED);
  tft.fillRect(cx - 1, cy - 8, 3, 16, ILI9341_RED);
  tft.fillCircle(cx, cy, 4, ILI9341_YELLOW);
  tft.drawCircle(cx, cy, 5, ILI9341_RED);
}

void drawMovingHorizon(float roll, float pitch) {
  int cx = 160;
  int cy = 140;
  int radius = 75;

  
  int pitchPixel = (int)(pitch * 2.0);
  pitchPixel = constrain(pitchPixel, -50, 50);

  float rollRad = roll * PI / 180.0;
  float cosR = cos(rollRad);
  float sinR = sin(rollRad);


  for(int y = cy - radius; y <= cy + radius; y++) {
    int w = (int)sqrt(radius * radius - (y - cy) * (y - cy));
    if(w <= 0) continue;

    for(int x = cx - w; x <= cx + w; x++) {
      int dx = x - cx;
      int dy = y - cy;

      float rotY = -dx * sinR + dy * cosR;
      rotY += pitchPixel;

      uint16_t color;
      if(rotY < 0) {
        int skyDepth = (int)(-rotY / 2);
        skyDepth = constrain(skyDepth, 0, 100);
        color = tft.color565(0, 100 + skyDepth, 200);
      } else {
        color = tft.color565(139, 69, 19);
      }

      tft.drawPixel(x, y, color);
    }
  }

  for(int i = -radius; i <= radius; i++) {
    int px = cx + (int)(i * cosR);
    int py = cy + pitchPixel + (int)(i * sinR);
    int dist = (px - cx) * (px - cx) + (py - cy) * (py - cy);
    if(dist <= radius * radius) {
      tft.drawPixel(px, py, ILI9341_WHITE);
      if(py > 0 && py < 239) tft.drawPixel(px, py+1, ILI9341_WHITE);
      if(py > 1) tft.drawPixel(px, py-1, ILI9341_WHITE);
    }
  }

  for(int p = -20; p <= 20; p += 10) {
    if(p == 0) continue;
    int yLine = cy + pitchPixel - p * 2;
    if(abs(yLine - cy) < radius - 5) {
      int lineLen = 20;
      for(int step = -lineLen; step <= lineLen; step++) {
        int px = cx + (int)(step * cosR);
        int py = yLine + (int)(step * sinR);
        int dist = (px - cx) * (px - cx) + (py - cy) * (py - cy);
        if(dist <= (radius - 3) * (radius - 3)) {
          tft.drawPixel(px, py, ILI9341_YELLOW);
        }
      }
    }
  }

}

void drawIMUScreen(bool init = false) {
  if(init) {
    tft.fillScreen(ILI9341_BLACK);

    tft.fillRect(0, 0, 320, 50, tft.color565(20, 20, 40));
    tft.drawFastHLine(0, 50, 320, ILI9341_CYAN);

    drawButton(btnBack, tft.color565(80, 20, 20));

    tft.setTextColor(ILI9341_GREEN);
    tft.setTextSize(2);
    tft.setCursor(100, 18);
    tft.println("IMU SENSOR");

    drawStaticHorizon();
  }
}

void updateReadout(float roll, float pitch) {
  static float lastDispRoll = -999;
  static float lastDispPitch = -999;
  static unsigned long lastReadoutUpdate = 0;

  if(millis() - lastReadoutUpdate < 200) return;

  if(abs(roll - lastDispRoll) > 0.5 || abs(pitch - lastDispPitch) > 0.5) {
    tft.fillRect(10, 60, 95, 55, ILI9341_BLACK);
    tft.setTextColor(ILI9341_GREEN);
    tft.setTextSize(1);
    tft.setCursor(15, 65);
    tft.println("ROLL");
    tft.setTextSize(2);
    tft.setCursor(15, 80);
    if(roll >= 0) tft.print(" ");
    tft.print(roll, 1);
    tft.setTextSize(1);
    tft.print("o");

    tft.fillRect(215, 60, 95, 55, ILI9341_BLACK);
    tft.setTextColor(ILI9341_CYAN);
    tft.setTextSize(1);
    tft.setCursor(220, 65);
    tft.println("PITCH");
    tft.setTextSize(2);
    tft.setCursor(220, 80);
    if(pitch >= 0) tft.print(" ");
    tft.print(pitch, 1);
    tft.setTextSize(1);
    tft.print("o");

    lastDispRoll = roll;
    lastDispPitch = pitch;
    lastReadoutUpdate = millis();
  }
}

void setup() {
  Serial.begin(115200);
  SPI.begin(18, 19, 23);
  Wire.begin(21, 22);

  tft.begin();
  tft.setRotation(3);

  ts.begin();
  ts.setRotation(3);

  mpu.initialize();
  mpu.setDLPFMode(6); 

  if (!mpu.testConnection()) {
    tft.fillScreen(ILI9341_RED);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(60, 120);
    tft.println("MPU6050 HATA!");
    while (1);
  }

  drawMenu();
}

unsigned long lastUpdate = 0;

void loop() {

  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    int tx = map(p.x, X_MIN, X_MAX, 0, 320);
    int ty = map(p.y, Y_MIN, Y_MAX, 0, 240);
    tx = constrain(tx, 0, 319);
    ty = constrain(ty, 0, 239);

    if (currentScreen == SCREEN_MENU) {
      if (isPressed(btnIMU, tx, ty)) {
        currentScreen = SCREEN_IMU;
        drawIMUScreen(true);
        delay(300);
      }
    } else if (currentScreen == SCREEN_IMU) {
      if (isPressed(btnBack, tx, ty)) {
        currentScreen = SCREEN_MENU;
        drawMenu();
        delay(300);
      }
    }

    while(ts.touched()) { delay(10); }
    delay(100);
  }

  if (currentScreen == SCREEN_IMU) {
    if(millis() - lastUpdate > 40) {
      int16_t ax, ay, az, gx, gy, gz;
      mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

      float axg = ax / 16384.0;
      float ayg = ay / 16384.0;
      float azg = az / 16384.0;

      float roll  = atan2(ayg, azg) * 180 / PI;
      float pitch = atan2(-axg, sqrt(ayg * ayg + azg * azg)) * 180 / PI;

      roll = lastRoll * 0.6 + roll * 0.4;
      pitch = lastPitch * 0.6 + pitch * 0.4;

      drawMovingHorizon(roll, pitch); 
      updateReadout(roll, pitch);

      lastRoll = roll;
      lastPitch = pitch;
      lastUpdate = millis();
    }
  }
}
