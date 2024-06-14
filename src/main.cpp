#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ESP8266TimerInterrupt.h> //some amazing man gave us this on github to compensate for the terribly designed timers on esp8266 . . . why is this unit this expensive comparing to esp32s price anyways >:( 
#include <SPI.h>
#include <Adafruit_GFX.h>
// #include <Adafruit_ST7789.h>
#include <TFT_eSPI.h>


//Note: For the analog pins since we have only one analog interface, we're gonna use an analog multiplexer I guess. . . let's hope it doesn't interfere with the US readings . . .

//Dev Prameter:
#define VerboseON 1
#define debug(x) Serial.println(x); 

//Defs
#define SCR_WD   240
#define SCR_HT   240
#define TFT_DC    D1     // TFT DC  pin is connected to NodeMCU pin D1 (GPIO5)
#define TFT_RST   D6     // TFT RST pin is connected to NodeMCU pin D2 (GPIO4)
#define TFT_CS    -1     // TFT CS  pin is not connected to anything. . .

//concerning coms:
IPAddress local_IP(4,4,4,100);
IPAddress gateway(4,4,4,100);
IPAddress subnet(255,255,255,0);
const char* ssid     = "Levler@1";
const char* password = "ASCE-123#";

//Concerning Objects
// Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS,TFT_DC,TFT_RST);

WebSocketsServer webSocket = WebSocketsServer(80);

TFT_eSPI tft = TFT_eSPI();

TFT_eSprite heat_status = TFT_eSprite(&tft);
TFT_eSprite battery_status = TFT_eSprite(&tft);
TFT_eSprite lock_status = TFT_eSprite(&tft);
TFT_eSprite gauge = TFT_eSprite(&tft); 


//Concerning Pins:
#define indicator LED_BUILTIN
#define LBtn D3
#define RBtn 3
#define OKBtn D2
#define Reset_Hold_Pin 1
#define Buzzer D8
#define Bat A0

//Concerning Audio  
//Audio Core
int Audio_Freq;

//#define sound(freq) (Audio_Freq=freq)

#define sound(freq) tone(Buzzer, freq)

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
{930,836,924,1200},             //boot 4
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
,{300,200,100,300}                  //boot 4
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
        //sound(0);
        noTone(Buzzer);
        audioConflict=1;
    }
}

unsigned int Audio_Millis;
bool buzzerState;
void performTone()
{
    if(Audio_Freq==0){
        digitalWrite(Buzzer,0);
    }else{
    if(millis() - Audio_Millis >= 1/(Audio_Freq*2))
    {
        buzzerState= !buzzerState;
        digitalWrite(Buzzer,buzzerState);
    }
    }
}

//Concerning Feedback
double Temp; //Throw an altered readings alert when temp is too low
double EchoTime[10]; //the system shall yield 10 samples to be averaged and filtered.
double Humidity;//Dunno how the humidity may be interesting in such applications but hey, it comes with the DHT anyways
bool hatch;//It'll be reported eitherway but it may not always be used for hatch purposes ig...

//Concerning Display Menu and Actions:
byte sensor_action_mode; //Real-Time, Frequent reporting, Long duration
byte alarm_dur; //figure out what else later on

int page_index;
int element_index; //Must be less CPU intensive than earlier... 
bool io_mode=1;
#define editing_mode 1
#define scrolling_mode 0

//Redoing a less cpu intensive IO system:
// byte ok_count;
// byte right_count;
// byte left_count;

unsigned long Debounce_left;
unsigned long Debounce_right;
unsigned long Debounce_ok; 

bool confirmation = 0;

IRAM_ATTR void OKISR()
{
    if(millis()-Debounce_ok>=120)
    {
        if(editing_mode)
        {
            play_audio(audio_button_push);
        }
        Debounce_ok=millis();
    }
} //we could reset the fetch timer here for convenience but like.  .  . cpu is weak man...

IRAM_ATTR void LISR()
{
    if(millis()-Debounce_left>=120)
    {
        if(editing_mode)
        {
            play_audio(audio_scroll_down);
            if(confirmation){ESP.deepSleep(0);}
            else{confirmation=1;}
        }
        Debounce_left=millis();
    }
}

