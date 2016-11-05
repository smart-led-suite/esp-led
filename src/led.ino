extern "C" {
  #include "user_interface.h"
}
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define NUMBER_LEDS 5

// necessary preparations for udp
WiFiUDP Udp;
unsigned int localUdpPort = 4210;
char message[255];
const int UDP_PORT = 915;

// preparations for timer
os_timer_t myTimer;

// TODO put these into a struct or object
volatile int pins[] = {16,14,12,13,15};
volatile int target[NUMBER_LEDS];
volatile int current[NUMBER_LEDS];
volatile int step[NUMBER_LEDS];
volatile int time_per_step[NUMBER_LEDS];
volatile int time_since_last[NUMBER_LEDS];

#define WIFISSID "gibtnix_optout"
#define PASSWORD "n_Pow?sjTn,Zq8."

char ssid[] = WIFISSID;
char pass[] = PASSWORD;


// This function is called every millisecond to
// fade the LEDs
void timerCallback(void *pArg)
{
  // go through all the LEDs
  for(int i = 0; i < NUMBER_LEDS; i++)
  {
    // skip the whole function if the
    // time since the last fade step is smaller
    // than defined by time_per_step (this is to)
    // allow fade times of more than 1024ms)
    time_since_last[i]++;
    if(time_since_last[i] >= time_per_step[i])
    {
      // possibility 1: fading down
      if(current[i] > target[i])
      {
        current[i] -= step[i];
        // correct if the step was larger than the
        // difference between current and target
        if(current[i] < target[i])
        {
          current[i] = target[i];
        }
      } else if (current[i] < target[i]) // possibility 2: fading up
      {
        current[i] += step[i];
        if(current[i] > target[i])
        {
          current[i] = target[i];
        }
      }
      // apply the changes to the pin
      analogWrite(pins[i], current[i]);
      // reset the time of last fade step
      time_since_last[i] = 0;
    }
  }
} // End of timerCallback


// include the function to setup the interrupt
#include "interrupt_init.cc"


void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  analogWriteFreq(10000);

  // begin wifi connection
  WiFi.begin(ssid, pass);
  Serial.print("Starting connection");
  while(WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  // print IP address
  Serial.println("Got DHCP IP:");
  Serial.println(WiFi.localIP());

  // open udp
  Udp.begin(UDP_PORT);

  for(int i = 0; i < NUMBER_LEDS; i++)
  {
    pinMode(pins[i], OUTPUT);
    current[i] = 0;
    target[i] = 1023;
    step[i] = 1;
    time_per_step[i] = 5;
    time_since_last[i] = 0;
  }



  user_init();
}


void loop()
{
  //timerCallback(NULL);

  // UDP handling
  int packet_size = Udp.parsePacket();
  if(packet_size) // if the packet's size is not zero
  {
    int len = Udp.read(message, 255);
    if (len > 0) // check again if there is any content
    {  // this part is taken from the tutorial
      message[len] = 0;
    }
    Serial.print("len: ");
    Serial.print(len);
    Serial.print(" ");
    Serial.println(message);
    int val = atoi(message);
    // take the packet apart by the delimiter ":"
    // the first call of strtok has to be with the target string
    target[0] = atoi(strtok(message, ":"));
    Serial.print(target[0]);
    // the following calls with NULL pointer to string
    for(int i = 1; i < NUMBER_LEDS; i++)
    {
      const char* pointer = strtok(NULL, ":");
      if(pointer != NULL)
      {
        target[i] = atoi(pointer);
        Serial.print(".");
        Serial.print(target[i]);

      }
    }
    Serial.println();
  }

  // write to registers
  /*for(int i = 0; i < NUMBER_LEDS; i++)
  {

    analogWrite(pins[i], current[i]);
  }*/

  //yield();  // or delay(0);
  delay(100);



}
