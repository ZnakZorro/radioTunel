
#include "Arduino.h"
#include <vector>
#include "WiFi.h"
#include "WiFiMulti.h"
#include "DNSServer.h"
#include "AsyncTCP.h"             //https://github.com/me-no-dev/ESPAsyncTCP 
#include "ESPAsyncWebServer.h"    //https://github.com/me-no-dev/ESPAsyncWebServer
#include "ESPAsyncTunnel.h"
#include "HTTPClient.h"


#include "SPI.h"
//#include "SPIFFS.h"
#include "SD.h"
#include "FS.h"
#include "Wire.h"

#include "ES8388.h"  // https://github.com/maditnerd/es8388
#include "Audio.h"   // https://github.com/schreibfaul1/ESP32-audioI2S
#include "credentials.h"
#include "extra.h"
//#include "index.h"
//#include "vue.h"
//#include "web.h"
#include "ESPio.h"


#define SD_CS         21

// GPIOs for SPI
#define SPI_MOSI      13
#define SPI_MISO      12
#define SPI_SCK       14

// I2S GPIOs
#define I2S_SDOUT     26
#define I2S_BCLK       5
#define I2S_LRCK      25
#define I2S_MCLK       0

// I2C GPIOs
#define IIC_CLK       23
#define IIC_DATA      18

// Amplifier enable
#define GPIO_PA_EN    19

#define LED_BUILTIN    22
#define cur_volume_DEF  3
#define LAST_STATION    5

int start_volume        = 75;  // 0...100
int cur_volume    = cur_volume_DEF;
int cur_station   = 1;

int cur_equalizer = 0;


#define coIleSekund  3
unsigned long secundDelay = 1000*coIleSekund;  
unsigned long lastSecundDelay = 0;  
unsigned long licznik = 0;

unsigned long timerSLEEP = 1000 * 60 * 60 * 2;  // 2 godziny
unsigned long LastTimerSLEEP = 0;
unsigned long timerDelay15m = 1000 * 60 *15*3;  // 15minut * 3 = 45
unsigned long lastTime = 0;
unsigned long millisy = 0;

String LCD_Buffer1="                ";
String LCD_Buffer2="                ";

String jsonBuffer;
int loopa=0;
//String linia2="";
char   znakS = (char)223;


bool isSD        = false;

String hostURL = "http://stream.rcs.revma.com/ypqt40u0x1zuv";
String hostNAME = "RNS";

char extraInfo[32];// = hostURL;

char* indexPath = "/radioTunel/public/cartoon.html";

WiFiMulti wifiMulti;
ES8388 es;
Audio audio;
ESPio clio;
AsyncWebServer server(80);

#include <Preferences.h>
Preferences preferences;

//----------------------------------------------------------------------------------------------------------------------
void onScreens(const char *linia1, const char *linia2, int lineNR){
  //clio.drukLCD(linia2);
  Serial.printf("==========================\n %u :: %s:: %s\n", lineNR,linia1,linia2);
}

String padZero(int nr){
  if (nr>9) return String(nr);
  else return "0"+String(nr);
}

//+1 minuta ???? 30301

//unsigned long sekCount = 0;

void pogoda2LCD(){
    String isRun = audio.isRunning() ? "play":"stop";
    //if (!audio.isRunning()) playCurStation();
      
      /*Serial.print("\n======================="); 
      for (int n=0;n<7;n++) {
          Serial.print("---"); Serial.print(n); Serial.print(" = "); Serial.println(clio.linieLCD[n]);
      }*/
      //Serial.print("1= "); Serial.println(clio.linieLCD[1]);
      //Serial.print("2= "); Serial.println(clio.linieLCD[2]);
      clio.println(clio.linieLCD[loopa],1);
      if (loopa==0) {
          
          clio.println(LCD_Buffer1,1);
          //clio.println(LCD_Buffer2,0);
      }
      //if (loopa==1) clio.println(LCD_Buffer1,0);
      //if (loopa==2) clio.println(LCD_Buffer2,0);
      if (loopa==2) clio.println(clio.getClock(),0);
      
        
      loopa = (loopa+1) % 7;      
}


String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, serverName);
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();
  return payload;
}



