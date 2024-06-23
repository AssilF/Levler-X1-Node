#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
//some amazing man gave us this on github to compensate for the terribly designed timers on esp8266 . . . why is this unit this expensive comparing to esp32s price anyways >:( 
#include <SPI.h>
#include <Adafruit_GFX.h>
// #include <Adafruit_ST7789.h>
#include <TFT_eSPI.h>
#include <sprites.h>


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
;IPAddress local_IP(4,4,4,100);
IPAddress gateway(4,4,4,100);
IPAddress subnet(255,255,255,0);
const char* ssid     = "Levler@1";
const char* password = "ASCE-123#";

//Concerning Objects
// Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS,TFT_DC,TFT_RST);

WebSocketsServer webSocket = WebSocketsServer(80);

TFT_eSPI tft = TFT_eSPI();

TFT_eSprite canvas = TFT_eSprite(&tft);
TFT_eSprite screen = TFT_eSprite(&tft);



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
{444,652,866,1112},      //boot 4
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
,{150,100,50,400}                  //boot 4
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
                    
byte sound_set_width[sound_IDs] = {1,1,2,2,4,2,1,3,2,5,3,5,3,2,2,1};
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
//double Temp; //Throw an altered readings alert when temp is too low
//double EchoTime[10]; //the system shall yield 10 samples to be averaged and filtered.
//double Humidity;//Dunno how the humidity may be interesting in such applications but hey, it comes with the DHT anyways
//bool hatch;//It'll be reported eitherway but it may not always be used for hatch purposes ig...

//Concerning Display Menu and Actions:
//byte sensor_action_mode; //Real-Time, Frequent reporting, Long duration
//byte alarm_dur; //figure out what else later on

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

struct pack
{
float distance=0.0;
float humidity=0.0;
float temperature=0.0;
bool hatch_state=1;
int battery_level=0;
}data_in;


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
            webSocket.sendTXT(num,"Data");
            coms_status=cStatus_connected;
            // Serial.println(webSocket.remoteIP(num).toString());
            break;

            case WStype_ERROR:
            play_audio(audio_fail);
            coms_status = cStatus_Error;
            break;

            case WStype_TEXT:
            // play_audio(audio_cycle);
            // play_audio(audio_webCMD); //for debug purposes, since we won't be sending text I guess.
            // coms_status=cStatus_Reception;
            // msgBuffer = String((char *)payload);

            // Serial.println(msgBuffer);
            // webSocket.sendTXT(num, "200.00");
            // tft.println(msgBuffer);

            // if(msgBuffer.length()>=2)
            // {
            //     if(msgBuffer.substring(0,2).equals("ss"))
            //     {
            //         tft.fillScreen(TFT_WHITE);
            //         tft.fillRect(100,200,60,-msgBuffer.substring(2,4).toInt(),TFT_BLUE);
            //         tft.setCursor(20,20);
            //         tft.println("Temp:"+msgBuffer.substring(4,6));
            //         tft.println("Hum:"+msgBuffer.substring(6,8));
            //     }
            // }
            break;

            WStype_PING:
            play_audio(audio_boot);
            break;
            
            case WStype_BIN:
            // play_audio(audio_webCMD);
            memcpy(&data_in,payload,length);
            break;
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

// int blinkMillis[][indication_sets_width] 
// {
//     {0},
//     {0}, //disconnected
//     {100,100,200,200,100,100},//connected
//     {400,100,600,300}, //transmission
//     {0}, //reception
//     {0} //error
// };
// int blinkIntensities[][indication_sets_width]
// {
//     {0},//offline
//     {0},//disconnected
//     {200,800,2000,4000,500,0},//connected
//     {20,0,100,0},//transmission
//     {0},//reception
//     {0}//error
// }; //currently, we're not concerned with stuff other than connection status so. . . audio will do the rest.

// int blinkIndex;
// byte blinkMode;
// unsigned long blinkInstance;

// void blink(byte blinkStyle, byte blinkEase)
// {
//     if(blinkStyle != blinkMode)
//     {
//         blinkMode = blinkStyle;
//         blinkIndex = 0;
//     }
// }

// void performBlink()
// {
//         if(millis()-blinkInstance>= blinkMillis[blinkMode][blinkIndex])
//         {
//             wink(blinkIntensities[blinkMode][blinkIndex]);
//             blinkIndex>=indication_sets_width?blinkIndex=0:blinkIndex++;
//             blinkInstance=millis();
//         }
// }

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

