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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "SDL.h"
#include "SDL_mutex.h"
#include "lcd.h"
#include "lcd_font.h"
#include "mcu.h"
#include "submcu.h"
#include "utils/files.h"


static uint32_t LCD_DL, LCD_N, LCD_F, LCD_D, LCD_C, LCD_B, LCD_ID, LCD_S;
static uint32_t LCD_DD_RAM, LCD_AC, LCD_CG_RAM;
static uint32_t LCD_RAM_MODE = 0;
static uint8_t LCD_Data[80];
static uint8_t LCD_CG[64];

static uint8_t lcd_enable = 1;
static bool lcd_quit_requested = false;
static uint8_t lcd_contrast = 0;

void LCD_SetContrast(uint8_t contrast)
{
    if (mcu_jv880)
    {
        if (contrast > 10)
            contrast = 10;
        lcd_contrast = contrast; // FIXME
    }
    else
    {
        if (contrast > 16)
            contrast = 16;
        if (contrast < 1)
            contrast = 1;
        lcd_contrast = contrast;
    }
}

void LCD_Enable(uint32_t enable)
{
    lcd_enable = enable;
}

bool LCD_QuitRequested()
{
    return lcd_quit_requested;
}

void LCD_Write(uint32_t address, uint8_t data)
{
    if (address == 0)
    {
        if ((data & 0xe0) == 0x20)
        {
            LCD_DL = (data & 0x10) != 0;
            LCD_N = (data & 0x8) != 0;
            LCD_F = (data & 0x4) != 0;
        }
        else if ((data & 0xf8) == 0x8)
        {
            LCD_D = (data & 0x4) != 0;
            LCD_C = (data & 0x2) != 0;
            LCD_B = (data & 0x1) != 0;
        }
        else if ((data & 0xff) == 0x01)
        {
            LCD_DD_RAM = 0;
            LCD_ID = 1;
            memset(LCD_Data, 0x20, sizeof(LCD_Data));
        }
        else if ((data & 0xff) == 0x02)
        {
            LCD_DD_RAM = 0;
        }
        else if ((data & 0xfc) == 0x04)
        {
            LCD_ID = (data & 0x2) != 0;
            LCD_S = (data & 0x1) != 0;
        }
        else if ((data & 0xc0) == 0x40)
        {
            LCD_CG_RAM = (data & 0x3f);
            LCD_RAM_MODE = 0;
        }
        else if ((data & 0x80) == 0x80)
        {
            LCD_DD_RAM = (data & 0x7f);
            LCD_RAM_MODE = 1;
        }
        else
        {
            address += 0;
        }
    }
    else
    {
        if (!LCD_RAM_MODE)
        {
            LCD_CG[LCD_CG_RAM] = data & 0x1f;
            if (LCD_ID)
            {
                LCD_CG_RAM++;
            }
            else
            {
                LCD_CG_RAM--;
            }
            LCD_CG_RAM &= 0x3f;
        }
        else
        {
            if (LCD_N)
            {
                if (LCD_DD_RAM & 0x40)
                {
                    if ((LCD_DD_RAM & 0x3f) < 40)
                        LCD_Data[(LCD_DD_RAM & 0x3f) + 40] = data;
                }
                else
                {
                    if ((LCD_DD_RAM & 0x3f) < 40)
                        LCD_Data[LCD_DD_RAM & 0x3f] = data;
                }
            }
            else
            {
                if (LCD_DD_RAM < 80)
                    LCD_Data[LCD_DD_RAM] = data;
            }
            if (LCD_ID)
            {
                LCD_DD_RAM++;
            }
            else
            {
                LCD_DD_RAM--;
            }
            LCD_DD_RAM &= 0x7f;
        }
    }
    //printf("%i %.2x ", address, data);
    // if (data >= 0x20 && data <= 'z')
    //     printf("%c\n", data);
    //else
    //    printf("\n");
}

int lcd_width = 741;
int lcd_height = 268;
static const int lcd_width_max = 1024;
static const int lcd_height_max = 1024;
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;

static std::string m_back_path = "back.data";

static uint32_t lcd_buffer[lcd_height_max][lcd_width_max];
static uint32_t lcd_background[268][741];

static uint32_t lcd_init = 0;