void getYrnoPogoda(){
  Serial.println("Pogoda...");
  //clio.drukLCD("Pogoda...");
  clio.println("Pogoda...",1);
     if(WiFi.status()== WL_CONNECTED){
      String serverPath = proxyHost;
      Serial.println(serverPath);
      jsonBuffer = httpGETRequest(serverPath.c_str());
      Serial.println(jsonBuffer); 
      clio.payload2LCD(jsonBuffer);    
    }
    else {
      Serial.println("WiFi Disconnected");
    }
  
}


void savePreferences(){
         preferences.begin("my-app", false);
         preferences.putUInt("cur_volume", cur_volume);
         preferences.putUInt("cur_station", cur_station);
         preferences.putUInt("cur_equalizer", cur_equalizer);  
         preferences.putString("hostURL", hostURL);     
         preferences.end();
         LCD_Buffer1 = "Vol="+String(cur_volume) + ", Sta="+String(cur_station);
}  
  
void es_volume(int volum){
    es.volume(ES8388::ES_MAIN, volum);
    es.volume(ES8388::ES_OUT1, volum);
    es.volume(ES8388::ES_OUT2, volum);
}

unsigned long millisTEST = 0;

void audioStop(){
    Serial.println("#192 audioStop"); 
    es_volume(0);
    audio.setVolume(0);
    audio.stopSong();
    Serial.println(audio.isRunning());
    
    millisTEST = millis();
}

void audioStart(){
    Serial.println("#201 audioStart"); 
    //Serial.println(millis() - millisTEST); 
    audio.connecttohost(hostURL.c_str()); 
    es_volume(start_volume);
    audio.setVolume(cur_volume);
    LCD_Buffer2 = hostNAME;
    Serial.print("millis=");Serial.println(millis() - millisTEST);
    Serial.print("isRun =");Serial.println(audio.isRunning());
}

           

void setup()
{
  Serial.begin(115200);
    clio.initLED(LED_BUILTIN);
    clio.ledled();    
    clio.initLCD(IIC_DATA, IIC_CLK);
    //clio.drukLCD("RADIO");
    clio.println("RADIO...",0);
          preferences.begin("my-app", false);
          cur_station   = preferences.getUInt("cur_station", 1);
          cur_volume    = preferences.getUInt("cur_volume", cur_volume_DEF);
          cur_equalizer = preferences.getUInt("cur_equalizer", 0);
          hostURL       = preferences.getString("hostURL","http://pl-play.adtonos.com/tok-fm");
          Serial.println("#224 preferences.hostURL==="); 
          Serial.println(hostURL); 
          Serial.println(" ===========");
          String sName0  = preferences.getString("sName0","tok-fm");
          String sUrl0   = preferences.getString("sUrl0","http://pl-play.adtonos.com/tok-fm");
          Serial.println(sName0); 
          Serial.println(sUrl0);
          Serial.println(" ===========");
          preferences.end();
 
    Serial.println("\r\nStart...");
    Serial.printf_P(PSTR("Free mem=%d\n"), ESP.getFreeHeap());

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    SPI.setFrequency(1000000);
    SD.begin(SD_CS);

      if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
      } else{
        //readDATA();
        //Serial.println("");
        //Serial.println("readDATA EOF");
      }
    
    //WiFi.mode(WIFI_OFF);
    //WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
      for (int i = 0; i < 8; i++) {
         wifiMulti.addAP(credential[i].ssid, credential[i].pass);
      } 
    while(wifiMulti.run() != WL_CONNECTED) {
      Serial.print("*");
      licznik++;
      //clio.drukLCD("WiFi::"+String(licznik));
      clio.println("WiFi::"+String(licznik),0);
      delay(333);
      if (licznik>50) ESP.restart();
      } 
    //clio.drukLCD("Conected..."); 
    //clio.println("WiFi",1); 
    //delay(800);
    Serial.printf_P(PSTR("Connected\r\nRSSI: "));
    Serial.print("WiFi.status="); Serial.println(WiFi.status());
    Serial.print("WiFi.RSSI=");   Serial.println(WiFi.RSSI());
    Serial.print("IP: ");         Serial.println(WiFi.localIP());
    Serial.print("SSID: ");       Serial.println(WiFi.SSID());
    
        clio.ppp(WiFi.localIP());
        clio.println(String(WiFi.RSSI())+" "+ String(WiFi.SSID()),1);
        delay(2900);
        
        
    Serial.printf("Connect to ES8388 codec... ");
    while (not es.begin(IIC_DATA, IIC_CLK)){
        Serial.printf("Failed!\n");
        delay(1000);
    }
    Serial.printf("OK\n");
    es_volume(0);   
    es.mute(ES8388::ES_OUT1, false);
    es.mute(ES8388::ES_OUT2, false);
    es.mute(ES8388::ES_MAIN, false);

    // Enable amplifier
    pinMode(GPIO_PA_EN, OUTPUT);
    digitalWrite(GPIO_PA_EN, HIGH);

    audio.setPinout(I2S_BCLK, I2S_LRCK, I2S_SDOUT);
    audio.i2s_mclk_pin_select(I2S_MCLK);
    audio.setVolume(cur_volume); // 0...21
    audio_SetEQNr(String(cur_equalizer));
 
       getYrnoPogoda(); 
      delay(333);
      //listFileFromSPIFFS();
      //audio.connecttohost(hostURL.c_str());
      //audio.connecttoFS(SPIFFS, "/r.mp3");
      //playCurStation();
      LastTimerSLEEP = millis();
      snprintf(extraInfo, 32, hostURL.c_str());
      installServer();
      LCD_Buffer1 = "Vol="+String(cur_volume) + ", Sta="+String(cur_station);
}
// setup end ----------------------------------------------------------------------------------------------------------------------


