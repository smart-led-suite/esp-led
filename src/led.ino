#include <Arduino.h>

extern "C" {
  #include "user_interface.h" // needed for timer access
}
#include <ESP8266WiFi.h>

#define NUMBER_LEDS 3
#define TCP_PORT 915

// necessary preparations for tcp
WiFiServer server(TCP_PORT);

// preparations for timer
os_timer_t myTimer;

volatile int pins[] = {16, 5, 4};
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

    Serial.printf("Connecting to %s ", ssid);
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    Serial.println(" connected");

    server.begin();
    Serial.printf("Web server started, open %s in a web browser\n", WiFi.localIP().toString().c_str());


    analogWriteFreq(10000);


  for(int i = 0; i < NUMBER_LEDS; i++)
  {
    pinMode(pins[i], OUTPUT);
    leds[i].pin = pins[i];
    leds[i].target = 1023;
  }


  // the complete initalization of the interrupts
  // is outsourced to user_init(), even in a separate file
  user_init();
}


void loop()
{
  //timerCallback(NULL);
  // TCP handling
  WiFiClient client = server.available();
  // wait for a client (web browser) to connect
  if (client)
  {
    Serial.println("\n[Client connected]");
    while (client.connected())
    {
      // read line by line what the client (web browser) is requesting
      if (client.available())
      {
        String line = client.readStringUntil('\r');
        Serial.print("message received: ");
        Serial.print(line);

        // evaluate the client's request
        // setting fade time - e.g. "t2000"
        // check if first character is 't'
        if(line.charAt(0) == 't')
        {
          Serial.println("Time-set message received");
          fadetime = line.substring(1).toInt(); // substring to cut off the 't' at the beginning
          if(fadetime == 0)
          {
            fadetime = 1; // we can't divide by zero!
          }
          Serial.print("Fadetime set: ");
          Serial.println(fadetime);
        } else if(line.charAt(0) == 'f') // check if first char is 'f' for fade
        {
          Serial.println("fade message received");
          LED* led;
          // take the packet apart by the delimiter ":"
          int currentColon = 0;
          int lastColon;
          int index = 0; // needed to address the correct struct from leds[]
          String substring;
          do {
            led = &leds[index];

            // incr index to use the next led the next time the loop runs
            index++;

            lastColon = currentColon;
            currentColon = line.indexOf(':', lastColon + 1);
            Serial.print(currentColon);
            if(currentColon != -1) // if we haven't yet reached the last part of the line
            {
              substring = line.substring(lastColon + 1, currentColon); // + 1 to cut off the colon itself (or the 'f' in the first time the loop is run)
            } else
            {
              substring = line.substring(lastColon + 1); // substring goes to the end of the line
            }
            Serial.print(": ");
            Serial.println(substring);
            // apply and calculate values
            int target = substring.toInt();
            if(target == -1) // -1 means no change to the led's brightness
              continue;
            led->target = target;
            Serial.print(".");
            Serial.println(led->target);
            // calculate times needed
            int diff = led->target - led->current;
            if(diff == 0) // prevent division by zero
              continue;
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
            Serial.println(led->time_per_step);


          } while(currentColon != -1 && index < NUMBER_LEDS); // currentColon == -1 means we have reached the end of the string
        } else if(line.charAt(0) == 's') // 's' for direct brightness setting without fading
        {
          Serial.println("Direct-set message received");
          LED* led;
          // take the packet apart by the delimiter ":"
          int currentColon = 0;
          int lastColon;
          int index = 0; // needed to address the correct struct from leds[]
          String substring;
          do
          {
            led = &leds[index];

            // incr index to use the next led the next time
            index++;

            lastColon = currentColon;
            currentColon = line.indexOf(':', lastColon + 1);
            Serial.print(currentColon);
            if(currentColon != -1) // if we haven't yet reached the last part of the line
            {
              substring = line.substring(lastColon + 1, currentColon); // + 1 to cut off the colon itself (or the 'f' in the first time the loop is run)
            } else
            {
              substring = line.substring(lastColon + 1); // substring goes to the end of the line
            }
            Serial.print(": ");
            Serial.println(substring);
            // apply and calculate values
            int target = substring.toInt();
            if(target == -1) // -1 means no change to the led's brightness
              continue;

            led->target = target;
            // for direct fading, target and current need to be instantly the same
            led->current = led->target;
            led->time_per_step = 1; // make sure it is applied with the next interrupt
          } while(currentColon != -1 && index < NUMBER_LEDS); // currentColon == -1 means we have reached the end of the string
          }
        }
      }

    delay(1); // give the web browser time to receive the data

    // close the connection:
    client.stop();
    Serial.println("[Client disonnected]");
  }

  //yield();  // or delay(0);
  delay(100);



}
