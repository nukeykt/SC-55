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
#include <windows.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "serial.h"

#define BUFFER_SIZE 4096 

static int SERIAL_strnicmp(const char* a, const char* b, int len)
{
    int diff = 0;
    char chrA, chrB;
    for (int i = 0; i < len && !diff; i++)
    {
        chrA = *(a + i);
        chrA |= 0x20 * (chrA >= 'A' && chrA <= 'Z');
        chrB = *(b + i);
        chrB |= 0x20 * (chrB >= 'A' && chrB <= 'Z');
        diff |= chrA ^ chrB;
    }
    return diff;
}

static inline bool IsSerialPort(const char * path) {
    size_t len = strlen(path);
    if (*path == '\\' && len >= 4) {
        if (memcmp(path, "\\\\.\\", 4) != 0)
            return false;
        len -= 4;
        path += 4;
    }
    return len > 3 && SERIAL_strnicmp(path, "COM", 3) == 0 && atoi(path + 3) != 0;
}

static inline bool IsNamedPipe(const char * path) {
    size_t pathlen = strnlen(path, 257);
    if (pathlen < 10 || pathlen > 256)
        return false;
    if (memcmp(path, "\\\\", 2) != 0)
        return false;
    const char * hostname = path + 2;
    const char * pipename = hostname;
    while (*pipename != 0 && *pipename != '\\') pipename++;
    if (hostname == pipename)
        return false; // No hostname
    if (strnlen(pipename, 7) < 7 || SERIAL_strnicmp(pipename, "\\pipe\\", 6) != 0)
        return false;
    pipename += 7;
    return true;
}

static bool serial_init = false;
static bool is_serial_port = false;
static bool is_named_pipe = false;

static const * serial_path;
static HANDLE handle;
static OVERLAPPED olRead;
static OVERLAPPED olWrite;
static uint8_t read_buffer[BUFFER_SIZE];
static uint8_t * read_ptr = read_buffer;
static uint8_t * read_end = read_buffer;
static const uint8_t * read_limit = read_buffer + BUFFER_SIZE;
static uint8_t write_buffer[BUFFER_SIZE];
static uint8_t * write_ptr = write_buffer;
static uint8_t * write_end = write_buffer;
static const uint8_t * write_limit = write_buffer + BUFFER_SIZE;
static bool read_pending = false;
static bool write_pending = false;

static void Cleanup() {
    CloseHandle(handle);
    handle = INVALID_HANDLE_VALUE;
}

static void ReportIOError(DWORD error) {
    fprintf(stderr, "Serial I/O Error: %ld\n", error);
    fflush(stderr);
}

int SERIAL_Init(const char* path) {
    if (serial_init) return;
    // printf("is_serial_port: %d is_named_pipe: %d \n", IsSerialPort(path), IsNamedPipe(path));
    if (!IsSerialPort(path) && !IsNamedPipe(path)) {
        fprintf(stderr, "Can't open '%s': Not a serial port or named pipe\n", path);
        fflush(stderr);
        return false;
    }
    is_serial_port = IsSerialPort(path);
    is_named_pipe = !is_serial_port;

    serial_path = calloc(strlen(path) + 5, sizeof(char));
    if (is_serial_port && *path != '\\') {
        memcpy(serial_path, "\\\\.\\", 4);
    }
    strcat(serial_path, path);

    handle = CreateFileA(
        serial_path,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        NULL
    );
    if (handle == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Unable to open serial port: %s errcode: %d\n", path, GetLastError());
        fflush(stderr);
        free(serial_path);
        return false;
    }

    memset(&olRead, 0, sizeof(OVERLAPPED));
    memset(&olWrite, 0, sizeof(OVERLAPPED));

    olRead.hEvent = CreateEventA(NULL, true, false, NULL);
    olWrite.hEvent = CreateEventA(NULL, true, false, NULL);

    serial_init = 1;
    printf("Opened serial port '%s'\n", path);
    return true;
}
void SERIAL_Update(uint64_t cycles) {
    if (!serial_init || handle == INVALID_HANDLE_VALUE) return;
    // printf("readptr: %04x readend: %04x\n", read_ptr - read_buffer, read_end - read_buffer);
    if (read_ptr == read_end && !read_pending) {
        if (read_end == read_limit) {
            read_ptr = read_buffer;
            read_end = read_buffer;
        }
        DWORD dwReads = read_limit - read_end;
        bool success = ReadFile(handle, read_end, dwReads, &dwReads, &olRead);
        if (success) {
            read_end += dwReads;
            read_pending = false;
        } else {
            DWORD error = GetLastError();
            if (error != ERROR_IO_PENDING) {
                ReportIOError(error);
                Cleanup();
            } else {
                read_pending = true;
            }
        }
    }
    if (read_pending) {
        DWORD dwReads;
        bool success = GetOverlappedResult(handle, &olRead, &dwReads, false);
        if (success) {
            read_end += dwReads;
            read_pending = false;
        } else {
            DWORD error = GetLastError();
            if (error != ERROR_IO_INCOMPLETE) {
                ReportIOError(error);
                Cleanup();
            }
        }
    }
    if (write_ptr != write_end && !write_pending) {
        DWORD dwWrite = write_end - write_ptr;
        if (dwWrite < 0) {
            dwWrite += BUFFER_SIZE;
        }
        bool success = WriteFile(handle, write_ptr, dwWrite, &dwWrite, &olWrite);
        if (success) {
            write_ptr += dwWrite;
            write_pending = false;
        } else {
            DWORD error = GetLastError();
            if (error != ERROR_IO_PENDING) {
                ReportIOError(error);
                Cleanup();
            } else {
                write_pending = true;
            }
        }
    }
    if (write_pending) {
        DWORD dwWrite;
        bool success = GetOverlappedResult(handle, &olWrite, &dwWrite, false);
        if (success) {
            write_ptr += dwWrite;
            write_pending = false;
        } else {
            DWORD error = GetLastError();
            if (error != ERROR_IO_INCOMPLETE) {
                ReportIOError(error);
                Cleanup();
            }
        }
    }
    if (write_end == write_limit) {
        write_end = write_buffer;
    }
    if (write_ptr == write_limit) {
        write_ptr = write_buffer;
    }
}
int SERIAL_HasData() {
    return read_ptr < read_end;
}
uint8_t SERIAL_ReadUART() {
    if (read_ptr < read_end) {
        return *read_ptr++;
    }
    return 0;
}
void SERIAL_PostUART(uint8_t data) {
    if (!serial_init) return;
    if (write_end == write_limit) {
        fprintf(stderr, "SERIAL TX OVERFLOW, THIS IS A BUG\n");
        fflush(stderr);
        return;
    }
    *write_end++ = data;
}