void loop()
{
    millisy = millis();
    audio.loop();

  // timesleep time to sleep
  if ((millisy - LastTimerSLEEP) > timerSLEEP) {
      Serial.println("Sleep !!!!!!!!!!!!!!!!!!!");  
      esp_reBootSleep("2");
      LastTimerSLEEP = millis();
  }
  
  if ((millisy - lastTime) > timerDelay15m) {
      getYrnoPogoda(); 
      Serial.println("Pogoda --------------");  
      lastTime = millis();
  }
  
  if ((millisy - lastSecundDelay) > secundDelay) {
    //Serial.println("pogoda2LCD --------------"); 
      pogoda2LCD();
      lastSecundDelay = millisy;
      if (audio.isRunning() == false) audioStart();
  }
    

                                //----------------------------------------------------------------------------------------------------------------------
                                if (Serial.available() > 0) {
                                    String odczyt = Serial.readString(); 
                                    odczyt.trim();
                                    int odczytInt = odczyt.toInt();                                     
                                      //Serial.println("\noooooooooooooooo");
                                      Serial.println(odczyt);
                                      //Serial.println(odczytInt);
                                    if(odczyt.length()>5) {
                                      audio.stopSong();
                                      audio.connecttohost(odczyt.c_str());
                                      log_i("free heap=%i", ESP.getFreeHeap());
                                    }
                                    if (odczyt == "-9") {Serial.println("-9x");ESP.restart();}
                                    /*if (odczytInt>0 && odczytInt<3) {
                                      Serial.println(odczytInt);
                                      cur_station = odczytInt-1;
                                      playCurStation();
                                      }*/
                                    //if (odczyt == "+") {cur_station++;playCurStation();}
                                    //if (odczyt == "-") {cur_station--;playCurStation();}
                                    //if (odczyt == "*") {cur_volume++;setCurVolume();}
                                    //if (odczyt == "/") {cur_volume--;setCurVolume();}
                                    
                                      if (odczyt == "pl") audio.connecttospeech("Dzień dobry. Litwo! Ojczyzno moja! ty jesteś jak zdrowie: Ile cię trzeba cenić, ten tylko się dowie, Kto cię stracił. Dziś piękność twą w całej ozdobie Widzę i opisuję, bo tęsknię po tobie.", "pl");
                                     
                                  
                                }
                                //----------------------------------------------------------------------------------------------------------------------


    
}
//----------------------------------------------------------------------------------------------------------------------

