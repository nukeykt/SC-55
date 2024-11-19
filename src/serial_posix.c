/*
 * Copyright (C) 2021, 2024 nukeykt
 *
 *  Redistribution and use of this code or any derivative works are permitted
 *  provided that the following conditions are met:
 *
 *   - Redistributions may not be sold, nor may they be used in a commercial
 *     product or activity.
 *
 *   - Redistributions that are modified from the original source must include the
 *     complete source code, including the source code for all components used by a
 *     binary built from the modified sources. However, as a special exception, the
 *     source code distributed need not include anything that is normally distributed
 *     (in either source or binary form) with the major components (compiler, kernel,
 *     and so on) of the operating system on which the executable runs, unless that
 *     component itself accompanies the executable.
 *
 *   - Redistributions must reproduce the above copyright notice, this list of
 *     conditions and the following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */
#include "serial.h"
#include "SDL.h"
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer codes
#include <string.h> // strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h> // open(), write(), read(), close()

#define BUFFER_SIZE 4096
#define INVALID_VALUE -1

static bool serial_init = false;
static int port_handle = INVALID_VALUE;
static uint8_t read_buffer[BUFFER_SIZE];
static uint8_t * read_ptr = read_buffer;
static uint8_t * read_end = read_buffer;
static const uint8_t * read_limit = read_buffer + BUFFER_SIZE;
static uint8_t write_buffer[BUFFER_SIZE];
static uint8_t * write_ptr = write_buffer;
static uint8_t * write_end = write_buffer;
static const uint8_t * write_limit = write_buffer + BUFFER_SIZE;

static uint8_t linux_read_buffer[BUFFER_SIZE];
static uint8_t * linux_read_ptr = linux_read_buffer;
static uint8_t * linux_read_end = linux_read_buffer;
static const uint8_t * linux_read_limit = linux_read_buffer + BUFFER_SIZE;
static uint8_t linux_write_buffer[BUFFER_SIZE];
static uint8_t * linux_write_ptr = linux_write_buffer;
static uint8_t * linux_write_end = linux_write_buffer;
static const uint8_t * linux_write_limit = linux_write_buffer + BUFFER_SIZE;
static bool read_pending = true;
static bool write_pending = false;

static SDL_mutex *linux_io_lock = NULL;
static SDL_Thread *linux_io_thread = NULL;
static bool linux_thread_run = false; 

int SDLCALL linux_io_update(void *ptr)
{
    ptr=(void *)ptr;

    if (!serial_init || port_handle == INVALID_VALUE)
        return INVALID_VALUE;
    
    while(linux_thread_run)
    {
        if(!read_pending)
        {
            SDL_LockMutex(linux_io_lock);

            if(linux_read_ptr >= linux_read_limit)
                linux_read_ptr = linux_read_buffer;
            if(linux_read_end >= linux_read_limit)
                linux_read_end = linux_read_buffer;
            
            int32_t read_bytes = read(port_handle, linux_read_end, sizeof(linux_read_limit-linux_read_end));
            if(read_bytes == INVALID_VALUE)
            {
                fprintf(stderr, "Error %i reading from serial port: %s\n", errno, strerror(errno));
                SDL_UnlockMutex(linux_io_lock);
                return INVALID_VALUE;
            }
            if(read_bytes > 0)
            {
                linux_read_end += read_bytes;
                read_pending = true;
            }
            
            SDL_UnlockMutex(linux_io_lock);
        }

        if(write_pending)
        {
            SDL_LockMutex(linux_io_lock);
            
            for(uint8_t *byte=linux_write_ptr; byte!=linux_write_limit; byte++)
                *linux_write_end++ = *byte;
            int32_t write_len = linux_write_end - linux_write_ptr;
            uint8_t *linux_towrite = linux_write_ptr;
            if(write_len < 0)
            {
                if(linux_towrite >= linux_write_limit) 
                {
                    write_len += BUFFER_SIZE;
                    linux_towrite = linux_write_buffer;
                    linux_write_ptr = linux_write_buffer;
                } 
                else 
                    write_len = linux_write_limit - linux_towrite;
            }

            int write_bytes = write(port_handle, linux_towrite, write_len*sizeof(uint8_t));
            while(write_bytes < 0)
            {
                fprintf(stderr, "Error %i writing to serial port: %s\n", errno, strerror(errno));
                SDL_UnlockMutex(linux_io_lock);
            }
            linux_write_ptr += write_bytes;
            write_pending = false;

            SDL_UnlockMutex(linux_io_lock);
        }
    }
    return 0;
}

