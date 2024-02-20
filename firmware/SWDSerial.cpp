#include "SWDSerial.h"

SWDSerial::SWDSerial()
    : magic(SWDSERIAL_MAGIC),
      outHead(0),
      outTail(0),
      inHead(0),
      inTail(0)
{
}

// Stream overrides
int SWDSerial::available()
{
    return (inHead - inTail) % sizeof(inBuffer);
}

int SWDSerial::read()
{
    if (inHead == inTail)
        return -1;

    uint8_t next = inTail + 1;
    if (next >= sizeof(inBuffer))
        next = 0;
    
    uint8_t res = inBuffer[next];
    inTail = next;
    
    return res;
}

int SWDSerial::peek()
{
    if (inHead == inTail)
        return -1;
    else
        return inBuffer[inTail];
}

// Print overrides
size_t SWDSerial::write(uint8_t c)
{
    return write(&c, 1);
}

size_t SWDSerial::write(const uint8_t *buffer, size_t size)
{
    size_t i;
    for (i = 0; i < size; i++)
    {
        int next = outHead + 1;
        if (next >= sizeof(outBuffer))
            next = 0;

        if (next == outTail)
        {
#if 1
        // Overwrite
        int next_tail = outTail + 1;
        if (next_tail >= sizeof(outBuffer))
            next_tail = 0;
        outTail = next_tail;
#else
            break;
#endif
        }
        
        outBuffer[next] = buffer[i];
        outHead = next;
    }
    
    return i;
}

int SWDSerial::availableForWrite()
{
#if 1
    return sizeof(outBuffer);
#else
    if (outHead < outTail)
        return outTail - outHead;
    else
        return sizeof(outBuffer) + outTail - outHead;
#endif
}
