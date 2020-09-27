#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <output_spi.h>

// GUItool: begin automatically generated code
AudioSynthWaveformSine   sine1;          //xy=272,218
AudioOutputTDM           tdm2;           //xy=514,291
AudioOutputSPI           tdm1;           //xy=514,291
AudioConnection          patchCord1(sine1, 0, tdm1, 0);
AudioConnection          patchCord2(sine1, 0, tdm1, 1);
AudioConnection          patchCord3(sine1, 0, tdm1, 2);
AudioConnection          patchCord4(sine1, 0, tdm1, 3);
AudioConnection          patchCord5(sine1, 0, tdm1, 4);
AudioConnection          patchCord6(sine1, 0, tdm1, 5);
AudioConnection          patchCord7(sine1, 0, tdm1, 6);
AudioConnection          patchCord8(sine1, 0, tdm1, 7);
AudioConnection          patchCord9(sine1, 0, tdm1, 8);
AudioConnection          patchCord10(sine1, 0, tdm1, 9);
AudioConnection          patchCord11(sine1, 0, tdm1, 10);
AudioConnection          patchCord12(sine1, 0, tdm1, 11);
AudioConnection          patchCord13(sine1, 0, tdm1, 12);
AudioConnection          patchCord14(sine1, 0, tdm1, 13);
AudioConnection          patchCord15(sine1, 0, tdm1, 14);
AudioConnection          patchCord16(sine1, 0, tdm1, 15); 
// GUItool: end automatically generated code

void setup() {
  Serial.begin(9600);
  while(!Serial) {
    delay(100);
  }
  delay(2000);
  AudioMemory(60);
 
  sine1.frequency(1000);
  sine1.amplitude(1.0);
}

void loop() {
  delay(1000);
  Serial.print(".");
}
