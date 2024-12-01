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
#include <cstdio>
#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer codes
#include <string.h> // strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h> // open(), write(), read(), close()
#include <vector>

#define BUFFER_SIZE 4096
#define INVALID_VALUE -1

SDL_mutex *linux_io_lock = nullptr;
SDL_Thread *linux_io_thread = nullptr;
bool linux_thread_run = false; 

class SerialHandler
{
    private:
        bool serial_init = false;
        int port_handle = INVALID_VALUE;
        
        uint8_t linux_read_buffer[BUFFER_SIZE];
        uint8_t * linux_read_ptr = linux_read_buffer;
        uint8_t * linux_read_end = linux_read_buffer;
        bool read_pending = true;
        
        const uint8_t * linux_read_limit = linux_read_buffer + BUFFER_SIZE;
        uint8_t linux_write_buffer[BUFFER_SIZE];
        uint8_t * linux_write_ptr = linux_write_buffer;
        uint8_t * linux_write_end = linux_write_buffer;
        const uint8_t * linux_write_limit = linux_write_buffer + BUFFER_SIZE;
        bool write_pending = false;
    public:
        bool SerialInit(std::string_view serial_port);
        void SerialClose();
        bool IsSerialInit() { return serial_init && port_handle!=INVALID_VALUE; }
        void ResetReadPending() { read_pending = false; }
        void SetReadPending() { read_pending = true; }
        bool IsReadPending() { return read_pending; }
        void ResetWritePending() { write_pending = false; }
        void SetWritePending() { write_pending = true; }
        bool IsWritePending() { return write_pending; }
        int SerialIOUpdate();
        std::vector<uint8_t> GetReadBytes();
        void SetWriteBytes(std::vector<uint8_t> write_data);
};

bool SerialHandler::SerialInit(std::string_view serial_port)
{
    port_handle = open(std::string(serial_port).c_str(), O_RDWR|O_NOCTTY);
    if(port_handle == INVALID_VALUE)
    {
        fprintf(stderr, "Failed to open serial port: %s\n", std::string(serial_port).c_str());
        return false;
    }

    termios tty_handle;
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

    fprintf(stderr, "Opened serial port %s\n", std::string(serial_port).c_str());
    serial_init = true;
    return true;
}

void SerialHandler::SerialClose()
{
    if(!IsSerialInit())
        return;

    close(port_handle);
    port_handle = INVALID_VALUE;
    serial_init = false;
}

int SerialHandler::SerialIOUpdate()
{

    if(!IsReadPending())
    {
        SDL_LockMutex(linux_io_lock);

        linux_read_ptr = linux_read_buffer;
        linux_read_end = linux_read_buffer;
        
        int16_t read_bytes = read(port_handle, linux_read_end, BUFFER_SIZE * sizeof(uint8_t));
        if(read_bytes == INVALID_VALUE)
        {
            fprintf(stderr, "Error %i reading from serial port: %s\n", errno, strerror(errno));
            SDL_UnlockMutex(linux_io_lock);
            return INVALID_VALUE;
        }
        if(read_bytes > 0)
        {
            linux_read_end += read_bytes;
            SetReadPending();
        }
        
        SDL_UnlockMutex(linux_io_lock);
    }

    if(IsWritePending())
    {
        SDL_LockMutex(linux_io_lock);
        
        int16_t write_len = linux_write_end - linux_write_ptr;
        int16_t write_bytes = write(port_handle, linux_write_ptr, write_len * sizeof(uint8_t));
        if(write_len < 0)
        {
            fprintf(stderr, "Error %i writing to serial port: %s\n", errno, strerror(errno));
            SDL_UnlockMutex(linux_io_lock);
            return INVALID_VALUE;
        }
        if(write_bytes > 0)
        {
            linux_write_ptr += write_bytes;
            if(linux_write_ptr >= linux_write_end)
            {
                linux_write_ptr = linux_write_buffer;
                linux_write_end = linux_write_buffer;
                ResetWritePending();
            }
        }

        SDL_UnlockMutex(linux_io_lock);
    }
    return 0;
}

std::vector<uint8_t> SerialHandler::GetReadBytes()
{
    std::vector<uint8_t> read_bytes;
    while(linux_read_ptr < linux_read_end)
        read_bytes.push_back(*linux_read_ptr++);

    return read_bytes;
}

void SerialHandler::SetWriteBytes(std::vector<uint8_t> write_data)
{
    for(auto byte:write_data)
        *linux_write_end++ = byte;
}

static SerialHandler *shandler = nullptr;
std::vector<uint8_t> read_buffer;
std::vector<uint8_t> write_buffer;

int SDLCALL Linux_IO_Run(void *ptr)
{
    while(linux_thread_run)
    {
        shandler->SerialIOUpdate();
    }
    return 0;
}

bool SERIAL_Init(FE_Application& fe, std::string_view serial_port)
{
    (void)fe;

    shandler = new SerialHandler;
    if(!shandler->SerialInit(serial_port))
        return false;
    
    linux_io_lock = SDL_CreateMutex();
    linux_io_thread = SDL_CreateThread(Linux_IO_Run, "LinuxIO", nullptr);

    return true;
}

bool SERIAL_HasData() 
{
    if(shandler == nullptr || !shandler->IsSerialInit())
        return false;

    return !read_buffer.empty();
}

void SERIAL_Update()
{
    if(shandler == nullptr || !shandler->IsSerialInit())
        return;

    if(shandler->IsReadPending())
    {
        read_buffer = shandler->GetReadBytes();
        shandler->ResetReadPending();
    }
    if(!shandler->IsWritePending())
    {
        shandler->SetWriteBytes(write_buffer);
        shandler->SetWritePending();
    }
}

uint8_t SERIAL_ReadUART()
{
    if(shandler == nullptr || !shandler->IsSerialInit())
        return 0;
    
    if (!read_buffer.empty()) 
    {
        uint8_t byte = *read_buffer.cbegin();
        read_buffer.erase(read_buffer.begin());
        return byte;
    }
    return 0;
}

void SERIAL_PostUART(uint8_t byte)
{
    if(shandler == nullptr || !shandler->IsSerialInit())
        return;
    
    if(write_buffer.size() < BUFFER_SIZE)
        write_buffer.push_back(byte);
}

void SERIAL_Close()
{
    if(!shandler->IsSerialInit())
        return;

    linux_thread_run = false;
    SDL_WaitThread(linux_io_thread, nullptr);
    SDL_DestroyMutex(linux_io_lock);
    
    shandler->SerialClose();
    delete shandler;
    shandler = nullptr;
}