IRAM_ATTR void RISR()
{
    if(millis()-Debounce_right>=120)
    {
        if(io_mode)
        {
            play_audio(audio_scroll_up);
        }
        Debounce_right=millis();
    }
}


//Concerning Coms:
uint8_t coms_status; //if I'd ever need to do something with a coms Flag
#define cStatus_offline 0
#define cStatus_disconnected 1
#define cStatus_connected 2
#define cStatus_transmission 3
#define cStatus_Reception 4
#define cStatus_Error 5
String msgBuffer;
int msgBufferIndex;
int entryIndex;

//Check if connected or not, if not, let the message to be sent on standby until connection is established again, if the sensor echoes back, move on.

void socketEvent(uint8_t num,WStype_t type,uint8_t *payload,size_t length) //note: payload pointer appears to be of 8 bits, meaning this may have constrains if not used properly. just a thought of mine :)
{
        switch(type) //do stuff based on the socket events, that the library somewhat handles for us already
        {
            case WStype_DISCONNECTED:
            play_audio(audio_disconnected);
            coms_status=cStatus_disconnected; 
            break;

            case WStype_CONNECTED:
            play_audio(audio_connected);
            coms_status=cStatus_connected;
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
            webSocket.sendTXT(num, "200.00");
            tft.println(msgBuffer);

            if(msgBuffer.length()>=2)
            {
                if(msgBuffer.substring(0,2).equals("ss"))
                {
                    tft.fillScreen(TFT_WHITE);
                    tft.fillRect(100,200,60,-msgBuffer.substring(2,4).toInt(),TFT_BLUE);
                    tft.setCursor(20,20);
                    tft.println("Temp:"+msgBuffer.substring(4,6));
                    tft.println("Hum:"+msgBuffer.substring(6,8));
                }
            }
        }
}

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
    {200,800,2000,4000,500,0},//connected
    {20,0,100,0},//transmission
    {0},//reception
    {0}//error
}; //currently, we're not concerned with stuff other than connection status so. . . audio will do the rest.

int blinkIndex;
byte blinkMode;
unsigned long blinkInstance;

void blink(byte blinkStyle, byte blinkEase)
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
            blinkIndex>=indication_sets_width?blinkIndex=0:blinkIndex++;
            blinkInstance=millis();
        }
}

//concerning data retrieval 
int battery_level;
int last_battery_level;
bool is_charging;
unsigned long battery_monitoring_interval;

void readBattery()
{
    if(millis()-battery_monitoring_interval>=500){
    battery_level =  map(analogRead(Bat),0,650,0,100);
    if(battery_level>100||last_battery_level-battery_level<-4)
    {
        if(battery_level>100){battery_level=100;}
        is_charging = 1;
        //play_audio(audio_cycle);
    }else
    {
        is_charging=0;
    }
        last_battery_level = battery_level;
        battery_monitoring_interval = millis();
    }

}


//Concerning Video
//Video core:
#define vertical_divisions 4
#define horizontal_divisions 5
#define white TFT_WHITE
#define white2 0xe7fc
#define lime 0x07e0
#define green 0x1346
#define cyan TFT_CYAN
#define blue 0x0c10
#define lime2 0xd7f0
#define black 0x0

void divideMenu()
{
    tft.fillScreen(lime);
    tft.fillRect(0,0,20,20,lime);
    tft.fillRect(20,0,20,20,TFT_GREEN);
    tft.fillRoundRect(0,60,180,180,4,white);
}

void drawGauge()
{   
}

