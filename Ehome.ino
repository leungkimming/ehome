// EmonLibrary examples openenergymonitor.org, Licence GNU GPL V3
#include "Arduino.h"
#include "EmonLib.h"
#include <TM1637Display.h>
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include "time.h"
#include "SD.h"
#include "SPI.h"
#include "Preferences.h"
//#include "soc/soc.h"
//#include "soc/rtc_cntl_reg.h"
#include "SPIFFS.h"
#include "esp_task_wdt.h"

#define TM1637_CLK 15
#define TM1637_DIO 16
#define CT_ADC 36
#define SD_CS_Pin 5
#define SD_MOSI_Pin 23
#define SD_MISO_Pin 19
#define SD_SCK_Pin 18
#define buzzer 26
#define cycle_factory 1480
#define max_sd_file_size 86300
// 1sec=17982, 5 cycles @ 50 Hz =100ms=1798 18cycle=300ms=5394 old=1480,1676

const int   led = LED_BUILTIN;
const char* ssid     = "feifei";
const char* password = "87080183";
const char* ntpServer = "hk.pool.ntp.org";
const long  gmtOffset_sec = 28800;
const int   daylightOffset_sec = 0;
const int   alert_factory = 3300; //33.00A
const int   CT_ADJ_factory = 600;
const int   changed_A = 50; // 0.5A
const uint8_t OFF[] = {0x00};

struct tm timeinfo;
EnergyMonitor emon1;                   // Create an instance
Preferences preferences;
hw_timer_t * timer = NULL;
TM1637Display display(TM1637_CLK, TM1637_DIO);
AsyncWebServer server(80);
File history;
File historyAction;
File root;

volatile byte state = LOW;
volatile int cycle_cnt = 0;
int Irms = 0;
int last_Irms = 0001; //avoid div by zero
int changed_duration = 10; // for 10 seconds
bool changed = false;
bool buzzer_ff = false;
int len, pos;
uint8_t dot;
double CT_ADJ;
String Webmessage;
int alert;
int cycle;
int reconnect_cnt = 0;
int start;
String currentHistory;

void IRAM_ATTR onTimer() {
  state = !state;
  cycle_cnt += 1;
}

void initwifi() {
  bool timesync = false;
  while (!timesync) {
    int retrycount = 0;
    Serial.print(ssid);
    WiFi.begin(ssid, password);      
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      retrycount += 1;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(WiFi.localIP());
      String s_ip = WiFi.localIP().toString();
      s_ip = s_ip.substring(s_ip.length() - 3, s_ip.length());
      int i_ip = s_ip.toInt();
      display.clear();
      display.showNumberDec(i_ip);
      delay(3000);
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      if (getLocalTime(&timeinfo)) {
        Serial.println(&timeinfo, "%Y/%m/%d %H:%M:%S");
        timesync = true;
      } else {
        delay(30000);
      }
    }
  }
}

String latestHistory()
{
  String fname;
  String suffix;
  String fnameMax = "/history.csv";
  int idxMax = 0;
  
  root = SD.open("/");
  while (true) {
    File entry =  root.openNextFile();
    if (! entry) {
      break;
    }
//    Serial.println(entry.name());
    fname = entry.name();
    if (fname.startsWith("/history")) {
      suffix = fname.substring(8);
      if (suffix != "") {
        if (suffix.toInt() > idxMax) {
          idxMax = suffix.toInt();
          fnameMax = fname;
        }
      }
    }
    entry.close();
  }
  root.close();
  return fnameMax;
}

void newHistory()
{
  String fname;
  int count = 0;

  File currentCopy = SD.open(currentHistory);
  if (currentCopy.size() <= max_sd_file_size) {
    currentCopy.close();
//    Serial.println("history size not exceed");
    return;
  }
  Serial.println("history size exceeded");
  
  root = SD.open("/");
  while (true) {
    File entry =  root.openNextFile();
    if (! entry) {
      break;
    }
//    Serial.println(entry.name());
    fname = entry.name();
    if (fname.startsWith("/history")) {
      count++;
    }
    entry.close();
  }
  root.close();
   
  fname = "/history";
  fname += String(count);
  fname += ".csv";
  currentHistory = fname;
  historyAction = SD.open(fname, FILE_WRITE);
  if (historyAction) {
    historyAction.println("Date_Time,Amp");
    historyAction.close();
  }
  Serial.print("New History = ");
  Serial.println(fname);
}

