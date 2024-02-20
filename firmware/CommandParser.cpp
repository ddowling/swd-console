/*
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
 */
#include "CommandParser.h"
#include <Arduino.h>

#if COMMAND_CRC
#define CRC_INITIAL 0xffff

#if defined(__AVR__)
#include <util/crc16.h>
#else
uint16_t _crc_ccitt_update(uint16_t crc, uint8_t data)
{
    data ^= crc & 0xff;
    data ^= data << 4;

    return ((((uint16_t)data << 8) | (crc>>8)) ^ (uint8_t)(data >> 4)
            ^ ((uint16_t)data << 3));
}
#endif
#endif

CommandParser::CommandParser(Stream &serial_,
                             const Command  * PROGMEM commands_,
                             uint8_t rs485_txen
#if COMMAND_INTERACTIVE
                             , bool interactive_
#endif
    )
    : serial(serial_),
      commands(commands_),
#if COMMAND_INTERACTIVE
      interactive(interactive_),
      needsPrompt(true),
#endif
      rs485TXEN(rs485_txen),
      readPos(0), writePos(0), needResponsePrefix(true),
      rs485Address(0)
{
    if (rs485TXEN > 0)
    {
	// Set the RS485 direction pin to output
	pinMode(rs485TXEN, OUTPUT);
    }

    // FIXME This is causing prompt output in the wrong place on boot?
    // Set to input
    //releaseOutput();

#if COMMAND_STATS
    clearStats();
#endif
#if COMMAND_CRC
    addCRC = false;
    crc = CRC_INITIAL;
#endif
}

void CommandParser::clearBuffer()
{
    readPos = 0;
    writePos = 0;
}

uint8_t CommandParser::getRS485Address() const
{
    return rs485Address;
}

void CommandParser::setRS485Address(uint8_t addr)
{
    rs485Address = addr;
}

void CommandParser::poll()
{
    while (serial.available())
    {
	if (writePos >= MAX_PACKET)
	{
	    writePos = 0;

#if COMMAND_STATS
            statsMaxPacketLength++;
#endif
	    return;
	}

	char c = serial.read();

	if (c == '\r' || c == '\n')
        {
#if COMMAND_INTERACTIVE
            if (interactive)
                serial.write("\r\n");
#endif
            processPacket();
        }
        else if (c == 0x03) // Ctrl-C
        {
            clearBuffer();

#if COMMAND_INTERACTIVE
            if (interactive)
                serial.write("\r\n");
            needsPrompt = true;
#endif
        }
#if COMMAND_INTERACTIVE
        else if (interactive && (c == 0x08 || c == 0x7f)) // Backspace or Delete
        {
            if (writePos > 0)
            {
                serial.write("\x08 \x08");
                writePos--;
            }
        }
#endif
        else if (c < ' ')
        {
            // Ignore some control characters as we seem to get some
            // NUL characters on the RS485 bus
#if COMMAND_STATS
            statsIllegalCharacter++;
#endif
        }
	else if (writePos == 0 || buffer[0] != '*')
	{
#if COMMAND_INTERACTIVE
            if (interactive)
                serial.write(c);
#endif

	    // Only copy characters into the command buffer if it is not
	    // a response to another command. Otherwise long responses exceed
	    // MAX_PACKET length and cause output conflicts
	    buffer[writePos++] = c;
	}
    }

    releaseOutput();
}

// Wrapper for serial.available.
// This is used by a client to turn on an LED when input is available
bool CommandParser::inputAvailable()
{
    return serial.available();
}

const Command * CommandParser::getCommand()
{
    const Command *matching_cmd = 0;
    uint8_t match_length = 0;

    for (uint8_t cmd_num = 0; true; cmd_num++)
    {
	const Command *cmd = &commands[cmd_num];
	// Read first word in a command which is the pointer
	// to the command string stored in program memory
	const char *cmd_p = (const char *)pgm_read_ptr(cmd);
	if (cmd_p == 0)
            break;

	for (int8_t i = 0; i <= writePos - readPos; i++)
	{
	    char cmd_c = pgm_read_byte(cmd_p + i);
	    char buf_c = buffer[i + readPos];
	    if (cmd_c == '\0' || buf_c == ' ' || i == (writePos - readPos))
	    {
                // If it is a longer match then replace
                if (i > match_length)
                {
                    match_length = i;
                    matching_cmd = cmd;
                }
                break;
            }
            else if (buf_c != cmd_c)
            {
                // If not a match then continue with the next command
		break;
            }
	}
    }

    // Move the read position on for the matched command if there is one
    readPos += match_length;
    // Return any matching command
    return matching_cmd;
}