#define first_quadron 0
#define second_quadron 1
#define third_quadron 2
#define fourth_quadron 3

byte current_quadron;
int relative_x;
int relative_y;

void divideMenu() //call this on page change only because apparently it doesn't affect the screen much. . .;
{
    screen.fillSprite(lime);
    screen.fillRoundRect(-relative_x,35-relative_y,205,205,4,white);

}

#define drawFilledCircle(x,y,r,c) screen.fillCircle(x-relative_x,y-relative_y,r,c)
#define drawRelativeArc(x,y,r1,r2,a1,a2,c1,c2) screen.drawArc(x-relative_x,y-relative_y,r1,r2,a1,a2,c1,c2)
#define setRelativeCursor(x,y) screen.setCursor(x-relative_x,y-relative_y)
#define calculateAngle(input) mapfloat(constrain(input,dead_zone,max_voulume),dead_zone,max_voulume,min_gauge_angle,max_gauge_angle)
#define drawRelativeString(s,x,y) screen.drawString(s,x-relative_x,y-relative_y,font)
#define gauge_radius 100
#define gauge_position_x 103
#define gauge_position_y 180
#define gauge_internal_radius 80
#define max_gauge_angle 315
#define min_gauge_angle 45 //(degrees)
#define gauge_percentage_size 4
#define gauge_volume_size 2
#define gauge_diff_size 2

//configurations:
float max_voulume=1000;
float dead_zone=200; 
float volume;

float mapfloat(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}

void drawGauge() //push this @ render state 1
{
    drawFilledCircle(gauge_position_x,gauge_position_y,gauge_radius,TFT_DARKCYAN);
    drawFilledCircle(gauge_position_x,gauge_position_y,gauge_internal_radius,TFT_GREENYELLOW);
    drawRelativeArc(gauge_position_x,gauge_position_y,gauge_radius,gauge_internal_radius,min_gauge_angle,calculateAngle(volume),TFT_GREEN,TFT_DARKCYAN);
    
    screen.setTextSize(5);
    screen.setTextDatum(4);
    screen.setTextColor(black);
    setRelativeCursor(60,151);
    screen.print(map(volume,dead_zone,max_voulume,0,100));
    screen.print("%");

    // screen.fillSprite(0);
    // screen.fillSmoothCircle(101,101,101,TFT_DARKCYAN);
    // screen.fillSmoothCircle(17+84,19+84,84,TFT_GREEN);
    // screen.setPivot(85,85);
    // canvas.fillSprite(TFT_TRANSPARENT);
    // canvas.drawWedgeLine(30,60,30,0,10,40,TFT_RED);
    // canvas.pushRotated(&screen,0,TFT_TRANSPARENT);
    // screen.drawArc(101,101,102,85,60,120,TFT_GREENYELLOW,1);
    // screen.setCursor(70,85);
    // screen.setTextColor(black,TFT_GREEN);
    // screen.setTextSize(5);
    // screen.print("20%");
    // screen.pushSprite(0,80,0);
    // yield();
}


unsigned long render_time;

void drawDebugData()
{
    if(current_quadron==first_quadron){
    screen.setTextSize(1);
    screen.setTextColor(black,white);
    screen.setTextWrap(0,0);
    screen.setCursor(10-relative_x,10-relative_y);
    screen.print("Bat:");
    screen.println(data_in.battery_level);
    screen.print("Dist:");
    screen.println(data_in.distance);
    screen.print("Hatch:");
    screen.println(data_in.hatch_state);
    screen.print("Humi:");
    screen.println(data_in.humidity);
    screen.print("Temp:");
    screen.println(data_in.temperature);
    screen.print("Free heap:");
    screen.println(system_get_free_heap_size());
    screen.print("render time:");
    screen.print(render_time);
    }
}

byte data_display_mode;
#define gauge_mode 0
#define dial_mode 1 
#define meter_mode 2
#define reservoir_mode 3 
#define seven_segment_mode 4
#define level_mode 5 
#define cubic_mode 6 //still experimenting with 3D transform and projection matrices 

void drawData()
{
    switch(data_display_mode)
    {
        case gauge_mode:
        drawGauge();
        break;

        case dial_mode:
        break;

        default:
        drawGauge();
        break;
    }
}

#define battery_icon_height 38
#define battery_icon_width 60
#define battery_icon_x 180
#define battery_icon_y 0

