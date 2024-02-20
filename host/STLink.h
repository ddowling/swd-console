#pragma once

#include <stlink.h>

class STLink
{
public:
    STLink();
    ~STLink();

    bool open();
    void close();

    // Read and write method handle switching between the 8 bit and 32 bit
    // variants depending on address alignment. For large transfers efficient
    // 32 bit transfers will be used for the aligned sections of the data
    bool read(uint8_t *ptr, size_t address, size_t size);
    bool write(uint8_t *ptr, size_t address, size_t size);

    void getRAM(size_t &base, size_t &size);
    void getFlash(size_t &base, size_t &size);
    
protected:
    stlink_t *handle;
};

