#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <output_ad5754_dual.h>

// GUItool: begin automatically generated code
AudioSynthWaveformSine   sine1;          //xy=272,218
AudioOutputTDM           tdm2;           //xy=514,291
AudioOutputAD5754Dual    tdm1;           //xy=514,291
AudioConnection          patchCord1(sine1, 0, tdm1, 0);
AudioConnection          patchCord2(sine1, 0, tdm1, 1);
AudioConnection          patchCord3(sine1, 0, tdm1, 2);
AudioConnection          patchCord4(sine1, 0, tdm1, 3);
AudioConnection          patchCord5(sine1, 0, tdm1, 4);
AudioConnection          patchCord6(sine1, 0, tdm1, 5);
AudioConnection          patchCord7(sine1, 0, tdm1, 6);
AudioConnection          patchCord8(sine1, 0, tdm1, 7);
// GUItool: end automatically generated code

void setup() {
  Serial.begin(9600);
  AudioMemory(60);
 
  sine1.frequency(60);
  sine1.amplitude(1.0);
}

void loop() {
  delay(1000);
  Serial.print(".");
}