void CommandParser::processPacket()
{
#if COMMAND_INTERACTIVE
    // After each command is processed possibly output a prompt
    needsPrompt = true;
#endif

    if (writePos == 0)
	return;

    // Ignore lines that start with * as they are just responses from other devices on the RS485 bus
    if (buffer[0] == '*')
    {
	clearBuffer();
	return;
    }

    // If the first part of the packet is a hex digit then this an
    // a device address. Strip this off and compare with the address
    // of this device.
    if (buffer[0] == '@')
    {
	readPos++;
	int command_address;
	// Check supplied address is equal to actual address or the broadcast
	// address
	if (!parseHex(&command_address) ||
	    (command_address != rs485Address && command_address != 0xff))
	{
	    clearBuffer();
	    return;
	}

	skipSpace();
    }
    else if (rs485Address != 0)
    {
	// If the command parser has an address and no address is supplied in
	// the command then return. This stops multiple devices fighting when
	// there are bus errors

#if COMMAND_STATS
        statsMissingAddress++;
#endif

	clearBuffer();
	return;
    }

#if COMMAND_CRC
    // Scan the packet buffer looking for the expected CRC and
    // also calculating the CRC along the way.
    // NOTE: Only the device that is addressed checks the CRC values
    bool has_crc = false;
    uint16_t expected_crc = 0;
    crc = CRC_INITIAL;
    addCRC = false;
    for (int8_t i = 0; i < writePos - readPos; i++)
    {
        char c = buffer[i + readPos];
        if (c == '$')
            has_crc = true;
        else if (has_crc)
            expected_crc = (expected_crc<<4) | convertNimble(c);
        else
            crc = _crc_ccitt_update(crc, c);
    }
    if (has_crc)
    {
        if (expected_crc != crc)
        {
            // FIXME Just for testing.
            print(PSTR("CRC mismatch"));
            printVarHex(PSTR("calculated"), crc);
            printVarHex(PSTR("expected"), expected_crc);

#if COMMAND_STATS
            statsCRCMismatch++;
#endif

            // This device was addressed but an error occured so return an error
            // Not completely sure this is better than a silent fail?
            clearBuffer();
            doStatus(false);
            return;
        }

        crc = CRC_INITIAL;
        addCRC = true;
    }

#endif

    // Support chaining of commands together on the same line to save bandwidth
    bool is_ok = true;
    while(true)
    {
        const Command *cmd = getCommand();
        if (cmd == 0)
        {
            print(PSTR("Not valid command"));
            printLine();
            is_ok = false;

#if COMMAND_STATS
            statsInvalidCommand++;
#endif

            break;
        }

        bool res = processCommand(cmd);
        is_ok &= res;

#if COMMAND_STATS
        if (res)
            statsCommandOK++;
        else
            statsCommandError++;
#endif

        // Fail on first error
        if (!is_ok)
            break;

        // Check if there is another command in this packet
        if (readPos < writePos && buffer[readPos] == ';')
        {
            readPos++;
            skipSpace();
        }
        else
            break;
    }

    clearBuffer();
    doStatus(is_ok);
}

bool CommandParser::processCommand(const Command *c)
{
    // Need to read the callback from program memory
    CommandCallbackFunction ccf =
        (CommandCallbackFunction)pgm_read_ptr(&c->callback);

    // Call the callback
    return ccf(this);
}

void CommandParser::doStatus(bool res)
{
    // The OK and ERROR responses are always output as the last part of a packet
    if (res)
	print(PSTR("OK\n"));
    else
	print(PSTR("ERROR\n"));
}

// Parse a byte
bool CommandParser::parseByte(uint8_t *b)
{
    skipSpace();

    uint8_t val = 0;

    uint8_t p = 0;
    while (readPos < writePos)
    {
	char c = buffer[readPos++];
	if (c >= '0' && c <= '9')
	    val = val * 10 + ( c - '0');
	else
	{
	    readPos--;
	    break;
	}

	p++;
    }

    if (p != 0)
    {
        *b = val;
        return true;
    }
    else
        return false;
}

// Parse an integer
bool CommandParser::parseInt(int *i)
{
    skipSpace();

    int val = 0;
    int sign = 1;

    uint8_t p = 0;
    while (readPos < writePos)
    {
	char c = buffer[readPos++];
	if (p == 0 && c == '-')
	    sign = -1;
	else if (c >= '0' && c <= '9')
	    val = val * 10 + ( c - '0');
	else
	{
	    readPos--;
	    break;
	}

	p++;
    }

    val *= sign;

    if (p != 0)
    {
        *i = val;
        return true;
    }
    else
        return false;
}

