#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <output_ad5754_dual.h>

#define BUSY 3
#define START_CONVERSION 5
#define CHIP_SELECT 36
#define RESET 4

#define TOTAL_RAW_BYTES 16

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
//#include <ST7735_t3.h> // Hardware-specific library

#define TFT_SCLK 13  // SCLK can also use pin 14
#define TFT_MOSI 11  // MOSI can also use pin 7
#define TFT_CS   6  // CS & DC can use pins 2, 6, 9, 10, 15, 20, 21, 22, 23
#define TFT_DC    2  //  but certain pairs must NOT be used: 2+10, 6+9, 20+23, 21+22
#define TFT_RST   -1 // RST can use any pin
//ST7735_t3 TFT = ST7735_t3(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
Adafruit_ST7735 TFT = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// GUItool: begin automatically generated code
AudioSynthWaveformSine   sine1;          //xy=272,218
AudioSynthWaveformSine   sine2;          //xy=272,218
AudioSynthWaveformSine   sine3;          //xy=272,218
AudioSynthWaveformSine   sine4;          //xy=272,218
AudioSynthWaveformSine   sine5;          //xy=272,218
AudioSynthWaveformSine   sine6;          //xy=272,218
AudioSynthWaveformSine   sine7;          //xy=272,218
AudioSynthWaveformSine   sine8;          //xy=272,218
AudioOutputTDM           tdm2;           //xy=514,291
AudioOutputAD5754Dual    tdm1;           //xy=514,291
AudioConnection          patchCord1(sine1, 0, tdm1, 0);
AudioConnection          patchCord2(sine2, 0, tdm1, 1);
AudioConnection          patchCord3(sine3, 0, tdm1, 2);
AudioConnection          patchCord4(sine4, 0, tdm1, 3);
AudioConnection          patchCord5(sine5, 0, tdm1, 4);
AudioConnection          patchCord6(sine6, 0, tdm1, 5);
AudioConnection          patchCord7(sine7, 0, tdm1, 6);
AudioConnection          patchCord8(sine8, 0, tdm1, 7);
// GUItool: end automatically generated code

int bytesToRead = TOTAL_RAW_BYTES;  
byte raw[TOTAL_RAW_BYTES];  
int16_t parsed[8];
int x = 0;
int16_t oldpos[8] = {0,0,0,0,0,0,0,0};
int16_t colors[8] = {ST7735_GREEN,ST7735_RED,ST7735_BLUE,ST7735_CYAN,ST7735_MAGENTA,ST7735_YELLOW,0x00AA,ST7735_WHITE};

int8_t trigger_channel = 7;

void setup() {
  Serial.begin(9600);
  AudioMemory(60);
  pinMode(31, INPUT); // SPI1 SCK copy  
  pinMode(37, INPUT);
  pinMode(34, INPUT);
  pinMode(BUSY, INPUT_PULLUP);
  pinMode(TFT_CS, OUTPUT);
  pinMode(START_CONVERSION, OUTPUT);
  pinMode(CHIP_SELECT, OUTPUT);
  pinMode(RESET, OUTPUT);
  digitalWrite(RESET, LOW); 

  SPI2.setSCK(49);
  SPI2.setMISO(54);
  SPI2.begin();
  
  TFT.initR(INITR_144GREENTAB);
  TFT.setRotation(3);
  TFT.fillScreen(ST7735_BLACK);
  
  sine1.frequency(1);
  sine1.amplitude(1.0);

  sine2.frequency(2);
  sine2.amplitude(1.0);

  sine3.frequency(4);
  sine3.amplitude(1.0);

  sine4.frequency(8);
  sine4.amplitude(1.0);

  sine5.frequency(16);
  sine5.amplitude(1.0);

  sine6.frequency(32);
  sine6.amplitude(1.0);

  sine7.frequency(64);
  sine7.amplitude(1.0);

  sine8.frequency(128);
  sine8.amplitude(1.0);

}

void loop() {
  int i;
  //SPI.begin();
  digitalWrite(START_CONVERSION, LOW);
  //Serial.print("L");
  delayMicroseconds(100);
  digitalWrite(START_CONVERSION, HIGH);
  //Serial.print("H");
    
  while (digitalRead(BUSY) == HIGH) {
    // wait for conversion to complete
    //Serial.print(".");
    //delayMicroseconds(10);
  }
  //Serial.print("\n");

  digitalWrite(CHIP_SELECT, LOW); 
  //SPI2.beginTransaction(SPISettings());
  //AudioNoInterrupts();
  while (bytesToRead > 0) {
    raw[TOTAL_RAW_BYTES - bytesToRead] = SPI2.transfer(0x00);
    bytesToRead--;
  }
  //AudioInterrupts();
  //SPI2.endTransaction(); 
  digitalWrite(CHIP_SELECT, HIGH);
  bytesToRead = TOTAL_RAW_BYTES;

  parseRawBytes();
  int8_t lastValueOnChannel3 = (trigger_channel != -1)? oldpos[trigger_channel] : 0;
 
  for(i=0; i<8; i++) {
    int8_t value = (parsed[i] >> 10) + 64;

    if (x < 126) 
      TFT.drawLine(x, oldpos[i], x+1, value, colors[i]);
    //TFT.drawPixel(x, value, colors[i]);
    //Serial.printf("%d,",  parsed[i]);

    oldpos[i] = value;
  }
  //Serial.print("\n");

  if (x <= 126) { 
    x++;
  } else {

    if ( trigger_channel == -1 || ((lastValueOnChannel3 <= 64) && (oldpos[trigger_channel] >= 64))) {
      x = 0;
      TFT.fillScreen(ST7735_BLACK);
    }
  }
}

void parseRawBytes() {
  parsed[0] = (raw[0] << 8) + raw[1];
  parsed[1] = (raw[2] << 8) + raw[3];
  parsed[2] = (raw[4] << 8) + raw[5];
  parsed[3] = (raw[6] << 8) + raw[7];
  parsed[4] = (raw[8] << 8) + raw[9];
  parsed[5] = (raw[10] << 8) + raw[11];
  parsed[6] = (raw[12] << 8) + raw[13];
  parsed[7] = (raw[14] << 8) + raw[15];
}
