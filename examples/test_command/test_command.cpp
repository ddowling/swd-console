#include <Arduino.h>
#include "SWDStream.h"
#include "CommandParser.h"

#define VERSION "1.0"

SWDStream logger;

bool help_cmd(CommandParser *c)
{
    c->showHelp();
    return true;
}

bool version_cmd(CommandParser *c)
{
    c->print("test_command\n");
    c->printVar("version", VERSION);
    c->printVar("built",  __DATE__ " " __TIME__);

    return true;
}

const Command commands[] =
{
    { "help", 0, help_cmd, "Show help on all commands" },
    { "version", 0, version_cmd, "Show the firmware version" },
};

CommandParser parser(logger, commands, 0, true);

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    parser.print("Booted\n");
    parser.releaseOutput();
}

void loop()
{
    if (parser.inputAvailable())
    {
        digitalWrite(LED_BUILTIN, LOW);
        parser.poll();
        digitalWrite(LED_BUILTIN, HIGH);
    }
}


    