// Parse a long integer
bool CommandParser::parseLong(long *i)
{
    skipSpace();

    long val = 0;
    long sign = 1;

    uint8_t p = 0;
    while (readPos < writePos)
    {
	char c = buffer[readPos++];
	if (p == 0 && c == '-')
	    sign = -1;
	else if (c >= '0' && c <= '9')
	    val = val * 10 + ( c - '0');
	else
	{
	    readPos--;
	    break;
	}

	p++;
    }

    val *= sign;

    if (p != 0)
    {
        *i = val;
        return true;
    }
    else
        return false;
}

int CommandParser::convertNimble(char c)
{
    // Note it is deliberate that lower case 'a' to 'f' are not considered a hex
    // nimble as if they are then starts of commands are confused with initial
    // RS485 addresses.
    if (c >= '0' && c <= '9')
	return c - '0';
    else if (c >= 'A' && c <= 'F')
	return c - 'A' + 10;
    else
	return -1;
}

bool CommandParser::parseHex(int *i)
{
    skipSpace();

    int val = 0;

    uint8_t p = 0;
    while (readPos < writePos)
    {
	char c = buffer[readPos++];
	int v = convertNimble(c);
	if (v >= 0)
	    val = val * 16 + v;
	else
	{
	    readPos--;
	    break;
	}

	p++;
    }

    if (p != 0)
    {
        *i = val;
        return true;
    }
    else
        return false;
}

// Parse a string
bool CommandParser::parseString(char *str, uint8_t max_length)
{
    skipSpace();

    uint8_t p;
    for (p = 0; p < max_length; p++)
    {
	if (readPos >= writePos)
	    break;

	char c = buffer[readPos++];
	if (c == ' ' || c == ';')
	    break;

	str[p] = c;
    }

    if (p != 0)
    {
        str[p] = '\0';
        return true;
    }
    else
        return false;
}

// Parse the rest of the line including any spaces
bool CommandParser::parseRest(char *str, uint8_t max_length)
{
    skipSpace();

    uint8_t p;
    for (p = 0; p < max_length; p++)
    {
	if (readPos >= writePos)
	    break;

	char c = buffer[readPos++];
        if (c == ';')
            break;

	str[p] = c;
    }

    if (p != 0)
    {
	str[p] = '\0';

	return true;
    }
    else
	return false;
}

#if COMMAND_FLOAT
bool CommandParser::parseFloat(float *f)
{
    skipSpace();

    bool negative_mantissa = false;
    bool negative_exponent = false;
    float mantissa = 0;
    float fract_part = 1.0;
    int8_t exponent = 0;
    enum Stage {
        IN_INT,
        IN_FRACT,
        IN_EXPONENT
    };
    enum Stage stage = IN_INT;

    while (readPos < writePos)
    {
	char c = buffer[readPos++];

        if (c >= '0' && c <= '9')
        {
            int d = c - '0';
            if (stage == IN_INT)
                mantissa = mantissa*10.0f + d;
            else if (stage == IN_FRACT)
            {
                fract_part /= 10;
                mantissa += d * fract_part;
            }
            else if (stage == IN_EXPONENT)
                exponent = exponent*10 + d;
        }
        else if (c == '-')
        {
            if (stage == IN_INT)
                negative_mantissa = true;
            else if (stage == IN_EXPONENT)
                negative_exponent = true;
            else
                return false;
        }
        else if (c == '+')
        {
            // Ignore redundant '+' if it occurs in sensible position

            if (stage != IN_INT && stage != IN_EXPONENT)
                return false;
        }
        else if (c == '.' && stage == IN_INT)
            stage = IN_FRACT;
        else if (c == 'e' || c == 'E')
        {
            if (stage == IN_INT || stage == IN_FRACT)
                stage = IN_EXPONENT;
            else
                return false;
        }
        else if (c == ',')
            continue;
        else if (c == ' ')
        {
            readPos--;
            break;
        }
        else
            return false;
    }

    if (negative_mantissa)
        mantissa = -mantissa;

    float fp_result = mantissa;
    while(exponent > 0)
    {
        if (negative_exponent)
            fp_result /= 10.0f;
        else
            fp_result *= 10.0f;

        --exponent;
    }

    *f = fp_result;
    return true;
}
#endif