const int button_map_sc55[][2] =
{
    SDL_SCANCODE_Q, MCU_BUTTON_POWER,
    SDL_SCANCODE_W, MCU_BUTTON_INST_ALL,
    SDL_SCANCODE_E, MCU_BUTTON_INST_MUTE,
    SDL_SCANCODE_R, MCU_BUTTON_PART_L,
    SDL_SCANCODE_T, MCU_BUTTON_PART_R,
    SDL_SCANCODE_Y, MCU_BUTTON_INST_L,
    SDL_SCANCODE_U, MCU_BUTTON_INST_R,
    SDL_SCANCODE_I, MCU_BUTTON_KEY_SHIFT_L,
    SDL_SCANCODE_O, MCU_BUTTON_KEY_SHIFT_R,
    SDL_SCANCODE_P, MCU_BUTTON_LEVEL_L,
    SDL_SCANCODE_LEFTBRACKET, MCU_BUTTON_LEVEL_R,
    SDL_SCANCODE_A, MCU_BUTTON_MIDI_CH_L,
    SDL_SCANCODE_S, MCU_BUTTON_MIDI_CH_R,
    SDL_SCANCODE_D, MCU_BUTTON_PAN_L,
    SDL_SCANCODE_F, MCU_BUTTON_PAN_R,
    SDL_SCANCODE_G, MCU_BUTTON_REVERB_L,
    SDL_SCANCODE_H, MCU_BUTTON_REVERB_R,
    SDL_SCANCODE_J, MCU_BUTTON_CHORUS_L,
    SDL_SCANCODE_K, MCU_BUTTON_CHORUS_R,
    SDL_SCANCODE_LEFT, MCU_BUTTON_PART_L,
    SDL_SCANCODE_RIGHT, MCU_BUTTON_PART_R,
};

const int button_map_jv880[][2] =
{
    SDL_SCANCODE_P, MCU_BUTTON_PREVIEW,
    SDL_SCANCODE_LEFT, MCU_BUTTON_CURSOR_L,
    SDL_SCANCODE_RIGHT, MCU_BUTTON_CURSOR_R,
    SDL_SCANCODE_TAB, MCU_BUTTON_DATA,
    SDL_SCANCODE_Q, MCU_BUTTON_TONE_SELECT,
    SDL_SCANCODE_A, MCU_BUTTON_PATCH_PERFORM,
    SDL_SCANCODE_W, MCU_BUTTON_EDIT,
    SDL_SCANCODE_E, MCU_BUTTON_SYSTEM,
    SDL_SCANCODE_R, MCU_BUTTON_RHYTHM,
    SDL_SCANCODE_T, MCU_BUTTON_UTILITY,
    SDL_SCANCODE_S, MCU_BUTTON_MUTE,
    SDL_SCANCODE_D, MCU_BUTTON_MONITOR,
    SDL_SCANCODE_F, MCU_BUTTON_COMPARE,
    SDL_SCANCODE_G, MCU_BUTTON_ENTER,
};


void LCD_SetBackPath(const std::string &path)
{
    m_back_path = path;
}

void LCD_Init(void)
{
    FILE *raw;

    if(lcd_init)
        return;

    lcd_quit_requested = false;

    std::string title = "Nuked SC-55: ";

    title += rs_name[romset];

    window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, lcd_width, lcd_height, SDL_WINDOW_SHOWN);
    if (!window)
        return;

    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer)
        return;

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR888, SDL_TEXTUREACCESS_STREAMING, lcd_width, lcd_height);

    if (!texture)
        return;

    raw = Files::utf8_fopen(m_back_path.c_str(), "rb");
    if (!raw)
        return;

    fread(lcd_background, 1, sizeof(lcd_background), raw);
    fclose(raw);

    lcd_init = 1;
}

void LCD_UnInit(void)
{
    if(!lcd_init)
        return;
}

uint32_t lcd_col1 = 0x000000;
uint32_t lcd_col2 = 0x0050c8;

