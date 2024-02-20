#include "STLink.h"
#include <assert.h>
#include <string.h>
#include <iostream>

extern "C"
{
#include <read_write.h>
}

#define MAKE_STR(x) _MAKE_STR(x)
#define _MAKE_STR(x) #x


STLink::STLink()
    : handle(nullptr)
{
    const char *chip_dir = MAKE_STR(STLINK_CHIPS_DIR);
    char buf[256];
    strcpy(buf, chip_dir);
    init_chipids(buf);
}

STLink::~STLink()
{
    close();
}

bool STLink::open()
{
    enum ugly_loglevel loglevel = UERROR;
    enum connect_type  ct = CONNECT_HOT_PLUG;
    char *serial_number = 0;
    
    handle = stlink_open_usb(loglevel, ct, serial_number,
                             STLINK_SWDCLK_4MHZ_DIVISOR);

    if (handle == nullptr)
    {
        std::cerr << "Failed to open the debugger\n";
        return false;
    }
    
    stlink_set_swdclk(handle, 24000); // Run STLINK/V3 @ 24MHz


    if (stlink_load_device_params(handle) != 0)
    {
        std::cerr << "Failed to load device parameters\n";
        return false;
    }

    std::cout << "Chip Id " << handle->chip_id << "\n";

    return true;
}

void STLink::close()
{
    if (handle != nullptr)
    {
        stlink_close(handle);
        handle = nullptr;
    }
}

bool STLink::read(uint8_t *ptr, size_t address, size_t size)
{
    // Seems to lockup if trying to read larger than 0x1000
    size_t block_size = 0x1000;
    
    while (size != 0)
    {
        if (size < block_size)
            block_size = size;
        
        // Need to align the address and size
        int address_offset = address % 4;
        int size_offset = address_offset;
        if ((block_size + size_offset) % 4 != 0)
            size_offset += 4 - ((block_size + size_offset) % 4);
    
        // Reads have to be aligned to 32bit boundaries.
        // Does not like doing reads or writes of zero size
        assert(block_size + size_offset <= Q_BUF_LEN);
        if (stlink_read_mem32(handle, address - address_offset,
                              block_size + size_offset))
        {
            
            perror("Failed to read from device\n");
            return false;
        }
    
        memcpy(ptr, handle->q_buf + address_offset, block_size);
        size -= block_size;
        address += block_size;
        ptr += block_size;
    }
    
    return true;
}

bool STLink::write(uint8_t *ptr, size_t address, size_t size)
{
    size_t block_size = 0x1000;

    // Does not like doing reads or writes of zero size
    while (size != 0)
    {
        if (size < block_size)
            block_size = size;
        
        assert(size <= Q_BUF_LEN);
        memcpy(handle->q_buf, ptr, size);

        uint8_t unaligned_address_offset = address % 4;
        if (unaligned_address_offset % 4 != 0 || block_size < 4)
        {
            // Do 8 bit writes at the start to align the address on a word
            // boundary. Also do 8 bit writes are the end to finish off
            // the buffer.
            
            // Write 1 to 3 bytes to align on the address boundary
            if (unaligned_address_offset + block_size > 4)
                block_size = 4 - unaligned_address_offset;

            if (stlink_write_mem8(handle, address, block_size) )
            {
                
                perror("Failed to write to device\n");
                return false;
            }
        }
        else
        {
            // Address is word aligned so just need to align the size
            block_size -= (block_size % 4);
            
            if (stlink_write_mem32(handle, address, size) )
            {
                perror("Failed to read from device\n");
                return false;
            }
        }

        address += block_size;
        size -= block_size;
    }

    return true;
}

void STLink::getRAM(size_t &base, size_t &size)
{
    base = handle->sram_base;
    size = handle->sram_size;
}

void STLink::getFlash(size_t &base, size_t &size)
{
    base = handle->flash_base;
    size = handle->flash_size;
}