//optional
/*void audio_info(const char *info){
    Serial.print("info        "); Serial.println(info);
}*/
void audio_id3data(const char *info){  //id3 metadata
    Serial.print("id3data     ");Serial.println(info);
}
void audio_eof_mp3(const char *info){  //end of file
    Serial.print("-----eof_mp3     ");Serial.println(info);
    //playCurStation();
}
void audio_showstation(const char *info){
  snprintf(extraInfo, 32, info);
    onScreens("audio_showstation::",String(info).c_str(),384);
    //LCD_Buffer1 = String(info).substring(0,16);
    //LCD_Buffer2 = String(info).substring(16,32);
    es_volume(start_volume);
}
void audio_showstreaminfo(const char *info){
  snprintf(extraInfo, 32, info);
    //Serial.print("streaminfo  ");Serial.println(info);
    onScreens("streaminfo::",String(info).c_str(),392);
    //LCD_Buffer1 = String(info).substring(0,16);
    //LCD_Buffer2 = String(info).substring(16,32);
}
void audio_showstreamtitle(const char *info){
  snprintf(extraInfo, 32, info);
  LCD_Buffer1 = String(info).substring(0,16);
    //Serial.print("streamtitle ");Serial.println(info);
    onScreens("Streamtitle::",String(info).c_str(),399);
}
void audio_bitrate(const char *info){
    Serial.print("bitrate     ");Serial.println(info);
}
void audio_commercial(const char *info){  //duration in sec
    Serial.print("commercial  ");Serial.println(info);
}
void audio_icyurl(const char *info){  //homepage
    Serial.print("icyurl      ");Serial.println(info);
}
void audio_lasthost(const char *info){  //stream URL played
    //Serial.print("lasthost    ");Serial.println(info);
    onScreens("Lasthost::",String(info).c_str(),412);
}
void audio_eof_speech(const char *info){
    Serial.print("eof_speech  ");Serial.println(info);
}


//inforadio info radio
String getRadioInfo(){
  const String      sep = "!"; 
  String v = String(cur_volume);
  String q = String(cur_equalizer);
  String n = String(cur_station);
  String s = hostNAME;    //String(clio.radia[cur_station].info);
  String ri    = String(WiFi.RSSI());
    LCD_Buffer1 = extraInfo;
    //LCD_Buffer2 = s;
    String czas = String((timerSLEEP/(1000*60)));
  return n+sep+v+sep+ri+sep+s+sep+String(extraInfo)+sep+q+sep+czas+sep+hostURL;
/*
0: "0"
1: "3"
2: "-49"
3: "TOK-FM"
4: "Radio Nowy Świat - Pion i poziom"
5: ""
6: "1"
7: "http://stream.rcs.revma.com/ypqt40u0x1zuv"
*/  
}


void setCurVolume(){
    if (cur_volume < 0)  cur_volume = 0;
    if (cur_volume > 19) cur_volume = 19;
    //int ampli = clio.radia[cur_station].ampli;
    //if (cur_volume<2) ampli = 0;
    audio.setVolume(cur_volume); // 0...21
    Serial.println("#464 ?????Vol="+String(cur_volume));
    //LCD_Buffer2 = "Vol="+String(cur_volume);
    //LCD_Buffer2 = "Sta="+String(cur_station);
    //clio.println(LCD_Buffer1,0);
        //clio.drukLCD("Vol="+String(cur_volume));
        //String  printext = String(cur_volume);
        //printext = "Vol="+printext;
        //clio.println(printext,0);
        //clio.println(printext.c_str(),1);
        //clio.drukLCD(printext);
    savePreferences();
}

//ajax
void audio_ChangeVolume(String ParamValue){
  LastTimerSLEEP = millis()-60000;
    if (ParamValue=="p") cur_volume++;
    if (ParamValue=="m") cur_volume--;
    if (ParamValue=="*") cur_volume++;
    if (ParamValue=="-") cur_volume--;
    setCurVolume();
}


void audio_SetIncreaseStation(int delta){
    cur_station += delta;
    if (cur_station<0) cur_station=LAST_STATION;
    if (cur_station>LAST_STATION) cur_station=0; 
    Serial.print("#467 cur_station=");Serial.println(cur_station);
    audio_ChangeStation(String(cur_station));
    //playCurStation(); ---usun ---won
}

//#define isint(X) (!((X)==(X)))

void audio_SetStationUrl(const String ParamValue){
  Serial.println("#575 xxxxxxxxxxxxxxxxxxx---won"); 
        hostURL = ParamValue;
        
          Serial.println("#478 audio_SetStationUrl hostURL==="); 
          Serial.println(hostURL); 

      //onScreens(("Url="+String(ParamValue)),483);
      audio.stopSong();
      es_volume(0);
      audio.setVolume(0);
      delay(333);
      audio.connecttohost(ParamValue.c_str());
      audio.setVolume(cur_volume);
      es_volume(start_volume);
      savePreferences();   
}