void LCD_FontRenderStandard(int32_t x, int32_t y, uint8_t ch, bool overlay = false)
{
    uint8_t* f;
    if (ch >= 16)
        f = &lcd_font[ch - 16][0];
    else
        f = &LCD_CG[(ch & 7) * 8];
    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; j < 5; j++)
        {
            uint32_t col;
            if (f[i] & (1<<(4-j)))
            {
                col = lcd_col1;
            }
            else
            {
                col = lcd_col2;
            }
            int xx = x + i * 6;
            int yy = y + j * 6;
            for (int ii = 0; ii < 5; ii++)
            {
                for (int jj = 0; jj < 5; jj++)
                {
                    if (overlay)
                    {
                        if (lcd_buffer[xx+ii][yy+jj] != col && col == lcd_col1)
                            lcd_buffer[xx+ii][yy+jj] = col;
                    }
                    else
                        lcd_buffer[xx+ii][yy+jj] = col;
                }
            }
        }
    }
}

void LCD_FontRenderLevel(int32_t x, int32_t y, uint8_t ch, uint8_t width = 5)
{
    uint8_t* f;
    if (ch >= 16)
        f = &lcd_font[ch - 16][0];
    else
        f = &LCD_CG[(ch & 7) * 8];
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < width; j++)
        {
            uint32_t col;
            if (f[i] & (1<<(4-j)))
            {
                col = lcd_col1;
            }
            else
            {
                col = lcd_col2;
            }
            int xx = x + i * 11;
            int yy = y + j * 26;
            for (int ii = 0; ii < 9; ii++)
            {
                for (int jj = 0; jj < 24; jj++)
                {
                    lcd_buffer[xx+ii][yy+jj] = col;
                }
            }
        }
    }
}

static const uint8_t LR[2][12][11] =
{
    {
        1,1,0,0,0,0,0,0,0,0,0,
        1,1,0,0,0,0,0,0,0,0,0,
        1,1,0,0,0,0,0,0,0,0,0,
        1,1,0,0,0,0,0,0,0,0,0,
        1,1,0,0,0,0,0,0,0,0,0,
        1,1,0,0,0,0,0,0,0,0,0,
        1,1,0,0,0,0,0,0,0,0,0,
        1,1,0,0,0,0,0,0,0,0,0,
        1,1,0,0,0,0,0,0,0,0,0,
        1,1,0,0,0,0,0,0,0,0,0,
        1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,
    },
    {
        1,1,1,1,1,1,1,1,1,0,0,
        1,1,1,1,1,1,1,1,1,1,0,
        1,1,0,0,0,0,0,0,1,1,0,
        1,1,0,0,0,0,0,0,1,1,0,
        1,1,0,0,0,0,0,0,1,1,0,
        1,1,1,1,1,1,1,1,1,1,0,
        1,1,1,1,1,1,1,1,1,0,0,
        1,1,0,0,0,0,0,1,1,0,0,
        1,1,0,0,0,0,0,0,1,1,0,
        1,1,0,0,0,0,0,0,1,1,0,
        1,1,0,0,0,0,0,0,0,1,1,
        1,1,0,0,0,0,0,0,0,1,1,
    }
};

static const int LR_xy[2][2] = {
    { 70, 264 },
    { 232, 264 }
};


void LCD_FontRenderLR(uint8_t ch)
{
    uint8_t* f;
    if (ch >= 16)
        f = &lcd_font[ch - 16][0];
    else
        f = &LCD_CG[(ch & 7) * 8];
    int col;
    if (f[0] & 1)
    {
        col = lcd_col1;
    }
    else
    {
        col = lcd_col2;
    }
    for (int f = 0; f < 2; f++)
    {
        for (int i = 0; i < 12; i++)
        {
            for (int j = 0; j < 11; j++)
            {
                if (LR[f][i][j])
                    lcd_buffer[i+LR_xy[f][0]][j+LR_xy[f][1]] = col;
            }
        }
    }
}

static uint32_t inline LCD_MixColor(uint32_t color, uint8_t contrast)
{
    uint8_t b = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t r = color & 0xFF;

    b = (b * contrast) >> 8;
    g = (g * contrast) >> 8;
    r = (r * contrast) >> 8;

    return (color & 0xFF000000) | ((b & 0xFF) << 16) | ((g & 0xFF) << 8) | (r & 0xFF);
}

