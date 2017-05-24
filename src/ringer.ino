#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <PubSubClient.h>
#include "user_interface.h"
#include "i2s.h"

// If serial debug is on then no sound is played
//#define SERIAL_DEBUG

#include "../../pwd/WIFI_AndreEnJantina.c"
const char* mqtt_server = "192.168.1.20";

WiFiClient client;
PubSubClient mqtt(client);
long setupStartMillis;

void checkForUpdate() {
  t_httpUpdate_return ret = ESPhttpUpdate.update("http://192.168.1.20/espupdate/update.php","doorbell.ringer.bin"); // Should be the same as in uploadfirmware.txt
#ifdef SERIAL_DEBUG
  switch(ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      break;
  }
#endif  
}

void setup()
{
#ifdef SERIAL_DEBUG
  setupStartMillis = millis();
  Serial.begin(74880);
  Serial.println();
  Serial.print("Setup start millis: "); Serial.println(setupStartMillis);
  // Serial.print(", Mac: "); Serial.println(WiFi.macAddress());
  // Mac: 5C:CF:7F:F8:77:06
#endif

  WiFi.mode(WIFI_STA);
  // Serial.print("Connecting to: "); Serial.println(ssid);
    
  pinMode(2, OUTPUT);
  WiFi.begin(ssid, password);
  WiFi.config(IPAddress(192,168,1,131), IPAddress(192,168,1,1), IPAddress(255,255,255,0));
  
  int connectCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if ((connectCount&0xFF) == 0x00)
      digitalWrite(2, HIGH); 
    if ((connectCount&0xFF) == 0x80)
      digitalWrite(2, LOW); 
    connectCount++;
    delay(1);
  }
#ifdef SERIAL_DEBUG
   Serial.print("WiFi connected in: "); Serial.println(millis()-setupStartMillis);
   //Serial.print(", IP: "); Serial.print(WiFi.localIP());
   //Serial.print(", Mac: "); Serial.print(WiFi.macAddress());
   //Serial.println();
#endif
       
  checkForUpdate();

  digitalWrite(2, HIGH); 
  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(mqttMessage);
}

// OK no sin but a square wave, sin is to slow
float ICACHE_RAM_ATTR sin(float x) {
  x -= 6.28f*(int)(x / 6.28f);
  return x>3.14f?0.6f:-0.6f;
}

// Aproximation of exp used to make ding no math used just guesswork
float ICACHE_RAM_ATTR exp(float x) {
  if (x<-4.0f)
    return 0.0f;
  x = (4.0f+x)/4.0f;
  return x*x;
}

// Only for positive values
float ICACHE_RAM_ATTR mod(float x, float y) {
  return x - ((int)(x / y) * y);
}

bool doPlayDingDong = false;
const uint32_t sampleRate = 32575;