void audio_ChangeStation(String ParamValue){
    audioStop();
    
    LastTimerSLEEP = millis()-60000;
    Serial.println("#496 audio_ChangeStation.............");
    Serial.println(ParamValue);
    cur_station = ParamValue.toInt();
          preferences.begin("my-app", false);
            String keyName = "sName" + ParamValue;
            String keyUrl  = "sUrl"  + ParamValue;
            String sName  = preferences.getString(keyName.c_str(),"tok-fm");
            String sUrl   = preferences.getString(keyUrl.c_str(),"http://pl-play.adtonos.com/tok-fm");
            Serial.print("#504 sName=");  Serial.print(sName);  Serial.print(" sUrl= "); Serial.println(sUrl);
            hostURL = sUrl;
            hostNAME = sName;
          preferences.end();
        savePreferences();
        //clio.drukLCD("Sta="+String(cur_station));
        //clio.println("Sta="+String(cur_station),0);
        //LCD_Buffer2 = "Sta="+String(cur_station);
      audioStart();
}

void audio_SetEQNr(String ParamValue){
      cur_equalizer = ParamValue.toInt();
      //onScreens(String(cur_equalizer).c_str(),477);    
      audio.setTone(qqq[cur_equalizer].l, qqq[cur_equalizer].m, qqq[cur_equalizer].h);
}
void audio_SetEQ(String ParamValue){
      Serial.println(ParamValue);
      int arr[7] = {};
      for (int i=0; i<7; i++){
        String l = clio.splitValue(ParamValue, ';',i);
        arr[i] = l.toInt();
        //Serial.print(i);Serial.print("=");Serial.println(arr[i]);
      }
      audio.setTone(arr[0],arr[1],arr[2]);
      //audio.newTone(arr[0],arr[1],arr[2],arr[3],arr[4],arr[5],arr[6]);
      //????????????????????????????????????????????????????????????????????????????????????????????????????
}


void ESP_sleep_za(const String ParamValue){
    unsigned long newTime = ParamValue.toInt(); // time in minutes
    timerSLEEP = newTime * 60 *1000;
    Serial.println("#535 new Sleep "+String(timerSLEEP));
    //clio.println("Sleep "+String(ParamValue),1);
    LCD_Buffer2="Sleep "+String(ParamValue);
}

void esp_reBootSleep(const String ParamValue){
    if (ParamValue=="0") { 
        audio.setVolume(0);
        audio.stopSong();
        es_volume(0); 
        delay(1500); 
        ESP.restart();
    } // reboot
    if (ParamValue=="1") {audioStop(); delay(500);audioStart();} //replay
    if (ParamValue=="2") {
        audio.stopSong();
        es_volume(0); 
        String czas = String((timerSLEEP/(1000*60)));
        //clio.drukLCD("Sleep " + czas);
        clio.println("Sleep " + czas,0);
        clio.println(clio.getClock(),1);
        delay(500); 
        esp_sleep_enable_ext0_wakeup(GPIO_NUM_18,0);
        esp_deep_sleep_start();
    } //sleep
  
}

/************* SERVER *******************/
/************* SERVER *******************/
/************* SERVER *******************/


