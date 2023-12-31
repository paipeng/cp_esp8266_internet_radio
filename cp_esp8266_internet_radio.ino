#include <Arduino.h>

#if defined(ESP32)
    #include <WiFi.h>
#else
    #include <ESP8266WiFi.h>
#endif

#include "wifi_const.h"
//#define WIFI_SSID "xxx"
//#define WIFI_PASSWD "xxx"


#include "AudioFileSourceICYStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2SNoDAC.h"


#define MAX98357A_I2S_DOUT  RX
#define MAX98357A_I2S_BCLK D8
#define MAX98357A_I2S_LRC  D4




// Stream URL of Logitech Media Server, aka LMS, Version: 8.2.0 (August 2021)
const char *URL="http://192.168.1.228/mp3/snow.mp3";

AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
AudioFileSourceBuffer *buff;
AudioOutputI2SNoDAC *out;


// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void) isUnicode; // Punt this ball for now
  // Note that the type and string may be in PROGMEM, so copy them to RAM for printf
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1)-1]=0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2)-1]=0;
  Serial.printf("METADATA(%s) '%s' = '%s'\n", ptr, s1, s2);
  Serial.flush();
}



// Called when there's a warning or error (like a buffer underflow or decode hiccup)
void StatusCallback(void *cbData, int code, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  // Note that the string may be in PROGMEM, so copy it to RAM for printf
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1)-1]=0;
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  Serial.flush();
}


void setup() {
  delay(2000);
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  
  Serial.println("init wifi...");
  WiFi.disconnect();

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWD);

  // Try forever
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("...Connecting to WiFi");
    delay(1000);
  }
  Serial.println("Connected");

  audioLogger = &Serial;
  file = new AudioFileSourceICYStream(URL);

  // Commented out for performance issues with high rate MP3 stream
  file->RegisterMetadataCB(MDCallback, (void*)"ICY");

  buff = new AudioFileSourceBuffer(file, 8192);	// Doubled form default 2048

  // Commented out for performance issues with high rate MP3 stream
  buff->RegisterStatusCB(StatusCallback, (void*)"buffer");

  // Set SPDIF output
  out = new AudioOutputI2SNoDAC();
  mp3 = new AudioGeneratorMP3();

  // Commented out for performance issues with high rate MP3 stream
  mp3->RegisterStatusCB(StatusCallback, (void*)"mp3");
  mp3->begin(buff, out);

}


void loop() {
  static int lastms = 0;
  if (mp3->isRunning()) {
    if (millis()-lastms > 1000) {
      lastms = millis();
      Serial.printf("Running for %d ms...\n", lastms);
      Serial.flush();
     }
    if (!mp3->loop()) {
      mp3->stop();
    }
  } else {
    Serial.printf("MP3 done\n");
    // Restart ESP when streaming is done or errored
    delay(10000);
    //ESP.restart();
  }
}