void drawButtons()
{
    if(page_index==0){
    tft.fillRect(0,0,240,45,lime);
    switch (element_index)
    {
    case 0:
        tft.fillRoundRect(4,5,75,42,4,lime2);
        tft.setCursor(7,8); tft.print("Settings");
        tft.fillRoundRect(85,4,70,38,3,white);
        tft.setCursor(92,8); tft.print("Overview");
        tft.fillRoundRect(160,4,70,38,3,white);
        tft.setCursor(167,8); tft.print("Control");
    break;
    case 1:
        tft.fillRoundRect(4,4,70,38,6,white);
        tft.setCursor(6,8); tft.print("Settings");
        tft.fillRoundRect(80,5,75,42,4,lime2);
        tft.setCursor(87,8); tft.print("Overview");
        tft.fillRoundRect(160,4,70,38,6,white);
        tft.setCursor(167,8); tft.print("Control");
    break;
    case 2:
        tft.fillRoundRect(4,4,70,38,6,white);
        tft.setCursor(6,8); tft.print("Settings");
        tft.fillRoundRect(80,4,70,38,6,white);
        tft.setCursor(87,8); tft.print("Overview");
        tft.fillRoundRect(155,5,75,42,4,lime2);
        tft.setCursor(162,8); tft.print("Control");
    break;
    }
    
    }
}

void drawIcons()
{

}


void renderGraphics()
{

}

void Hibernate() //save all the important states before hibernating//
{
    ESP.deepSleep(5e6);
    yield();
}

void Wake()
{

}


void setup() {

    //Wifi Init: 
    WiFi.setOutputPower(2.5);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(ssid,password,2,0,4);

    pinMode(indicator,OUTPUT);
    pinMode(OKBtn,INPUT_PULLUP);
    pinMode(RBtn, FUNCTION_3);
    pinMode(RBtn,INPUT_PULLUP);
    pinMode(LBtn,INPUT_PULLUP);
    pinMode(Buzzer,OUTPUT);

    //Hold the sleep.
    pinMode(Reset_Hold_Pin, FUNCTION_3);
    pinMode(Reset_Hold_Pin, OUTPUT);
    digitalWrite(Reset_Hold_Pin,0);
    digitalWrite(indicator, 0);
    
    //Interrupts config:
    attachInterrupt(digitalPinToInterrupt(LBtn), LISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(RBtn), RISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(OKBtn), OKISR, FALLING);

    //Check for System state:


    //Websocket Init:
    webSocket.begin(); //initiate the websocket only when there's connectivity, save resources otherwise.
    webSocket.onEvent(socketEvent);

    //Display init:
    tft.init();
    tft.fillScreen(black);
    tft.setTextSize(3);
    tft.setRotation(0);
        //tft.initDMA();
        // tft.init(240, 240, SPI_MODE2); this works with the adafruit library, gives good speed, however, I am using the tft_espi library instead to gain time with the UX design
        // tft.setSPISpeed(79999900);

        // tft.setTextWrap(1);

    

    //Greet
    tft.print("All booten'n good @");
    play_audio(audio_boot);
    tft.print(ESP.getCpuFreqMHz());
    while(isSounding)
    {
    performAudioFeedback();
    }
    tft.fillScreen(black);
    
    //Menu Init:
    divideMenu();
    drawButtons();
    
}

void loop() 
{
    webSocket.loop();
    readBattery();
    
    // tft.setTextColor(TFT_RED,black);
    // tft.setTextSize(8);
    // tft.print(battery_level);
    // tft.setCursor(20,20);
    // tft.drawWideLine();


    //fetchIO();
    performAudioFeedback();

    //performTone(); I programmed this because the tone function wasn't working, only to realize that that issue was the power supply for the buzzer. . .
    //fetchInputs();
}











// Discontinued code

//Concerning IO:              =========> This is a polling solution, away from interrupts as I Wanna reserve them for wifi stuff (low frequency and limited battery give us some desperate times :))
// volatile bool OKBtn_s;
// volatile bool LBtn_s;
// volatile bool RBtn_s; //I don't wanna use enums to keep things simple here.  . .

// #define pressed 1
// #define held_down 2
// #define released 3

// volatile byte LB_State;
// volatile byte RB_State;
// volatile byte OK_State;

// unsigned int ButtonClock;
// byte button_count;

// #define okB digitalRead(OKBtn)
// #define leftB digitalRead(LBtn)
// #define rigthB digitalRead(RBtn)

//volatile bool actFlag=0; //The fetch function can be broken into 3 parts for each pin and turned into an ISR to save power, however, this may introduce unpredictable wifi behaviour which may in turn toss energy around, this is just a theory though so, determining this later on if I liberate enough time for this project