void CommandParser::skipSpace()
{
    while (readPos < writePos && buffer[readPos] == ' ')
	readPos++;
}

void CommandParser::putChar(char c)
{
    grabOutput();
    if (needResponsePrefix)
        sendResponsePrefix();

    if (c == '\n')
    {
#if COMMAND_CRC
        if (addCRC)
        {
            serial.print('$');
            serial.print(crc, HEX);
            crc = CRC_INITIAL;
        }
#endif

        serial.print('\r');
        serial.print('\n');

        // Add a prefix to the next line as we run around the loop
        needResponsePrefix = true;
    }
    else
    {
#if COMMAND_CRC
        if (addCRC)
            crc = _crc_ccitt_update(crc, c);
#endif

        serial.print(c);
    }
}

void CommandParser::print(const char * PROGMEM str)
{
    while (true)
    {
	char c = pgm_read_byte(str++);

	if (c == '\0')
	    break;

        putChar(c);
    }
}

// All responses have a prefix so other units on the bus can ignore
// the response messages
void CommandParser::sendResponsePrefix()
{
    needResponsePrefix = false;

#if COMMAND_INTERACTIVE
    if (interactive)
        return;
#endif

    if (rs485Address != 0)
    {
        serial.print('*');
        serial.print(rs485Address, HEX);
        serial.print(' ');
    }
}

void CommandParser::putNibble(uint8_t n)
{
    if (n < 10)
        putChar('0' + n);
    else
        putChar('A' + n - 10);
}

void CommandParser::putByteHex(uint8_t b)
{
    putNibble(b>>4);
    putNibble(b & 0x0f);
}

void CommandParser::putNumber(unsigned long i,
                              int8_t width,
                              bool leading_zeros,
                              uint8_t base,
                              uint8_t num_factors,
                              unsigned long max_factor)
{
#if COMMAND_ADVANCED_FORMAT
    // Padding and truncation for right justification
    if (width > 0)
    {
        // Pad out the string to the correct width
        while (width > num_factors)
        {
            width--;
            if (leading_zeros)
                putChar('0');
            else
                putChar(' ');
        }

        while (num_factors > width && i < max_factor)
        {
            num_factors--;
            max_factor /= base;
        }
    }
#else
    width = width;
    num_factors = num_factors;
#endif

    unsigned long f = max_factor;
    while (true)
    {
        if (f == 0)
            break;

        unsigned int d = i/f;

        if (d != 0 || leading_zeros || f == 1)
        {
            putNibble(d);
            i -= d*f;

            // Once we have output a digit then
            // all other parts must have a zero
            leading_zeros = true;

#if COMMAND_ADVANCED_FORMAT
            if (width < 0)
                width++;
#endif
        }
#if COMMAND_ADVANCED_FORMAT
        else if (width > 0)
            putChar(' ');
#endif

        f /= base;
    }

#if COMMAND_ADVANCED_FORMAT
    // Padding for left justification
    while (width < 0)
    {
        putChar(' ');
        width++;
    }
#endif
}

void CommandParser::printHex(unsigned long var,
                             int8_t width,
                             bool leading_zeros)
{
    print(PSTR("0x"));
    putNumber(var, width, leading_zeros, 16, 8, 0x10000000);
}

void CommandParser::printDecimal(long var,
                                 int8_t width,
                                 bool leading_zeros)
{
    // Output the sign
    if (var < 0)
    {
        // FIXME not correct for something like %10d
        putChar('-');
        var = -var;

        if (width > 0)
            width--;
        else if (width < 0)
            width++;
    }

    putNumber(var, width, leading_zeros, 10, 10, 1000000000);
}

void CommandParser::printBinary(unsigned long var,
                                int8_t width)
{
    putNumber(var, width, true, 2, 32, 0x80000000);
}

#if COMMAND_FLOAT
void CommandParser::printFloat(float var, int8_t decimal_places)
{
    if (var < 0)
    {
        putChar('-');
        var = -var;
    }

    int v = int(var);
    printDecimal(v);
    float res = var - v;

    if (decimal_places > 0)
    {
        putChar('.');
        float fract = res;
        for(uint8_t i = 0; i < decimal_places; i++)
            fract *= 10;

        printDecimal(fract, decimal_places, true);
    }
}
#endif

void CommandParser::printVarHex(const PROGMEM char *str, int var)
{
    // The print does the grabOutput() and prefix handling
    print(str);
    putChar('=');
    printHex(var);
    printLine();
}

