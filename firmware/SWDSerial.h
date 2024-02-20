#pragma once

#include <Stream.h>

#define SWDSERIAL_MAGIC 0xd5715e0d

class SWDSerial : public Stream
{
public:
    SWDSerial();

    // Stream overrides
    virtual int available();
    virtual int read();
    virtual int peek();

    // Print overrides
    virtual size_t write(uint8_t c);
    virtual size_t write(const uint8_t *buffer, size_t size);
    virtual int availableForWrite();

protected:
    // Implement 255 (256-1) byte circular buffers for input and output
    // If head==tail then the buffer is empty. If head+1==tail the buffer is full
    // Writes go to the position indicated by head. Reads work from the tail
    uint32_t magic;
    uint8_t outHead;
    uint8_t outTail;
    uint8_t inHead;
    uint8_t inTail;
    uint8_t outBuffer[256];
    uint8_t inBuffer[256];
};

        