void setup()
{
//  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  Serial.begin(115200);
  pinMode(led, OUTPUT); //working flashing
  pinMode(buzzer, OUTPUT); //busser
  digitalWrite(buzzer, true);
  delay(500);
  digitalWrite(buzzer, false);
  
  preferences.begin("Ehome", false);
  int i_CT_ADJ = preferences.getUInt("CT_ADJ", CT_ADJ_factory);
  CT_ADJ = i_CT_ADJ / 10.0;
  Serial.print("CT_ADJ=");
  Serial.println(CT_ADJ);
  alert = preferences.getUInt("ALERT", alert_factory);
  Serial.print("ALERT=");
  Serial.println(alert);
  cycle = preferences.getUInt("CYCLE", cycle_factory);
  Serial.print("CYCLE=");
  Serial.println(cycle);

  display.setBrightness(4);
  display.clear();

  digitalWrite(led, true);
  initwifi();  
  digitalWrite(led, false);
  digitalWrite(buzzer, true);
  delay(500);
  digitalWrite(buzzer, false);

  analogSetAttenuation(ADC_11db);
  emon1.current(CT_ADC, CT_ADJ);             // 36, Current: input pin, calibration. 60, 11.7

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000000, true);
  timerAlarmEnable(timer);

  if (!SD.begin(SD_CS_Pin)) {
    Serial.println("SD failed");
  }
  if (!SPIFFS.begin(true)) {
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
  }
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  Serial.println(file.name());
  currentHistory = latestHistory();
  Serial.print("Historyfile = ");
  Serial.println(currentHistory);
  digitalWrite(led, true);

  cycle_cnt = -10; // let stablize

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    getLocalTime(&timeinfo);
    char timeStringBuff[22];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y/%m/%d %H:%M:%S", &timeinfo);
    Webmessage = timeStringBuff;
    Webmessage += " - ";
    Webmessage += Irms / 100.00;
    Webmessage += "A";
    request->send(200, "text/plain", Webmessage);
  });
  
  server.on("/history", HTTP_GET, [](AsyncWebServerRequest *request){
    File sourceFile;
    if(request->hasParam("value")) {
      String reqfile = request->getParam("value")->value();
      Serial.println(reqfile);
      sourceFile = SD.open(reqfile);
      if (!sourceFile) {   
        Webmessage = reqfile + " not found";
        request->send(200, "text/plain", Webmessage);
      }
    } else {
      sourceFile = SD.open(currentHistory);    
    }

    if (sourceFile) {
      File destFile = SPIFFS.open("/history.csv", FILE_WRITE);
      Serial.println(sourceFile);
      Serial.println(sourceFile.size());
      static uint8_t buf[4096];
      size_t n;
      while ( (n = sourceFile.read( buf, sizeof(buf))) > 0 ) {
        destFile.write( buf, n );
//        Serial.print(".");
        esp_task_wdt_reset();
      }
//      Serial.print("+");
      destFile.close();
      sourceFile.close();
      request->send(SPIFFS, "/history.csv", "text/plain");
    }
  });

  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request){
    Webmessage = "List of History Files: \n";
    File root = SD.open("/");
    while (true) {
      File entry =  root.openNextFile();
      if (! entry) {
        break;
      }
//      Serial.println(entry.name());
      String fname = entry.name();
      if (fname.startsWith("/history")) {
        Webmessage += fname;
        Webmessage += '\n';
      }
      entry.close();
    }
    root.close();
    request->send(200, "text/plain", Webmessage);
  });

  server.on("/ctadj", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("value")) {
      String s_ctadj = request->getParam("value")->value();
      Serial.println(s_ctadj);
      double d_ctadj = s_ctadj.toDouble();
      if (d_ctadj > 0) {
        CT_ADJ = d_ctadj;
        preferences.putUInt("CT_ADJ", (CT_ADJ * 10));
        emon1.current(CT_ADC, CT_ADJ);   
        Webmessage = "CT ADJ set to " + String(CT_ADJ, 1);
      } else {
        Webmessage = "Input Error";
      }
    } else {
      Webmessage = "No input";      
    }
    request->send(200, "text/plain", Webmessage);     
  });

  server.on("/alert", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("value")) {
      String s_alert = request->getParam("value")->value();
      Serial.println(s_alert);
      int d_alert = s_alert.toInt();
      if (d_alert > 0) {
        alert = d_alert;
        preferences.putUInt("ALERT", alert);
        Webmessage = "ALERT set to " + String(alert);
      } else {
        Webmessage = "Input Error";
      }
    } else {
      Webmessage = "No input";      
    }
    request->send(200, "text/plain", Webmessage);     
  });

  server.on("/cycle", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("value")) {
      String s_cycle = request->getParam("value")->value();
      Serial.println(s_cycle);
      int d_cycle = s_cycle.toInt();
      if (d_cycle > 0) {
        cycle = d_cycle;
        preferences.putUInt("CYCLE", cycle);
        Webmessage = "CYCLE set to " + String(cycle);
      } else {
        Webmessage = "Input Error";
      }
    } else {
      Webmessage = "No input";      
    }
    request->send(200, "text/plain", Webmessage);     
  });

  server.begin();
  digitalWrite(led, false);
  digitalWrite(buzzer, true);
  delay(500);
  digitalWrite(buzzer, false);
}