int SERIAL_Init(const char* path) 
{
    if(serial_init)
        return false;

    port_handle = open(path, O_RDWR|O_NOCTTY);
    if(port_handle == INVALID_VALUE)
    {
        fprintf(stderr, "Failed to open serial port: %s\n", path);
        return false;
    }

    struct termios tty_handle;
    // Read in existing settings, and handle any error
    if(tcgetattr(port_handle, &tty_handle) != 0) {
        fprintf(stderr, "Error %i from getting tty attributes: %s\n", errno, strerror(errno));
        return false;
    }

    // Clear parity bit, stop field (use only one stop bit) and data size bits.
    tty_handle.c_cflag &= ~(PARENB|CSTOPB|CSIZE);
    // 8 bits per byte, turn on READ and ignore ctrl lines
    tty_handle.c_cflag |= (CS8|CREAD|CLOCAL);

    // Disable echo, erasure, new-line echo ,interpretation of INTR, QUIT and SUSP & Turn off s/w flow ctrl
    tty_handle.c_iflag &= ~(ICANON|ECHO|ECHOE|ECHONL|ISIG|IXON|IXOFF|IXANY);
    tty_handle.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

    tty_handle.c_oflag &= ~(OPOST|ONLCR); // Prevent special interpretation of output bytes (e.g. newline chars)
    // Prevent conversion of newline to carriage return/line feed

    tty_handle.c_cc[VTIME] = 0;    // Do not wait, process asap
    tty_handle.c_cc[VMIN] = 0;

    tcflush(port_handle, TCIFLUSH);  

    // Save tty settings, also checking for error
    if (tcsetattr(port_handle, TCSANOW, &tty_handle) != 0) {
        fprintf(stderr, "Error %i from getting tty attributes: %s\n", errno, strerror(errno));
        return false;
    }

    fprintf(stderr, "Opened serial port %s\n", path);
    serial_init = true;

    linux_thread_run = true;
    linux_io_thread = SDL_CreateThread(linux_io_update, "LinuxIO", NULL); 

    return true;
}

void SERIAL_Update(uint64_t cycles) 
{
    (void)cycles;
    if (!serial_init || port_handle == INVALID_VALUE)
        return;
    
    if(read_ptr == read_end && read_pending) 
    {
        if (read_end >= read_limit) 
        {
            read_ptr = read_buffer;
            read_end = read_buffer;
        }

        while(linux_read_ptr < linux_read_end && read_end <= read_limit)
            *read_end++ = *linux_read_ptr++;
        read_pending = false;
    }

    if(write_ptr != write_end && !write_pending)
    {
        int16_t len = write_end - write_ptr;
        uint8_t *towrite = write_ptr;
        if (len < 0)
        {
            if(towrite == write_limit) 
            {
                len += BUFFER_SIZE;
                towrite = write_buffer;
                write_ptr = write_buffer;
            } 
            else 
            {
                len = write_limit - towrite;
            }
        }

        for(int i=0; i<len; i++)
        {
            if(linux_write_end >= linux_write_limit)
                linux_write_end = linux_write_buffer;
            
            *linux_write_end++ = *(towrite + i);
        }
        write_ptr += len;
    }
}

int SERIAL_HasData() 
{
    return read_ptr < read_end;
}

uint8_t SERIAL_ReadUART()
{
    if (read_ptr < read_end) {
        return *read_ptr++;
    }
    return 0;
}

void SERIAL_PostUART(uint8_t data) 
{
    if (!serial_init || port_handle == INVALID_VALUE) 
        return;

    if (write_end == write_ptr - 1) {
        fprintf(stderr, "SERIAL TX OVERFLOW, THIS IS A BUG\n");
        fflush(stderr);
        return;
    }

    *write_end++ = data;
    if (write_end == write_limit)
        write_end = write_buffer;
}

void SERIAL_Close() 
{
    if(!serial_init)
        return;
    
    linux_thread_run = false;
    SDL_WaitThread(linux_io_thread, NULL);
    SDL_DestroyMutex(linux_io_lock);

    close(port_handle);
    port_handle = INVALID_VALUE;
    serial_init = false;
}