#define humidity_icon_height 55
#define humidity_icon_width 29
#define humidity_icon_x 87
#define humidity_icon_y 36

#define temp_icon_width 60
#define temp_icon_height 65
#define temp_icon_x 6
#define temp_icon_y 30
#define temp_text_size 3
#define temp_text_x 2
#define temp_text_y 68

#define page_text_size 3 //good idea to break down the < > arrows and put them in a box
#define page_text_x 0
#define page_text_y 1

#define remote_battery_width 44
#define remote_battery_height 28
#define remote_battery_x 158
#define remote_battery_y 49

#define host_name_size 1
#define host_name_x 158
#define host_name_y 36

#define coms_icon_width 34
#define coms_icon_height 36
#define coms_icon_x 206
#define coms_icon_y 41

#define padlock_icon_width 60
#define padlock_icon_height 60
#define padlock_icon_x 204
#define padlock_icon_y 99

#define output_icon_width 38
#define output_icon_height 50
#define output_icon_x 206
#define output_icon_y 168
#define output_condition_size 1
#define output_conidition_x 201
#define output_condition_y 223

#define bell_icon_width 60
#define bell_icon_height 55
#define bell_icon_x 113
#define bell_icon_y 36

byte icon_index=0;

void drawIcons() //a side note, find a way to only update icons whenever there's need for it, each millisecond counts.
{ //don't forget to make these draw calls called one at a time each cycle to not put too much strain on the CPU and elongate the frame render time. . .
    //Drawing Temp Status (Add a third state maybe)
    // switch (icon_index)
    // {
    // case 1:
            canvas.fillSprite(TFT_BLACK);
    canvas.pushImage(0,0,temp_icon_width,temp_icon_height,data_in.temperature>8?heat_icon:freeze_icon);
    canvas.pushToSprite(&screen,temp_icon_x-relative_x,temp_icon_y-relative_y,TFT_BLACK);
    //     break;
    
    // case 2:
        //Drawing coms status
    if(coms_status==cStatus_connected)
    {
        canvas.fillSprite(TFT_BLACK);
        canvas.pushImage(0,0,coms_icon_width,coms_icon_height,coms_icon);
        canvas.pushToSprite(&screen,coms_icon_x-relative_x,coms_icon_y-relative_y,TFT_BLACK);
    }

// break;

//     case 3:
        //Drawing battery status
    canvas.fillSprite(TFT_BLACK);
    screen.fillRect(battery_icon_x-relative_x,battery_icon_y-relative_y,battery_icon_width,battery_icon_height,TFT_DARKGREY);
    screen.fillRect(battery_icon_x+map(battery_level,0,100,battery_icon_width,0)-relative_x,battery_icon_y-relative_y,map(battery_level,0,100,0,battery_icon_width)
    ,battery_icon_height,TFT_GOLD);


    canvas.pushImage(0,0,battery_icon_width,battery_icon_height,battery_icon);
    canvas.pushToSprite(&screen,battery_icon_x-relative_x,battery_icon_y-relative_y,TFT_BLACK);
//don't forget to add percentage text

// break;

//     case 4:
        //Drawing remote battery status
    canvas.fillSprite(TFT_BLACK);
    screen.fillRect(remote_battery_x-relative_x,remote_battery_y-relative_y,remote_battery_width,remote_battery_height,TFT_DARKGREY);
    screen.fillRect(remote_battery_x+map(data_in.battery_level,0,100,remote_battery_width,0)-relative_x,remote_battery_y-relative_y,map(data_in.battery_level,0,100,0,remote_battery_width)
    ,remote_battery_height,TFT_GOLD);

    canvas.pushImage(0,0,remote_battery_width,remote_battery_height,remote_battery_icon);
    canvas.pushToSprite(&screen,remote_battery_x-relative_x,remote_battery_y-relative_y,TFT_BLACK);
//don't forget to add percentage text

// break;

//     case 5:
        //drawing padlock:
    canvas.fillSprite(TFT_BLACK);
    canvas.pushImage(0,0,padlock_icon_width,padlock_icon_height,data_in.hatch_state?lock_open_icon:lock_icon);
    canvas.pushToSprite(&screen,padlock_icon_x-relative_x,padlock_icon_y-relative_y,TFT_BLACK);

// break;

//     case 6:
    
    //drawing humidity status:
    canvas.fillSprite(TFT_BLACK);
    canvas.pushImage(0,0,humidity_icon_width,humidity_icon_height,humidity_icon);
    canvas.pushToSprite(&screen,humidity_icon_x-relative_x,humidity_icon_y-relative_y,TFT_BLACK);

// break;

//     case 7:
    
    //drawing bell status: (this is reserved for when we build the alert engine)
    canvas.fillSprite(TFT_BLACK);
    canvas.pushImage(0,0,bell_icon_width,bell_icon_height,alert_icon);
    canvas.pushToSprite(&screen,bell_icon_x-relative_x,bell_icon_y-relative_y,TFT_BLACK);

// break;

//     case 8:
        //drawing button: (reserved for output)
    screen.pushImage(output_icon_x-relative_x,output_icon_y-relative_y,output_icon_width,output_icon_height,output_on_icon);

// break;

//     case 9:

//     break;
//     }
//     icon_index++;
}