void CommandParser::printVar(const PROGMEM char *str, int var)
{
    // The print does the grabOutput() and prefix handling
    print(str);
    putChar('=');
    printDecimal(var);
    printLine();
}

void CommandParser::printVar(const PROGMEM char *str, unsigned int var)
{
    // The print does the grabOutput() and prefix handling
    print(str);
    putChar('=');
    printDecimal(var);
    printLine();
}

void CommandParser::printVar(const PROGMEM char *str, long var)
{
    // The print does the grabOutput() and prefix handling
    print(str);
    putChar('=');
    printDecimal(var);
    printLine();
}

void CommandParser::printVar(const PROGMEM char *str, unsigned long var)
{
    // The print does the grabOutput() and prefix handling
    print(str);
    putChar('=');
    printDecimal(var);
    printLine();
}

void CommandParser::printVar(const PROGMEM char *str, const char *var)
{
    // The print does the grabOutput() and prefix handling
    print(str);
    putChar('=');
    while (true)
    {
	char c = *var++;
	if (c == '\0')
	    break;

        putChar(c);
    }
    printLine();
}

#if COMMAND_FLOAT
void CommandParser::printVar(const char * PROGMEM str, float var)
{
    print(str);
    putChar('=');
    printFloat(var);
    printLine();
}
#endif

void CommandParser::printLine()
{
    putChar('\n');
}

// Output the help information
void CommandParser::showHelp()
{
    int cmd_num = 0;
    while(1)
    {
	const Command *cmd = &commands[cmd_num++];
	const char *c_name = (const char *)pgm_read_ptr(&cmd->name);
	if (c_name == 0)
	    break;

	const char *c_arguments =
	    (const char *)pgm_read_ptr(&cmd->arguments);

	const char *c_description =
	    (const char *)pgm_read_ptr(&cmd->description);

	print(c_name);
	if (c_arguments != 0)
	{
	    print(PSTR(" "));
	    print(c_arguments);
	}
	print(PSTR(" : "));
	print(c_description);
	printLine();
    }
}

void CommandParser::grabOutput()
{
    if (rs485TXEN > 0)
    {
	// Previously we were just checking the output register not the
	// input register. This is still correct and will detect the
        // actual state of the line.
	if (!digitalRead(rs485TXEN))
	{
	    digitalWrite(rs485TXEN, HIGH);

	    // Any response now must first output the response prefix
	    needResponsePrefix = true;
	}
    }
}

void CommandParser::releaseOutput()
{
#if COMMAND_INTERACTIVE
    // Show the prompt if not interactive
    if (interactive && needsPrompt)
    {
        serial.write("> ");
        needsPrompt = false;
    }
#endif

    if (rs485TXEN > 0)
    {
	if (digitalRead(rs485TXEN))
	{
	    // Wait for the last character to be sent
	    // Note that this only works if the bit is cleared in the
	    // write routine.
#if defined(__AVR__)
#  if defined(UCSRA)
	    while((UCSRA & ( 1 << TXC)) == 0)
		continue;
#  elif defined(UCSR0A)
	    while((UCSR0A & ( 1 << TXC0)) == 0)
		continue;
#  endif
#else
            // Modern versions of the Arduino library supports flush that
            // will wait until all characters are sent.
            serial.flush();
#endif

	    digitalWrite(rs485TXEN, LOW);
	}
    }
}

#if COMMAND_STATS
void CommandParser::clearStats()
{
    statsMaxPacketLength = 0;
    statsIllegalCharacter = 0;
#if COMMAND_CRC
    statsCRCMismatch = 0;
#endif
    statsMissingAddress = 0;
    statsInvalidCommand = 0;
    statsCommandError = 0;
    statsCommandOK = 0;
}

// Output the statistics
void CommandParser::showStats()
{
    printVar(PSTR("statsMaxPacketLength"), statsMaxPacketLength);
    printVar(PSTR("statsIllegalCharacter"), statsIllegalCharacter);
#if COMMAND_CRC
    printVar(PSTR("statsCRCMismatch"), statsCRCMismatch);
#endif
    printVar(PSTR("statsMissingAddress"), statsMissingAddress);
    printVar(PSTR("statsInvalidCommand"), statsInvalidCommand);
    printVar(PSTR("statsCommandError"), statsCommandError);
    printVar(PSTR("statsCommandOK"), statsCommandOK);
}

#endif

#if COMMAND_INTERACTIVE
void CommandParser::setInteractive(bool b)
{
    interactive = b;
    needsPrompt = b;
}
#endif
