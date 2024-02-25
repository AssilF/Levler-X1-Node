#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ESP8266TimerInterrupt.h> //some amazing man gave us this on github to compensate for the terribly designed timers on esp8266 . . . why is this unit this expensive comparing to esp32s price anyways >:( 
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

//Note: For the analog pins since we have only one analog interface, we're gonna use an analog multiplexer I guess. . . let's hope it doesn't interfere with the US readings . . .

//Dev Prameter:
#define VerboseON 1
#define debug(x) Serial.println(x); 

//Defs
#define SCR_WD   240
#define SCR_HT   240
#define TFT_DC    D1     // TFT DC  pin is connected to NodeMCU pin D1 (GPIO5)
#define TFT_RST   D2     // TFT RST pin is connected to NodeMCU pin D2 (GPIO4)
#define TFT_CS    D8     // TFT CS  pin is connected to NodeMCU pin D8 (GPIO15)

//concerning coms:
IPAddress local_IP(4,4,4,100);
IPAddress gateway(4,4,4,100);
IPAddress subnet(255,255,255,0);
const char* ssid     = "Levler@1";
const char* password = "ASCE-123#";

//Bismilah
//Concerning Objects
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS,TFT_DC,TFT_RST);

WebSocketsServer webSocket = WebSocketsServer(80);


//Concerning Pins:
#define indicator D0

//Concerning Functions = = = = = = = = = = = = = = = = = = = = = = = ======================
//Concerning indication
#define wink(intensity) analogWrite(indicator,intensity)
#define indication_sets_width 6

//LED indication
#define indicate_offline
#define indicate_disconnection
#define indicate_connection
#define indicate_transmission
#define indicate_reception
#define indicate_error

int blinkMillis[][indication_sets_width] 
{
    {0},
    {0}, //disconnected
    {100,100,200,200,100,100},//connected
    {400,100,600,300}, //transmission
    {0}, //reception
    {0} //error
};
int blinkIntensities[][indication_sets_width]
{
    {0},//offline
    {0},//disconnected
    {200,800,2000,800,200,0},//connected
    {20,0,100,0},//transmission
    {0},//reception
    {0}//error
}; //currently, we're not concerned with stuff other than connection status so. . . audio will do the rest.
int blinkPriorities[]
{
};
int blinkDefaults[]
{
};

int blinkIndex;
byte blinkMode;
unsigned long blinkInstance;

void blink(byte blinkStyle)
{
    if(blinkStyle != blinkMode)
    {
        blinkMode = blinkStyle;
        blinkIndex = 0;
    }
}

void performBlink()
{
        if(millis()-blinkInstance>= blinkMillis[blinkMode][blinkIndex])
        {
            wink(blinkIntensities[blinkMode][blinkIndex]);
            blinkIndex>4?blinkIndex=0:blinkIndex++;
            blinkInstance=millis();
        }
}




//Concerning Video
//Video core:
#define vertical_divisions 4
#define horizontal_divisions 5

void divideMenu()
{

}









//Concerning Audio  
//Audio Core
#define sound(freq) 

#define sound_partitions 6
#define sound_IDs 16

#define audio_scroll_up 0
#define audio_scroll_down 1
#define audio_value_inc 2
#define audio_value_dec 3
#define audio_boot 4
#define audio_shut_down 5
#define audio_home 6
#define audio_value_edit 7
#define audio_button_push 8
#define audio_cycle 9
#define audio_fail 10
#define audio_start 11
#define audio_finished 12
#define audio_connected 13
#define audio_disconnected 14
#define audio_webCMD 15

int sound_sets[sound_IDs][sound_partitions]={
{760},                  //scroll up 0
{560},                  //scroll down 1
{780,1290},             //inc 2
{1290,780},             //dec 3
{1000,760},             //boot 4
{1000,840},             //shut down 5
{560},                  //home 6
{1033,0,1033},          //edit 7
{450,670},              //Button 8
{310,0,310,0,310},      //cycle 9
{80,0,80},              //fail 10
{840,770,0,840,1000},   //start 11
{445,660,890}           //finished 12
,{660,890}              //Connected 13
,{445,660},             //Disconnected 14
{890}                   //Coms 15
}; 