void LCD_Update(void)
{
    if (!lcd_init)
        return;

    if (!mcu_cm300 && !mcu_st && !mcu_scb55)
    {
        MCU_WorkThread_Lock();

        if (!lcd_enable && !mcu_jv880)
        {
            memset(lcd_buffer, 0, sizeof(lcd_buffer));
        }
        else
        {
            if (mcu_jv880)
            {
                for (size_t i = 0; i < lcd_height; i++) {
                    for (size_t j = 0; j < lcd_width; j++) {
                        lcd_buffer[i][j] = 0xFF03be51;
                    }
                }
            }
            else
            {
                for (size_t i = 0; i < lcd_height; i++) {
                    for (size_t j = 0; j < lcd_width; j++) {
                        lcd_buffer[i][j] = lcd_background[i][j];
                    }
                }
            }

            uint32_t con = 0x11 * (lcd_contrast - 1);
            con = (con * con) >> 8;
            lcd_col2 = LCD_MixColor(lcd_buffer[0][0], 0xFF - (con / 4 + 4));
            lcd_col1 = LCD_MixColor(lcd_col2, 0x11 * (16 - (((lcd_contrast + 1) >> 1) + 4)));

            if (mcu_jv880)
            {
                for (int i = 0; i < 2; i++)
                {
                    for (int j = 0; j < 24; j++)
                    {
                        uint8_t ch = LCD_Data[i * 40 + j];
                        LCD_FontRenderStandard(10 + i * 50, 4 + j * 34, ' ');
                        LCD_FontRenderStandard(4 + i * 50, 4 + j * 34, ch);
                    }
                }
                
                // cursor
                int j = LCD_DD_RAM % 0x40;
                int i = LCD_DD_RAM / 0x40;
                if (i < 2 && j < 24 && LCD_C)
                    LCD_FontRenderStandard(10 + i * 50, 4 + j * 34, '_', true);
            }
            else
            {
                for (int i = 0; i < 3; i++)
                {
                    uint8_t ch = LCD_Data[0 + i];
                    LCD_FontRenderStandard(11, 34 + i * 35, ch);
                }
                for (int i = 0; i < 16; i++)
                {
                    uint8_t ch = LCD_Data[3 + i];
                    LCD_FontRenderStandard(11, 153 + i * 35, ch);
                }
                for (int i = 0; i < 3; i++)
                {
                    uint8_t ch = LCD_Data[40 + i];
                    LCD_FontRenderStandard(75, 34 + i * 35, ch);
                }
                for (int i = 0; i < 3; i++)
                {
                    uint8_t ch = LCD_Data[43 + i];
                    LCD_FontRenderStandard(75, 153 + i * 35, ch);
                }
                for (int i = 0; i < 3; i++)
                {
                    uint8_t ch = LCD_Data[49 + i];
                    LCD_FontRenderStandard(139, 34 + i * 35, ch);
                }
                for (int i = 0; i < 3; i++)
                {
                    uint8_t ch = LCD_Data[46 + i];
                    LCD_FontRenderStandard(139, 153 + i * 35, ch);
                }
                for (int i = 0; i < 3; i++)
                {
                    uint8_t ch = LCD_Data[52 + i];
                    LCD_FontRenderStandard(203, 34 + i * 35, ch);
                }
                for (int i = 0; i < 3; i++)
                {
                    uint8_t ch = LCD_Data[55 + i];
                    LCD_FontRenderStandard(203, 153 + i * 35, ch);
                }

                LCD_FontRenderLR(LCD_Data[58]);

                for (int i = 0; i < 2; i++)
                {
                    for (int j = 0; j < 4; j++)
                    {
                        uint8_t ch = LCD_Data[20 + j + i * 40];
                        LCD_FontRenderLevel(71 + i * 88, 293 + j * 130, ch, j == 3 ? 1 : 5);
                    }
                }
            }
        }

        MCU_WorkThread_Unlock();

        SDL_UpdateTexture(texture, NULL, lcd_buffer, lcd_width_max * 4);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    SDL_Event sdl_event;
    while (SDL_PollEvent(&sdl_event))
    {
        if (sdl_event.type == SDL_KEYDOWN)
        {
            if (sdl_event.key.keysym.scancode == SDL_SCANCODE_COMMA)
                MCU_EncoderTrigger(0);
            if (sdl_event.key.keysym.scancode == SDL_SCANCODE_PERIOD)
                MCU_EncoderTrigger(1);
        }

        switch (sdl_event.type)
        {
            case SDL_QUIT:
                lcd_quit_requested = true;
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                if (sdl_event.key.repeat)
                    continue;
                
                int mask = 0;
                uint32_t button_pressed = (uint32_t)SDL_AtomicGet(&mcu_button_pressed);

                auto button_map = mcu_jv880 ? button_map_jv880 : button_map_sc55;
                auto button_size = (mcu_jv880 ? sizeof(button_map_jv880) : sizeof(button_map_sc55)) / sizeof(button_map_sc55[0]);
                for (size_t i = 0; i < button_size; i++)
                {
                    if (button_map[i][0] == sdl_event.key.keysym.scancode)
                        mask |= (1 << button_map[i][1]);
                }

                if (sdl_event.type == SDL_KEYDOWN)
                    button_pressed |= mask;
                else
                    button_pressed &= ~mask;

                SDL_AtomicSet(&mcu_button_pressed, (int)button_pressed);

#if 0
                if (sdl_event.key.keysym.scancode >= SDL_SCANCODE_1 && sdl_event.key.keysym.scancode < SDL_SCANCODE_0)
                {
#if 0
                    int kk = sdl_event.key.keysym.scancode - SDL_SCANCODE_1;
                    if (sdl_event.type == SDL_KEYDOWN)
                    {
                        MCU_PostUART(0xc0);
                        MCU_PostUART(118);
                        MCU_PostUART(0x90);
                        MCU_PostUART(0x30 + kk);
                        MCU_PostUART(0x7f);
                    }
                    else
                    {
                        MCU_PostUART(0x90);
                        MCU_PostUART(0x30 + kk);
                        MCU_PostUART(0);
                    }
#endif
                    int kk = sdl_event.key.keysym.scancode - SDL_SCANCODE_1;
                    const int patch = 47;
                    if (sdl_event.type == SDL_KEYDOWN)
                    {
                        static int bend = 0x2000;
                        if (kk == 4)
                        {
                            MCU_PostUART(0x99);
                            MCU_PostUART(0x32);
                            MCU_PostUART(0x7f);
                        }
                        else if (kk == 3)
                        {
                            bend += 0x100;
                            if (bend > 0x3fff)
                                bend = 0x3fff;
                            MCU_PostUART(0xe1);
                            MCU_PostUART(bend & 127);
                            MCU_PostUART((bend >> 7) & 127);
                        }
                        else if (kk == 2)
                        {
                            bend -= 0x100;
                            if (bend < 0)
                                bend = 0;
                            MCU_PostUART(0xe1);
                            MCU_PostUART(bend & 127);
                            MCU_PostUART((bend >> 7) & 127);
                        }
                        else if (kk)
                        {
                            MCU_PostUART(0xc1);
                            MCU_PostUART(patch);
                            MCU_PostUART(0xe1);
                            MCU_PostUART(bend & 127);
                            MCU_PostUART((bend >> 7) & 127);
                            MCU_PostUART(0x91);
                            MCU_PostUART(0x32);
                            MCU_PostUART(0x7f);
                        }
                        else if (kk == 0)
                        {
                            //MCU_PostUART(0xc0);
                            //MCU_PostUART(patch);
                            MCU_PostUART(0xe0);
                            MCU_PostUART(0x00);
                            MCU_PostUART(0x40);
                            MCU_PostUART(0x99);
                            MCU_PostUART(0x37);
                            MCU_PostUART(0x7f);
                        }
                    }
                    else
                    {
                        if (kk == 1)
                        {
                            MCU_PostUART(0x91);
                            MCU_PostUART(0x32);
                            MCU_PostUART(0);
                        }
                        else if (kk == 0)
                        {
                            MCU_PostUART(0x99);
                            MCU_PostUART(0x37);
                            MCU_PostUART(0);
                        }
                        else if (kk == 4)
                        {
                            MCU_PostUART(0x99);
                            MCU_PostUART(0x32);
                            MCU_PostUART(0);
                        }
                    }
                }
#endif
                break;
            }
        }
    }
}