unsigned long render_time_reference;
byte render_index;
void renderGraphics()
{
    switch(current_quadron) //offset everything on the screen;
    {
        case first_quadron:
            relative_x = 0;
            relative_y = 0;
        break;

        case second_quadron:
            relative_x = 120;
            relative_y = 0;
        break;

        case third_quadron:
            relative_x = 0;
            relative_y = 120;
        break;

        case fourth_quadron:
            relative_x = 120;
            relative_y = 120;
        break;
    }
    //Put what to be rendered here
    if(current_quadron==first_quadron){render_time_reference = millis();}
    screen.fillSprite(TFT_TRANSPARENT);
    divideMenu();
    drawIcons();
    webSocket.loop(); //to avoid freezing that stuff
    drawDebugData();
    drawData();
    webSocket.loop();
    //end of what to be rendered
    current_quadron>=fourth_quadron?current_quadron=first_quadron:current_quadron++; //increment the index of the quadron for next render iteration.
    screen.pushSprite(relative_x,relative_y,TFT_TRANSPARENT);
    if(current_quadron==first_quadron){render_time= millis()-render_time_reference;}
}

void render_greet()
{
    for(int i=0;i<4;i++){
    screen.fillSprite(TFT_BLACK);
    switch(current_quadron) //offset everything on the screen;
    {
        case first_quadron:
            relative_x = 0;
            relative_y = 0;
        screen.setTextColor(TFT_GREEN,TFT_BLACK);
        screen.print("All booten'n good @");
        screen.print(ESP.getCpuFreqMHz());
        break;

        case second_quadron:
            relative_x = 120;
            relative_y = 0;
        break;

        case third_quadron:
            relative_x = 0;
            relative_y = 120;
        break;

        case fourth_quadron:
            relative_x = 120;
            relative_y = 120;
        break;
    }
    screen.pushImage(-relative_x,60-relative_y,240,120,ufas1_logo);
    screen.pushSprite(relative_x,relative_y,TFT_BLACK);
    current_quadron++;
    }
}

void Hibernate() //save all the important states before hibernating//
{
    ESP.deepSleep(5e6);
    yield();
}


void setup() {

    //Wifi Init: 
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
    tft.fillScreen(white);
    tft.setTextSize(1);
    tft.setRotation(0);
    screen.setColorDepth(8);//Quadrons mode to free some heap ig . . . 
    screen.createSprite(120,120);
    canvas.setColorDepth(16); //maybe come up with better names for screen and canvas next time . . . :)
    canvas.createSprite(60,65); //this should cover most sprites we're gonna be adding onto the screen.
    canvas.setSwapBytes(1);

    //Greet
    render_greet();
    play_audio(audio_boot);
    while(isSounding){performAudioFeedback();}
    tft.fillScreen(black);
    
    //Finalizing
}

unsigned long unfreezing_reference;
bool test_bool;

void loop() 
{
    webSocket.loop();
    volume=data_in.distance;
    renderGraphics();
    yield();
    // if(millis()-unfreezing_reference>=30)
    // {
    //     // if(test_bool)
    //     // {
    //     //     volume+=20;
    //     // }else{volume-=45;}
    //     // if(volume>=max_voulume)
    //     // {
    //     //     test_bool = 0;
    //     // }
    //     // if(volume<=dead_zone)
    //     // {
    //     //     test_bool=1;
    //     // }
    //     // unfreezing_reference=millis();
    // }

    performAudioFeedback();
}