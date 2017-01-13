extern "C" {
  #include "user_interface.h"
}
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define NUMBER_LEDS 3

// necessary preparations for udp
WiFiUDP Udp;
unsigned int localUdpPort = 4210;
char message[255];
const int UDP_PORT = 915;

// preparations for timer
os_timer_t myTimer;

// TODO put these into a struct or object
volatile int pins[] = {5, 16, 4};
// volatile int target[NUMBER_LEDS];
// volatile int current[NUMBER_LEDS];
// volatile int step[NUMBER_LEDS];
// volatile int time_per_step[NUMBER_LEDS];
// volatile int time_since_last[NUMBER_LEDS];

typedef volatile struct {
  int pin;
  int target = 0;
  int current = 0;;
  int step = 1;;
  int time_per_step =1;
  int time_since_last = 1;
} LED;

LED leds[NUMBER_LEDS];

volatile int fadetime = 1000;

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
    LED* led = &leds[i];
    // skip the whole function if the
    // time since the last fade step is smaller
    // than defined by time_per_step (this is to)
    // allow fade times of more than 1024ms)
    led->time_since_last++;
    if(led->time_since_last >= led->time_per_step)
    {
      // possibility 1: fading down
      if(led->current > led->target)
      {
        led->current -= led->step;
        // correct if the step was larger than the
        // difference between current and target (overshooting)
        if(led->current < led->target)
        {
          led->current = led->target;
        }
      } else if (led->current < led->target) // possibility 2: fading up
      {
        led->current += led->step;
        if(led->current > led->target)
        {
          led->current = led->target;
        }
      }
      // apply the changes to the pin
      analogWrite(led->pin, led->current);
      // reset the time of last fade step
      led->time_since_last = 0;
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
    leds[i].pin = pins[i];
    leds[i].target = 1023;
    /*
    current[i] = 0;
    target[i] = 1023;
    step[i] = 1;
    time_per_step[i] = 1;
    time_since_last[i] = 0;*/
  }


  // the complete initalization of the interrupts
  // is outsourced to user_init(), even in a separate file
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
    // check the type of message
    // setting fade time - e.g. "t2000"
    // check if first character is 't'
    if(strchr(message, 't') == &message[0]) { // strchr returns the pointer to the first occurence of 't'
      Serial.println("Time-set message received");
      // TODO calculate values
      fadetime = atoi(message + 1); // +1 to cut off the first character, the "t"
      if(fadetime == 0) {
        fadetime = 1; // we can't divide by zero!
      }
      Serial.println(fadetime);
    } else if(strchr(message, 'f') == &message[0]) { // fade values begin with f
      Serial.println("fade message received");
      LED* led = &leds[0];
      // take the packet apart by the delimiter ":"
      // the first call of strtok has to be with the target string
      led->target = atoi(strtok(message, ":")+1); // +1 to cut off the b which is the first character
      // calculate times needed
      int diff = led->target - led->current;
      Serial.println(led->target);
      Serial.print("diff: ");
      Serial.println(diff);
      Serial.print("fadetime: ");
      Serial.println(fadetime);
      led->step = abs(diff / fadetime); // size of a step is the needed fadesize divided by the time
      // we don't want the step to be zero, nothing is happening then
      if(led->step == 0)
        led->step = 1; // take the smallest possible value, 1
      Serial.print("step: ");
      Serial.println(led->step);
      led->time_per_step = abs(fadetime / diff); // just the other way round
      Serial.print("time_per_step: ");
      Serial.print(led->time_per_step);
      // the following calls with NULL pointer to string
      for(int i = 1; i < NUMBER_LEDS; i++)
      {
        led = &leds[i];
        const char* pointer = strtok(NULL, ":");
        if(pointer != NULL)
        {
          led->target = atoi(pointer);
          Serial.print(".");
          Serial.println(led->target);
          // calculate times needed
          int diff = led->target - led->current;
          Serial.println(led->target);
          Serial.print("diff: ");
          Serial.println(diff);
          Serial.print("fadetime: ");
          Serial.println(fadetime);
          led->step = abs(diff / fadetime); // size of a step is the needed fadesize divided by the time
          // we don't want the step to be zero, nothing is happening then
          if(led->step == 0)
            led->step = 1; // take the smallest possible value, 1
          Serial.print("step: ");
          Serial.println(led->step);
          led->time_per_step = fadetime / diff; // just the other way round
          Serial.print("time_per_step: ");
          Serial.print(led->time_per_step);

        }
      }
    } else if(strchr(message, 'd') == &message[0]) { // d for direct setting
      Serial.println("Direct-set message received");
      LED* led = &leds[0];
      led->target = atoi(strtok(message, ":")+1); // +1 to cut off the d which is the first character
      // directly set the value without any interrupt fading
      led->current = led->target;
      // make sure the interrupt applies the new value as early as possible
      led->time_per_step = 1;
      Serial.print(led->current);

      // the following calls with NULL pointer to string
      for(int i = 1; i < NUMBER_LEDS; i++)
      {
        led = &leds[i];
        const char* pointer = strtok(NULL, ":");
        if(pointer != NULL) // if there is data
        {
          led->target = atoi(pointer);
          led->current = led->target;
          led->time_per_step = 1;
          Serial.print(";");
          Serial.print(led->current);
        }
      }
    }
    Serial.println();
  }

  // write to registers
  /*for(int i = 0; i < NUMBER_LEDS; i++)
  {

    analogWrite(led->pins, led->current);
  }*/

  //yield();  // or delay(0);
  delay(100);



}