void installServer(){
  clio.println("installServer...",1);

  Serial.print("#571 indexPath=");
  Serial.println(indexPath);
  server.rewrite( "/", indexPath) ;
  server.rewrite( "/index.html", indexPath);

   // tunnel the index.html request 
  server.on(indexPath, HTTP_GET, [&](AsyncWebServerRequest *request){
    audioStop();
      ClientRequestTunnel tunnel; 
      if (tunnel.open("https://znakzorro.github.io", request->url())) {
          String result = tunnel.getString();
          request->send(200, "text/html", result); 
          audioStart();       
      } else {
          request->send(tunnel.getHttpCode());
      }
  });

  /*server.on("/public/*", HTTP_GET, [&](AsyncWebServerRequest *request){
    
    String moved_url = "https://znakzorro.github.io"+request->url();
    Serial.println(moved_url);
    request->redirect(moved_url);
  });*/


///////////////////////////////////////////


  
  // AJAXY *************************
  // AJAXY *************************
  // AJAXY *************************
  server.on("/key", HTTP_GET, [](AsyncWebServerRequest *request){
      int params = request->params();
      AsyncWebParameter* p0 = request->getParam(0); 
      AsyncWebParameter* p1 = request->getParam(1); 
      AsyncWebParameter* p2 = request->getParam(2); 
      String name0  = String(p0->name());
      String name1  = String(p1->name());
      String name2  = String(p2->name());
      String val0   = p0->value();
      String val1   = p1->value();
      String val2   = p2->value();
       
      //Serial.print("#710 name= ");Serial.print(name0.c_str()); Serial.print("="); Serial.println(val0.c_str());
      //Serial.print("#711 val = ");Serial.print(name1.c_str()); Serial.print("="); Serial.println(val1.c_str());
      //Serial.print("#712 nr  = ");Serial.print(name2.c_str()); Serial.print("="); Serial.println(val2.c_str());
      //Serial.println();

      String nr       = val2;
      String nameName = "sName"+nr;
      String nameVal  =  val0;
      String strmName = "sUrl"+nr;
      String strmVal  =  val1;
      Serial.print("#626  ");Serial.print(nameName); Serial.print(" = "); Serial.println(nameVal);
      Serial.print("#627  ");Serial.print(strmName); Serial.print(" = "); Serial.println(strmVal);
      Serial.println();
         preferences.begin("my-app", false);
         preferences.putString(nameName.c_str(), nameVal.c_str());     
         preferences.putString(strmName.c_str(), strmVal.c_str());     
         preferences.end();
      
      request->send(200, "text/plain",getRadioInfo());
  });
  
  
  server.on("/radio", HTTP_GET, [](AsyncWebServerRequest *request){
    clio.ledled();
    
    //last_get_url = "";
    /******************************/
     //List all parameters
     int params = request->params();
     for(int i=0;i<params;i++){
          AsyncWebParameter* p = request->getParam(i); 
          //Serial.print("#705 get? ");Serial.print(i); Serial.print(". ");Serial.print(p->name().c_str()); Serial.print("="); Serial.println(p->value().c_str());
          
          String ParamName  = String(p->name());
          String ParamValue = p->value();               
         Serial.print("#651 ");Serial.print(ParamName);Serial.print("=");Serial.println(ParamValue);
         
           if (ParamName=="start") {audio.pauseResume();}
           if (ParamName=="v")  audio_ChangeVolume(ParamValue);  
           if (ParamName=="s")  audio_ChangeStation(ParamValue); 
           //if (ParamName=="t")  audio_SetStationNr(ParamValue);
           if (ParamName=="q")  audio_SetEQNr(ParamValue);
           if (ParamName=="qq")  audio_SetEQ(ParamValue);
           //if (ParamName=="z")  saveDATA(ParamValue); 
           if (ParamName=="x")  audio_SetStationUrl(ParamValue);
           if (ParamName=="r")  {esp_reBootSleep(ParamValue);}
           if (ParamName=="y")  ESP_sleep_za(ParamValue);
           //if (ParamName=="a")  saveStation2Preferences(ParamValue);
           if (ParamName=="nx")  audio_SetIncreaseStation(1);
           if (ParamName=="pr")  audio_SetIncreaseStation(-1);
           
            if (ParamName=="n") {  }
           
                     
           if (ParamName=="mp3")  {
                //playMp3(ParamValue);
           }

          if (ParamName=="w") {
              //zapiszSPIFFS("/ssid.txt",p->value().c_str());
              //Serial.print("#676 radio?w="); Serial.println(ParamValue);
          }

         // prog mem prog mem prog mem prog mem prog mem prog mem
           if (ParamName=="iir") {
            //zapiszSPIFFS("/eq.txt",p->value().c_str());
            
          } 

    }
 
    request->send(200, "text/plain",getRadioInfo());
  });
 
  server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request){
    bool gramy = audio.pauseResume();
    request->send(200, "text/plain",  getRadioInfo());
  });
 

  
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin();
  //initWebSocket(); 
  Serial.println("#700 Start SERVER");
  //audio.connecttohost(hostURL.c_str());
  // po uruchomieniu servera
  audioStart();
}


/*
Szkic używa 1 256 946 bajtów (39%) pamięci programu. Maksimum to 3145728 bajtów.
Zmienne globalne używają 52 636 bajtów (16%) pamięci dynamicznej, pozostawiając 275044 bajtów dla zmiennych lokalnych. Maksimum to 327680 bajtów.



 */
