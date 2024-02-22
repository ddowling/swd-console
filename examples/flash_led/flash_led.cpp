#include <Arduino.h>
#include "SWDStream.h"

SWDStream logger;

uint32_t counter;

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    counter = 0;
}

void loop()
{
    digitalWrite(LED_BUILTIN, counter % 2);

    logger.print("Counter=");
    logger.print(counter);
    logger.println();
     
    counter++;

    int c = logger.read();
    if (c >= 0)
    {
        logger.print("Received key ");
        logger.println(c);
    }
    
    delay(100);
}


    
