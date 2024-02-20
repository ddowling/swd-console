/* $Id: CommandParser.h 1133 2013-08-20 01:32:43Z dpd $
 *
 * Copyright   : (c) 2011 by Denis Dowling.  All Rights Reserved
 * Project     : RAEdyne Pty Ltd
 * File        : CommandParser
 *
 * Author      : Denis Dowling
 * Created     : 17/3/2011
 *
 * Description : Class to help parsing commands on the serial interface
 *
 * Communication Protocol
 * ASCII format for all input. Format according to the following:
 * {@addr} command {args}* {$cccc}<CR|LF>
 *
 * Where: @addr is the optional RS485 address
 *        $cccc is an optional CRC16 checksum
 *
 */
#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

// FIXME Need to work out how to set for a project
#define COMMAND_INTERACTIVE 1
#define COMMAND_ADVANCED_FORMAT 1
#define COMMAND_FLOAT 1

#include <Arduino.h>

#ifndef COMMAND_STATS
#define COMMAND_STATS 1
#endif
#ifndef COMMAND_CRC
#define COMMAND_CRC 1
#endif
#ifndef COMMAND_INTERACTIVE
#define COMMAND_INTERACTIVE 0
#endif
#ifndef COMMAND_ADVANCED_FORMAT
#define COMMAND_ADVANCED_FORMAT 0
#endif
#ifndef COMMAND_FLOAT
#define COMMAND_FLOAT
#endif

class CommandParser;

typedef bool (*CommandCallbackFunction)(CommandParser *c);

struct Command
{
    const PROGMEM char *name;
    const PROGMEM char *arguments;
    CommandCallbackFunction PROGMEM callback;
    const PROGMEM char *description;
};

class CommandParser
{
public:
    CommandParser(Stream &serial,
                  const PROGMEM Command  *commands,
                  uint8_t rs485_txen = 0
#if COMMAND_INTERACTIVE
                  , bool interactive = false
#endif
        );

    // Poll the serial port reading characters as they become available
    // When a full packet has arrive the command will be dispatched off to
    // the correct handler
    void poll();

    // Wrapper for Serial.available. This is used to be a client to
    // turn on an LED when input is available
    bool inputAvailable();

    // Parse a byte from the command buffer. Note bytes are unsigned
    bool parseByte(uint8_t *b);

    // Parse an integer from the command buffer
    bool parseInt(int *i);

    // Parse a long integer from the command buffer
    bool parseLong(long *l);

    // Parse a string
    bool parseString(char *str, uint8_t max_length);

    // Parse a hex integer from the command buffer
    bool parseHex(int *i);

    // Parse the rest of the line including any spaces
    bool parseRest(char *str, uint8_t max_length);

#if COMMAND_FLOAT
    bool parseFloat(float *f);
#endif

    // Output the help information
    void showHelp();

    // Used by commands to generate the response
    // Note: These should always be used instead of Serial.print to ensure
    // the RS485 bus is captured and to calculate the optional CRC16 checksums
    void putChar(char c);
    void print(const char * PROGMEM str);
    void putNibble(uint8_t n);
    void putByteHex(uint8_t b);
    // Put a number to output.
    // Number is unsigned.
    // Width of 0 means size automatically.
    // Positive width left justify
    // Negative width right justify
    void putNumber(unsigned long i,
                   int8_t width,
                   bool leading_zeros,
                   uint8_t base,
                   uint8_t num_factors,
                   unsigned long max_factor);
    void printHex(unsigned long var,
                  int8_t width=0,
                  bool leading_zeros=false);
    void printDecimal(long var,
                      int8_t width=0,
                      bool leading_zeros=false);
    void printBinary(unsigned long var,
                     int8_t width=0);
#if COMMAND_FLOAT
    void printFloat(float var, int8_t decimal_places = 6);
#endif

    void printVar(const char * PROGMEM str, int var);
    void printVar(const char * PROGMEM str, unsigned int var);
    void printVar(const char * PROGMEM str, long var);
    void printVar(const char * PROGMEM str, unsigned long var);
    void printVar(const char * PROGMEM str, const char *var);
    void printVarHex(const char * PROGMEM str, int var);
#if COMMAND_FLOAT
    void printVar(const char * PROGMEM str, float var);
#endif
    void printLine();

    // Used to control the RS485 direction
    // Safe to call grab output multiple times
    // The print functions will automatically call grab output and it
    // will be released when the command parser is waiting for user input
    void grabOutput();
    void releaseOutput();
    void clearBuffer();

    uint8_t getRS485Address() const;
    void setRS485Address(uint8_t addr);

#if COMMAND_STATS
    void clearStats();
    void showStats();
#endif

    Stream &getStream() { return serial; }

#if COMMAND_INTERACTIVE
    void setInteractive(bool b);
#endif

protected:
    Stream &serial;

#define MAX_PACKET 64
    const Command * PROGMEM commands;
#if COMMAND_INTERACTIVE
    bool interactive;
    bool needsPrompt;
#endif
    uint8_t rs485TXEN;
    char buffer[MAX_PACKET];
    uint8_t readPos;
    uint8_t writePos;
    bool needResponsePrefix;
    uint8_t rs485Address;

    void processPacket();
    const Command *getCommand();
    bool processCommand(const Command *cmd);
    void skipSpace();
    void doStatus(bool res);
    void sendResponsePrefix();
    int convertNimble(char c);


#if COMMAND_CRC
    uint16_t crc;
    bool addCRC;
#endif
#if COMMAND_STATS
    unsigned int statsMaxPacketLength;
    unsigned int statsIllegalCharacter;
#if COMMAND_CRC
    unsigned int statsCRCMismatch;
#endif
    unsigned int statsMissingAddress;
    unsigned int statsInvalidCommand;
    unsigned int statsCommandError;
    unsigned int statsCommandOK;
#endif
};

#endif
