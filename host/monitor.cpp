#include "STLink.h"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>

#include <vector>
#include <string>
#include <iostream>

#define SWDPRINT_MAGIC  0xd5715e0c
#define SWDSTREAM_MAGIC 0xd5715e0d

static volatile bool running = true;

void intHandler(int /*sig*/)
{
    running = false;
}

int main(int argc, char **argv)
{
    signal(SIGINT, intHandler);
    signal(SIGTERM, intHandler);
    signal(SIGQUIT, intHandler);
    
    STLink stlink;

    if (!stlink.open())
        return 1;

    size_t addr = 0;

    if (true)
    {
        // Seatch for the magic address in the RAM
        size_t ram_base, ram_size;
        stlink.getRAM(ram_base, ram_size);

        std::vector<uint8_t> ram;
        ram.resize(ram_size);

        printf("Reading RAM\n");
        if (!stlink.read(ram.data(), ram_base, 0x1000))
        {
            printf("Could not read ram\n");
            return 1;
        }

	// FIXME Need to addd SWDPRINT_MAGIC
        uint32_t magic = SWDSTREAM_MAGIC;
        uint8_t *magic_ptr = (uint8_t *)&magic;
        
        printf("Looking for SWD magic numbers in memory\n");
        for (size_t i = 0; i < ram_size-3; i+=4)
        {
            if (ram[i] == magic_ptr[0] &&
                ram[i+1] == magic_ptr[1] &&
                ram[i+2] == magic_ptr[2] &&
                ram[i+3] == magic_ptr[3])
            {
                addr = ram_base + i;
                break;
            }
        }

        if (addr == 0)
        {
            printf("Did not find any SWD magic numbers in memory\n");
            
            return 1;
        }

        printf("Found SWDSTREAM_MAGIC number at 0x%zx\n", addr);
    }

    struct termios orig_tty;
    
    bool is_tty = isatty(STDIN_FILENO);
    bool need_raw_terminal = is_tty;
   
    if (need_raw_terminal)
    {
        if (tcgetattr(STDIN_FILENO, &orig_tty) != 0)
        {
            perror("tcgetaddr");
            return 1;
        }

        struct termios raw_tty = orig_tty;
        raw_tty.c_lflag &= ~(ECHO | ICANON);
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_tty) != 0)
        {
            perror("tcsetattr");
            return 1;
        }

        // Don't want to block on character input
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        if (flags == -1)
        {
            perror("fcntl F_GETFL");
            return 1;
        }

        flags |= O_NONBLOCK;
        if (fcntl(STDIN_FILENO, F_SETFL, flags) == -1)
        {
            perror("fcntl F_SETFL");
            return 1;
        }

        printf("Exit with ^D\n");
    }
        
    size_t status_addr = addr + 4;
    size_t out_buffer_addr = addr + 4 + 4;
    size_t in_buffer_addr = addr + 4 + 4 + 256;
    
    while (running)
    {
        uint8_t status[4];
        if (!stlink.read((uint8_t *)&status, status_addr, 4))
            break;

        uint8_t out_head = status[0];
        uint8_t out_tail = status[1];
        uint8_t in_head = status[2];
        uint8_t in_tail = status[3];

#if 0
        printf("out_head=%d out_tail=%d in_head=%d in_tail=%d\n",
               out_head, out_tail, in_head, in_tail);
#endif

        // If we do not perform any input of output then sleep for
        // 1ms so we do not consume 100% CPU
        bool active = false;
        
        // Read from buffer
        if (out_head != out_tail)
        {
            uint8_t buffer[256];
            int pos = 0;
            // Writes go to the head and reads from the tail
            if (out_head > out_tail)
            {
                if (!stlink.read(buffer, out_buffer_addr + out_tail + 1,
                                 out_head - out_tail))
                    break;
                
                pos = out_head - out_tail;
            }
            else
            {
                // Buffer wrap around
                // Read from the buffer before wrap around
                if (out_tail < 255)
                    stlink.read(buffer, out_buffer_addr + out_tail + 1,
                                255 - out_tail);
                pos = 255 - out_tail;
                

                // Read rest
                if (out_head != 0)
                {
                    if (!stlink.read(buffer+pos, out_buffer_addr, out_head))
                        break;
                    
                    pos += out_head;
                }
            }

            // Update the tail pointer to empty the buffer
            out_tail = out_head;

            if (!stlink.write(&out_tail, status_addr + 1, 1))
                break;

            write(STDOUT_FILENO, buffer, pos);

            active = true;
        }

        // Write to buffer
        uint8_t buffer[256];
        uint8_t in_free = 255 - (in_head - in_tail);
        if (in_free > 0)
        {
            int res = (int)read(STDIN_FILENO, buffer, in_free);
            if (res > 0)
            {
                active = true;

                // FIXME Does not see the ^D if the output buffer is full
                // as we never call into here
                
                // If the buffer contains a ^D (EOT) character then exit
                // after outputing the current text
                uint8_t *eot_ptr = (uint8_t *)memchr(buffer, '\x04', res);
                if (eot_ptr != nullptr)
                {
                    res = eot_ptr - buffer;
                    running = false;
                }

                int count = res;
                if (in_head + count > 255)
                    count = 255 - in_head;

                // Write as much as possible to the end of the buffer
                if (count > 0)
                {
                    stlink.write(buffer, in_buffer_addr + in_head + 1, count);
                    res -= count;
                    in_head += count;
                }

                // If still more then write as the head
                if (res > 0)
                {
                    stlink.write(buffer, in_buffer_addr, res);
                    in_head = res - 1;
                }
                
                stlink.write(&in_head, status_addr + 2, 1);
            }
        }
        
        if (!active)
            usleep(1000);
    }

    stlink.close();
    
    printf("\nExit\n");

    if (need_raw_terminal)
    {
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_tty) != 0)
        {
            perror("tcsetattr");
            return 1;
        }
    }

    return 0;
}