//No longer used (Polling mode)
// void fetchInputs() //I am not gonna focus on optimizing this code so much because time is running out for the rest of this project, COMS and SENSING
// {
    
//     if(millis()-ButtonClock>=80){

//     if(digitalRead(OKBtn)!=OKBtn_s)
//     {
//         if(okB)
//         {
//         OK_State=released;
//         actFlag = 0;
//         }else
//         {
//         OK_State=pressed;
//         actFlag = 1;
//         }

//         OKBtn_s=okB;
  
//     }else
//     {
//         if(actFlag)
//         {
//             if(button_count>=7){
//             OK_State=held_down;}
//         }else
//         {
//             OK_State=0;
//         }
//     }

//     if(digitalRead(LBtn)!=RBtn_s)
//     {
//         if(rigthB)
//         {
//         RB_State=released;
//         actFlag = 0;
//         }else
//         {
//         RB_State=pressed;    
//         actFlag = 1;
//         }

//         RBtn_s=rigthB;
//     }else
//     {
//         if(actFlag)
//         {   
//             if(button_count>=7){
//             RB_State=held_down;}
//         }else
//         {
//             RB_State=0;
//         }
//     }

//     if(digitalRead(RBtn)!=LBtn_s)
//     {
//         if(leftB)
//         {
//         LB_State=released;
//         actFlag = 0;
//         button_count=0;
//         }else
//         {
//         LB_State=pressed;
//         actFlag = 1;
//         }
//         LBtn_s=leftB;
//     }else
//     {
        
//         if(actFlag)
//         {
//             if(button_count>=7){
//             LB_State=held_down;}
//         }else
//         {
//             LB_State=0;
//         }
//  }

//     button_count>7?button_count=0:button_count++;
//     ButtonClock = millis();
//     }
// }


// unsigned long fetchMillis;
// unsigned long fetchInterval = 500;

// // int okBtnCNT;
// // int leftBtnCNT;
// // int rightBtnCNT;

// volatile unsigned long lMillis;
// volatile unsigned long rMillis;
// volatile unsigned long okMillis;

// ICACHE_RAM_ATTR void OKISR()
// {   
//     if(millis()-okMillis>=20){
//     actFlag=1;
//     if(okB){OK_State=released;}else{ OK_State=pressed;OKBtn_s=1;}

//     okMillis=millis();
//     }
// }

// ICACHE_RAM_ATTR void LISR()
// {
//     if(millis()-lMillis>=20){
//     actFlag=1;
//     if(leftB){LB_State=released;} else{LB_State=pressed;LBtn_s=1;}
//     lMillis=millis();
//     }
// }

// ICACHE_RAM_ATTR void RISR()
// {
//     if(millis()-rMillis>=20){
//     actFlag=1;
//     if(rigthB){RB_State=released;}else{ RB_State=pressed;RBtn_s=1;}
//     rMillis=millis();
//     }
// }


// void fetchPresses()
// {

//     if(actFlag)
//     {
//         actFlag=0;
//         fetchMillis=millis();
//         fetchInterval=1000;
//     }

//     if(millis()-fetchMillis>=fetchInterval)
//     {
//     fetchMillis=millis();
//     fetchInterval=100;

//     if(OKBtn_s && !okB)
//     {
//         OK_State=held_down;
//     }

//     if(RBtn_s && !rigthB)
//     {
//         RB_State=held_down;
//     }

//     if(LBtn_s&& !leftB)
//     {
//         LB_State=held_down;
//     }
//     }

// }

// void fetchIO() //you have 350ms to repress the button ;)
// {

//         //Left Button Status
//         switch(io_mode){
//             case editing_mode:
//             if(left_count>1)
//             {
//                 left_count=0;
//                 play_audio(audio_scroll_up);
//             }

//             //Right Button Status
//             if(right_count>1)
//             {
//                 right_count=0;
//                 play_audio(audio_scroll_down);
//             }break;
            
//             //Ok Button Status
//             if(ok_count>1)
//             {
//                 ok_count=0;
//                 play_audio(audio_button_push);
//             }
//             break;

//             default:
//             break;
//         }
// }