const int maxMelodyLength = 32;
int melodyLength = 2;
int melody[] = {760, 600, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                  0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void ICACHE_RAM_ATTR PlayDingDong() {
    const uint32_t fakePwm[] = 
    { 
      0x00000001, 0x00010001, 0x00030001, 0x00030003, 0x00070003, 0x00070007, 0x000F0007, 0x000F000F, 
      0x000F001F, 0x001F001F, 0x003F001F, 0x003F003F, 0x007F003F, 0x007F007F, 0x00FF007F, 0x00FF00FF, 
      0x00FF01FF, 0x01FF01FF, 0x03FF01FF, 0x03FF03FF, 0x07FF03FF, 0x07FF07FF, 0x0FFF07FF, 0x0FFF0FFF, 
      0x0FFF1FFF, 0x1FFF1FFF, 0x3FFF1FFF, 0x3FFF3FFF, 0x7FFF3FFF, 0x7FFF7FFF, 0xFFFF7FFF, 0xFFFFFFFF
    };
#ifdef SERIAL_DEBUG
    Serial.print("Play: "); Serial.println(melodyLength);
    Serial.print("Frequencies: ");
    for (int i = 0; i< melodyLength; i++) {
      Serial.print(" ");
      Serial.print(melody[i]);
    }
    Serial.println();
#else
    i2s_begin();
    i2s_set_rate(sampleRate);
#endif    
    uint32_t iSample;

    float sample;
    uint32_t notes = (melodyLength+1);
    uint32_t sampleCount = notes*sampleRate/2;
    for (uint32_t i = 0; i < sampleCount; i++)
    {
      float tm = ((float)i)/sampleRate;
      uint32_t note = i*2/sampleRate;
      
      float f1 = (            note< melodyLength)?melody[note  ]:0;
      float f2 = (note>1) && (note<=melodyLength)?melody[note-1]:0;

      sample = 0;
      if (f1>0.0f) {
        float tm1 = mod(tm, 0.5f);
        sample += sin(6.2831f*f1*tm) * exp(-3.3f*tm1);
      }
       
      if (f2>0.0f) {
        float tm2 = mod(tm, 0.5f) + 0.5f;
        sample += sin(6.2831f*f2*tm) * exp(-3.3f*tm2);
      }
        
#ifdef SERIAL_DEBUG
      if ((i & 0x00000fff)==0)
        yield();
#else
      iSample = fakePwm[(byte)(15.5f+15.4f*(sample<-1.0?-1.0:sample>1.0?1.0:sample))];
      while (!i2s_write_sample_nb(iSample))
        yield();
#endif        
    }
    
#ifndef SERIAL_DEBUG
    while ((sample -= 0.001)>0.01) {
      iSample = fakePwm[(byte)(15.5f+15.4f*(sample<-1.0?-1.0:sample>1.0?1.0:sample))];
      while (!i2s_write_sample_nb(iSample))
        yield();
    }
    
    while (!i2s_write_sample_nb(0))
      yield();
      
    while (!i2s_is_empty())
      yield();
    
    i2s_end();
    digitalWrite(3, LOW);
    pinMode(3, OUTPUT);
    digitalWrite(3, LOW);
#endif        
}

const char* melodyTrigger = "melody";
const int melodyTriggerLength = 6;

bool fillMelody(char* payload, int length)
{
  bool melodyFound = false;
  int i = 0;
  for (; i < length-melodyTriggerLength; i++) {
    if (memcmp(melodyTrigger, payload+i, melodyTriggerLength)==0) {
      // printf("melody found at %d\n",i);
      i += melodyTriggerLength;
      melodyFound = true;
      break;
    }
  }

  if (melodyFound) {
    bool arrayFound = false;
    for (; i < length; i++) {
      char c = payload[i];
      if (c=='[') {
        i++;
        arrayFound = true;
        break;
      } else
        if (c==']')
          break;
    }

    if (arrayFound) {
      melodyLength = 0;
      int number = 0;
      bool nFound = false;
      for (; i < length; i++) {
        char c = payload[i];
        if (c>='0' && c<='9') {
          number *= 10;
          number += c-48;
          nFound = true;
        } else {
          if (nFound) {
            // printf("%d\n",number);
            if (melodyLength<maxMelodyLength)
              melody[melodyLength++] = number;
          }
          number = 0;
          nFound = false;
        }

        if (c==']') {
          melody[melodyLength++] = 0;
          return true;
        }
      }
    }
  }

  return false;
}

void mqttMessage(char* topic, byte* payload, unsigned int length) {
#ifdef SERIAL_DEBUG
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
#endif
  if (!fillMelody((char*)payload, length))
  {
    melodyLength = 2;
    melody[0] = 760;
    melody[1] = 600;
    melody[2] = 0;
  }
  doPlayDingDong = true;
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
#ifdef SERIAL_DEBUG
    Serial.print("Attempting MQTT connection...");
#endif
    // Attempt to connect
    if (mqtt.connect("doorbell_ringer_NOT")) {
#ifdef SERIAL_DEBUG
      Serial.println("connected");
#endif
      // ... and resubscribe
      mqtt.subscribe("stat/doorbell/pressed");
    } else {
#ifdef SERIAL_DEBUG
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
#endif
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  if (!mqtt.connected()) {
    reconnect();
  }
  mqtt.loop();
  if (doPlayDingDong) {
    doPlayDingDong = false;
#ifdef SERIAL_DEBUG
  Serial.println("Play DingDong");
#endif
    PlayDingDong();
#ifdef SERIAL_DEBUG
  Serial.println("DingDong played");
#endif
  }
}