void loop()
{
  Serial.print("Cycle=");
  start = millis();
  Irms = round(emon1.calcIrms(cycle) * 100.00);
  Serial.print(millis() - start);
  Serial.print(",Irms=");
  Serial.print(Irms / 100.00);
  Serial.print(", last_Irms=");
  Serial.print(last_Irms / 100.00);
  Serial.print(", Diff=");
  Serial.print(abs(Irms - last_Irms));
  Serial.print(", cycle_cnt=");
  Serial.print(cycle_cnt);
  changed = false;
//  if ( (abs(Irms - last_Irms) * 1.00 / last_Irms * 1.00) > changed_pct) { //diff > x%
  if (abs(Irms - last_Irms) > changed_A) { //diff > x A * 100
    if (cycle_cnt > changed_duration) { //diff > x% for y seconds
      changed = true;
      last_Irms = Irms;
    }
  } else {
    cycle_cnt = 0;
  }  
  Serial.print(", changed=");
  Serial.print(changed);
  if (changed) {
    display.clear();
    showIrms(last_Irms);
    getLocalTime(&timeinfo);
    newHistory();
    history = SD.open(currentHistory, FILE_APPEND);
    if (history) {
      history.print(&timeinfo, "%Y/%m/%d %H:%M:%S");
      history.print(",");
      history.println(last_Irms / 100.00);
      history.close();
      Serial.print(&timeinfo, "%Y/%m/%d %H:%M:%S");
      Serial.print(",");
      Serial.println(last_Irms / 100.00);
    } else {
      Serial.println("SD error");        
    }
  }
  Serial.print(", wifi=");
  Serial.println(WiFi.status());

  digitalWrite(led, false);
  if (WiFi.status() != WL_CONNECTED) {
    reconnect_cnt += 1;
    if (reconnect_cnt > 30) {
      digitalWrite(led, true);
      //WiFi.disconnect();
      WiFi.reconnect();
      Serial.println("Reconnecting...");  
      reconnect_cnt = 0;
    }
  }
      
  if (Irms != last_Irms) {
    showIrms(Irms);   
  }
  
  if (last_Irms >= alert) {
    if (state) {
      digitalWrite(buzzer, true);      
    } else {
      digitalWrite(buzzer, false);
    }
  } else {
    digitalWrite(buzzer, false);                
  }

  delay(1000);
}

void showIrms(double value) {
  len = 3;
  pos = 1;
  dot = 0x80;
  if (value > 999) {
    len = 4;
    pos = 0;
    dot = 0x40;
  } else {
    display.setSegments(OFF, 1, 0);
  }
  display.showNumberDecEx(value, dot, true, len, pos);
}
