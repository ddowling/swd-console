#pragma once

#include <Stream.h>

#define SWDPRINT_MAGIC 0xd5715e0c

class SWDPrint : public Print
{
public:
    SWDPrint();

    // Print overrides
    virtual size_t write(uint8_t c);
    virtual size_t write(const uint8_t *buffer, size_t size);
    virtual int availableForWrite();

protected:
    // Implement 255 (256-1) byte circular buffer for output
    // If head==tail then the buffer is empty.
    // If head+1==tail the buffer is full
    // Writes go to the position indicated by head. Reads work from the tail
    uint32_t magic;
    uint8_t outHead;
    uint8_t outTail;
    uint8_t unused[2];
    uint8_t outBuffer[256];
};

        
