#include "SWDPrint.h"

SWDPrint::SWDPrint()
    : magic(SWDPRINT_MAGIC),
      outHead(0),
      outTail(0)
{
}

// Print overrides
size_t SWDPrint::write(uint8_t c)
{
    return write(&c, 1);
}

size_t SWDPrint::write(const uint8_t *buffer, size_t size)
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

int SWDPrint::availableForWrite()
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