unsigned long soundMillis[sound_IDs][sound_partitions]={
{180}                      //scroll up 0      
,{180}                      //scroll down 1
,{120,80}                   //inc 2
,{120,80}                   //dec 3
,{300,200}                  //boot 4
,{200,300}                  //shut down 5
,{160}                      //home 6
,{150,150,150}              //edit 7
,{200,50}                   //button 8
,{200,200,200,200,200}      //cycle 9
,{200,200,350}              //fail 10
,{200,100,300,100,200}      //start 11
,{500,400,300}              //finished 12
,{500,400,300}              //Connceted 13
,{500,400,300}              //Disconnected 14
,{200}                      //Coms 15
};                
                                  
byte sound_set_width[sound_IDs] = {1,1,2,2,2,2,1,3,2,5,3,5,3,2,2,1};
byte sound_priority[sound_IDs]; //later to be implemented, which sound cancels which :)
bool isSounding;
unsigned long sound_partition_reference;
byte sound_partition_index; //3 stages of sound or beyond, each stage acts like an index for the array of type of sounds, reset this index when overriding a sound
byte sound_ID;
bool willPlay=1;
bool audioConflict=1;

#define mute 0
#define alarms 1
#define minimal 2
#define all 3
byte audio_mode = 3;

void play_audio(byte sound_to_play)
{
    sound_partition_index=0;
    isSounding=1;
    sound_ID=sound_to_play;
    sound_partition_reference=millis();

    switch (audio_mode)
    {
    case mute:
    willPlay=0;
    break;

    case alarms:
    sound_ID >10? willPlay=1:willPlay=0; 
    break;

    case minimal:
    sound_ID>6?willPlay=1:willPlay=0;
    if(sound_ID!=audio_button_push){audioConflict=0;} //we giving the priority to these sounds except button push
    break;
    
    default:
    willPlay=1;
    break;
    }

    if(willPlay){sound(sound_sets[sound_ID][sound_partition_index]);}
}

void performAudioFeedback()
{
    if(isSounding && willPlay)
    {
             //this is quite unoptimized as it'll keep filling the PWM buffer with this freq each loop cycle but eh, can optimize later when this becomes a problem
            if(millis()-sound_partition_reference>=soundMillis[sound_ID][sound_partition_index])
            {
                sound_partition_index++;
                sound(sound_sets[sound_ID][sound_partition_index]);
                sound_partition_reference=millis();
            }
            
            if(sound_partition_index>sound_set_width[sound_ID]){isSounding=0;}
    }else
    {
        sound(0);
        audioConflict=1;
    }
}








//Concerning IO:              =========Currently, the system is mainly 



//Concerning Coms:
uint8_t coms_status; //if I'd ever need to do something with a coms Flag
uint8_t precoms_status;
#define cStatus_offline 0
#define cStatus_disconnected 1
#define cStatus_connected 2
#define cStatus_transmission 3
#define cStatus_Reception 4
#define cStatus_Error 5
String msgBuffer;
int msgBufferIndex;
int entryIndex;

void socketEvent(uint8_t num,WStype_t type,uint8_t *payload,size_t length) //note: payload pointer appears to be of 8 bits, meaning this may have constrains if not used properly. just a thought of mine :)
{
        switch(type) //do stuff based on the socket events, that the library somewhat handles for us already
        {
            case WStype_DISCONNECTED:
            play_audio(audio_disconnected);
            coms_status=cStatus_disconnected; 
            precoms_status = coms_status;
            break;

            case WStype_CONNECTED:
            play_audio(audio_connected);
            coms_status=cStatus_connected;
            precoms_status = coms_status;
            // Serial.println(webSocket.remoteIP(num).toString());
            break;

            case WStype_ERROR:
            play_audio(audio_fail);
            coms_status = cStatus_Error;
            break;

            case WStype_TEXT:
            play_audio(audio_webCMD); //for debug purposes, since we won't be sending text I guess.
            coms_status=cStatus_Reception;
            msgBuffer = String((char *)payload);

            Serial.println(msgBuffer);

            tft.println(msgBuffer);

            if(msgBuffer.length()>=2)
            {
                if(msgBuffer.substring(0,2).equals("ss"))
                {
                    tft.fillScreen(ST77XX_BLUE);
                }
            }
        }
}


//

void setup() {

    Serial.begin(115200);


    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(ssid,password,2,0,4);

    pinMode(indicator,OUTPUT);
    digitalWrite(indicator,1);

    blink(3);

    webSocket.begin(); //initiate the websocket only when there's connectivity, save resources otherwise.
    webSocket.onEvent(socketEvent);

    tft.init(240, 240, SPI_MODE2);
    tft.setRotation(2);
    tft.fillScreen(ST77XX_BLACK);
    tft.print("All booten'n good");
}

void loop() {
    webSocket.loop();
    performBlink();
}

