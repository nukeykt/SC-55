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
#pragma once

#include <cstdint>
#include <filesystem>

struct mcu_t;

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
union SDL_Event;

static const int lcd_width_max = 1024;
static const int lcd_height_max = 1024;

struct lcd_t {
    mcu_t* mcu = nullptr;

    size_t width = 0;
    size_t height = 0;

    uint32_t color1 = 0;
    uint32_t color2 = 0;

    uint32_t LCD_DL = 0, LCD_N = 0, LCD_F = 0, LCD_D = 0, LCD_C = 0, LCD_B = 0, LCD_ID = 0, LCD_S = 0;
    uint32_t LCD_DD_RAM = 0, LCD_AC = 0, LCD_CG_RAM = 0;
    uint32_t LCD_RAM_MODE = 0;
    uint8_t LCD_Data[80]{};
    uint8_t LCD_CG[64]{};

    uint8_t enable = 0;
    bool quit_requested = false;

    uint32_t buffer[lcd_height_max][lcd_width_max]{};
    uint32_t background[268][741]{};

    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *texture = nullptr;
};


void LCD_LoadBack(lcd_t& lcd, const std::filesystem::path& path);
void LCD_Init(lcd_t& lcd, mcu_t& mcu);
bool LCD_CreateWindow(lcd_t& lcd);
void LCD_UnInit(lcd_t& lcd);
void LCD_Write(lcd_t& lcd, uint32_t address, uint8_t data);
void LCD_Enable(lcd_t& lcd, uint32_t enable);
bool LCD_QuitRequested(lcd_t& lcd);
void LCD_Sync(void);
void LCD_Update(lcd_t& lcd);
void LCD_HandleEvent(lcd_t& lcd, const SDL_Event& sdl_event);
