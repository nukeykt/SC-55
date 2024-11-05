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
#include "emu.h"
#include <fstream>

const SDL_Rect lcd_button_regions_sc55[32] = {
    {38, 36, 67, 19}, // Power
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {968, 38, 53, 18}, // Instrument
    {1024, 38, 53, 18},
    {754, 82, 26, 26}, // Mute
    {754, 35, 26, 26}, // All
    {0, 0, 0, 0},
    {968, 178, 53, 18}, // MIDI ch
    {1024, 178, 53, 18},
    {968, 132, 53, 18}, // Chorus
    {1024, 132, 53, 18},
    {968, 85, 53, 18}, // Pan
    {1024, 85, 53, 18},
    {903, 37, 53, 18}, // Part R
    {0, 0, 0, 0},
    {831, 178, 53, 18}, // Key shift
    {887, 178, 53, 18},
    {831, 132, 53, 18}, // Reverb
    {887, 132, 53, 18},
    {831, 85, 53, 18}, // Level
    {887, 85, 53, 18},
    {849, 37, 53, 18}, // Part L
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
};

uint32_t inline LCD_MixColor(uint32_t color, uint8_t contrast) {
    uint8_t b = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t r = color & 0xFF;

    b = (b * contrast) >> 8;
    g = (g * contrast) >> 8;
    r = (r * contrast) >> 8;

    return (color & 0xFF000000) | ((b & 0xFF) << 16) | ((g & 0xFF) << 8) | (r & 0xFF);
}

void LCD_Enable(lcd_t& lcd, uint32_t enable)
{
    lcd.enable = enable;
}

void LCD_ButtonEnable(lcd_t& lcd, uint8_t enable)
{
    lcd.button_enable = enable;
}

void LCD_SetContrast(lcd_t& lcd, uint8_t contrast)
{
    if (contrast > 16)
        contrast = 16;
    else if(contrast < 1)
        contrast = 1;

    lcd.contrast = contrast;
}

bool LCD_QuitRequested(lcd_t& lcd)
{
    return lcd.quit_requested;
}

void LCD_Write(lcd_t& lcd, uint32_t address, uint8_t data)
{
    if (address == 0)
    {
        if ((data & 0xe0) == 0x20)
        {
            lcd.LCD_DL = (data & 0x10) != 0;
            lcd.LCD_N = (data & 0x8) != 0;
            lcd.LCD_F = (data & 0x4) != 0;
        }
        else if ((data & 0xf8) == 0x8)
        {
            lcd.LCD_D = (data & 0x4) != 0;
            lcd.LCD_C = (data & 0x2) != 0;
            lcd.LCD_B = (data & 0x1) != 0;
        }
        else if ((data & 0xff) == 0x01)
        {
            lcd.LCD_DD_RAM = 0;
            lcd.LCD_ID = 1;
            memset(lcd.LCD_Data, 0x20, sizeof(lcd.LCD_Data));
        }
        else if ((data & 0xff) == 0x02)
        {
            lcd.LCD_DD_RAM = 0;
        }
        else if ((data & 0xfc) == 0x04)
        {
            lcd.LCD_ID = (data & 0x2) != 0;
            lcd.LCD_S = (data & 0x1) != 0;
        }
        else if ((data & 0xc0) == 0x40)
        {
            lcd.LCD_CG_RAM = (data & 0x3f);
            lcd.LCD_RAM_MODE = 0;
        }
        else if ((data & 0x80) == 0x80)
        {
            lcd.LCD_DD_RAM = (data & 0x7f);
            lcd.LCD_RAM_MODE = 1;
        }
        else
        {
            address += 0;
        }
    }
    else
    {
        if (!lcd.LCD_RAM_MODE)
        {
            lcd.LCD_CG[lcd.LCD_CG_RAM] = data & 0x1f;
            if (lcd.LCD_ID)
            {
                lcd.LCD_CG_RAM++;
            }
            else
            {
                lcd.LCD_CG_RAM--;
            }
            lcd.LCD_CG_RAM &= 0x3f;
        }
        else
        {
            if (lcd.LCD_N)
            {
                if (lcd.LCD_DD_RAM & 0x40)
                {
                    if ((lcd.LCD_DD_RAM & 0x3f) < 40)
                        lcd.LCD_Data[(lcd.LCD_DD_RAM & 0x3f) + 40] = data;
                }
                else
                {
                    if ((lcd.LCD_DD_RAM & 0x3f) < 40)
                        lcd.LCD_Data[lcd.LCD_DD_RAM & 0x3f] = data;
                }
            }
            else
            {
                if (lcd.LCD_DD_RAM < 80)
                    lcd.LCD_Data[lcd.LCD_DD_RAM] = data;
            }
            if (lcd.LCD_ID)
            {
                lcd.LCD_DD_RAM++;
            }
            else
            {
                lcd.LCD_DD_RAM--;
            }
            lcd.LCD_DD_RAM &= 0x7f;
        }
    }
    //fprintf(stderr, "%i %.2x ", address, data);
    // if (data >= 0x20 && data <= 'z')
    //     fprintf(stderr, "%c\n", data);
    //else
    //    fprintf(stderr, "\n");
}

const int button_map_sc55[][2] =
{
    {SDL_SCANCODE_Q, MCU_BUTTON_POWER},
    {SDL_SCANCODE_W, MCU_BUTTON_INST_ALL},
    {SDL_SCANCODE_E, MCU_BUTTON_INST_MUTE},
    {SDL_SCANCODE_R, MCU_BUTTON_PART_L},
    {SDL_SCANCODE_T, MCU_BUTTON_PART_R},
    {SDL_SCANCODE_Y, MCU_BUTTON_INST_L},
    {SDL_SCANCODE_U, MCU_BUTTON_INST_R},
    {SDL_SCANCODE_I, MCU_BUTTON_KEY_SHIFT_L},
    {SDL_SCANCODE_O, MCU_BUTTON_KEY_SHIFT_R},
    {SDL_SCANCODE_P, MCU_BUTTON_LEVEL_L},
    {SDL_SCANCODE_LEFTBRACKET, MCU_BUTTON_LEVEL_R},
    {SDL_SCANCODE_A, MCU_BUTTON_MIDI_CH_L},
    {SDL_SCANCODE_S, MCU_BUTTON_MIDI_CH_R},
    {SDL_SCANCODE_D, MCU_BUTTON_PAN_L},
    {SDL_SCANCODE_F, MCU_BUTTON_PAN_R},
    {SDL_SCANCODE_G, MCU_BUTTON_REVERB_L},
    {SDL_SCANCODE_H, MCU_BUTTON_REVERB_R},
    {SDL_SCANCODE_J, MCU_BUTTON_CHORUS_L},
    {SDL_SCANCODE_K, MCU_BUTTON_CHORUS_R},
    {SDL_SCANCODE_LEFT, MCU_BUTTON_PART_L},
    {SDL_SCANCODE_RIGHT, MCU_BUTTON_PART_R},
};

const int button_map_jv880[][2] =
{
    {SDL_SCANCODE_P, MCU_BUTTON_PREVIEW},
    {SDL_SCANCODE_LEFT, MCU_BUTTON_CURSOR_L},
    {SDL_SCANCODE_RIGHT, MCU_BUTTON_CURSOR_R},
    {SDL_SCANCODE_TAB, MCU_BUTTON_DATA},
    {SDL_SCANCODE_Q, MCU_BUTTON_TONE_SELECT},
    {SDL_SCANCODE_A, MCU_BUTTON_PATCH_PERFORM},
    {SDL_SCANCODE_W, MCU_BUTTON_EDIT},
    {SDL_SCANCODE_E, MCU_BUTTON_SYSTEM},
    {SDL_SCANCODE_R, MCU_BUTTON_RHYTHM},
    {SDL_SCANCODE_T, MCU_BUTTON_UTILITY},
    {SDL_SCANCODE_S, MCU_BUTTON_MUTE},
    {SDL_SCANCODE_D, MCU_BUTTON_MONITOR},
    {SDL_SCANCODE_F, MCU_BUTTON_COMPARE},
    {SDL_SCANCODE_G, MCU_BUTTON_ENTER},
};


void LCD_LoadBack(lcd_t& lcd, const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
        return;

    file.read((char*)lcd.background, sizeof(lcd.background));
}

void LCD_Init(lcd_t& lcd, mcu_t& mcu)
{
    lcd.mcu = &mcu;
}

bool LCD_CreateWindow(lcd_t& lcd)
{
    SDL_Surface *background_image = nullptr;
    background_image = SDL_LoadBMP("sc55mkII_background.bmp");

    size_t screen_width = 0;
    size_t screen_height = 0;

    if (lcd.mcu->romset == Romset::JV880)
    {
        lcd.width = 820;
        lcd.height = 100;
        screen_width = lcd.width;
        screen_height = lcd.height;
        lcd.color1 = 0x000000;
        lcd.color2 = 0x78b500;
    }
    else if(lcd.mcu->romset == Romset::MK2 && background_image)
    {
        
        lcd.width = 741;
        lcd.height = 268;
        screen_width = 1120;
        screen_height = 233;
        lcd.color1 = 0x000000;
        lcd.color2 = 0x0050c8;
        lcd.background_enabled = 1;
    }
    else
    {
        lcd.width = 741;
        lcd.height = 268;
        screen_width = lcd.width;
        screen_height = lcd.height;
        lcd.color1 = 0x000000;
        lcd.color2 = 0x0050c8;
    }

    std::string title = "Nuked SC-55: ";

    title += EMU_RomsetName(lcd.mcu->romset);

    lcd.window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen_width, screen_height, SDL_WINDOW_SHOWN);
    if (!lcd.window)
        return false;

    lcd.renderer = SDL_CreateRenderer(lcd.window, -1, 0);
    if (!lcd.renderer)
        return false;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "BEST");

    lcd.texture = SDL_CreateTexture(lcd.renderer, SDL_PIXELFORMAT_BGR888, SDL_TEXTUREACCESS_STREAMING, lcd.width, lcd.height);

    if (!lcd.texture)
        return false;

    if(lcd.background_enabled)
    {
        lcd.background_image = SDL_CreateTextureFromSurface(lcd.renderer, background_image);
        if(!lcd.background_image)
            return false;
    }

    return true;
}

void LCD_UnInit(lcd_t& lcd)
{
    if (lcd.texture)
    {
        SDL_DestroyTexture(lcd.texture);
        lcd.texture = nullptr;
    }
    if (lcd.background_image)
    {
        SDL_DestroyTexture(lcd.background_image);
        lcd.background_image = nullptr;
    }
    if (lcd.renderer)
    {
        SDL_DestroyRenderer(lcd.renderer);
        lcd.renderer = nullptr;
    }
    if (lcd.window)
    {
        SDL_DestroyWindow(lcd.window);
        lcd.window = nullptr;
    }
}

void LCD_FontRenderStandard(lcd_t& lcd, int32_t x, int32_t y, uint8_t ch, bool overlay = false)
{
    uint8_t* f;
    if (ch >= 16)
        f = &lcd_font[ch - 16][0];
    else
        f = &lcd.LCD_CG[(ch & 7) * 8];
    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; j < 5; j++)
        {
            uint32_t col;
            if (f[i] & (1<<(4-j)))
            {
                col = lcd.color1;
            }
            else
            {
                col = lcd.color2;
            }
            int xx = x + i * 6;
            int yy = y + j * 6;
            for (int ii = 0; ii < 5; ii++)
            {
                for (int jj = 0; jj < 5; jj++)
                {
                    if (overlay)
                    {
                        if (lcd.buffer[xx+ii][yy+jj] != col && col == lcd.color1)
                            lcd.buffer[xx+ii][yy+jj] = col;
                    }
                    else
                        lcd.buffer[xx+ii][yy+jj] = col;
                }
            }
        }
    }
}

void LCD_FontRenderLevel(lcd_t& lcd, int32_t x, int32_t y, uint8_t ch, uint8_t width = 5)
{
    uint8_t* f;
    if (ch >= 16)
        f = &lcd_font[ch - 16][0];
    else
        f = &lcd.LCD_CG[(ch & 7) * 8];
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < width; j++)
        {
            uint32_t col;
            if (f[i] & (1<<(4-j)))
            {
                col = lcd.color1;
            }
            else
            {
                col = lcd.color2;
            }
            int xx = x + i * 11;
            int yy = y + j * 26;
            for (int ii = 0; ii < 9; ii++)
            {
                for (int jj = 0; jj < 24; jj++)
                {
                    lcd.buffer[xx+ii][yy+jj] = col;
                }
            }
        }
    }
}

static const uint8_t LR[2][12][11] =
{
    {
        {1,1,0,0,0,0,0,0,0,0,0,},
        {1,1,0,0,0,0,0,0,0,0,0,},
        {1,1,0,0,0,0,0,0,0,0,0,},
        {1,1,0,0,0,0,0,0,0,0,0,},
        {1,1,0,0,0,0,0,0,0,0,0,},
        {1,1,0,0,0,0,0,0,0,0,0,},
        {1,1,0,0,0,0,0,0,0,0,0,},
        {1,1,0,0,0,0,0,0,0,0,0,},
        {1,1,0,0,0,0,0,0,0,0,0,},
        {1,1,0,0,0,0,0,0,0,0,0,},
        {1,1,1,1,1,1,1,1,1,1,1,},
        {1,1,1,1,1,1,1,1,1,1,1,},
    },
    {
        {1,1,1,1,1,1,1,1,1,0,0,},
        {1,1,1,1,1,1,1,1,1,1,0,},
        {1,1,0,0,0,0,0,0,1,1,0,},
        {1,1,0,0,0,0,0,0,1,1,0,},
        {1,1,0,0,0,0,0,0,1,1,0,},
        {1,1,1,1,1,1,1,1,1,1,0,},
        {1,1,1,1,1,1,1,1,1,0,0,},
        {1,1,0,0,0,0,0,1,1,0,0,},
        {1,1,0,0,0,0,0,0,1,1,0,},
        {1,1,0,0,0,0,0,0,1,1,0,},
        {1,1,0,0,0,0,0,0,0,1,1,},
        {1,1,0,0,0,0,0,0,0,1,1,},
    }
};

static const int LR_xy[2][2] = {
    { 70, 264 },
    { 232, 264 }
};


void LCD_FontRenderLR(lcd_t& lcd, uint8_t ch)
{
    uint8_t* f;
    if (ch >= 16)
        f = &lcd_font[ch - 16][0];
    else
        f = &lcd.LCD_CG[(ch & 7) * 8];
    int col;
    if (f[0] & 1)
    {
        col = lcd.color1;
    }
    else
    {
        col = lcd.color2;
    }
    for (int letter = 0; letter < 2; letter++)
    {
        for (int i = 0; i < 12; i++)
        {
            for (int j = 0; j < 11; j++)
            {
                if (LR[letter][i][j])
                    lcd.buffer[i+LR_xy[letter][0]][j+LR_xy[letter][1]] = col;
            }
        }
    }
}

void LCD_Update(lcd_t& lcd)
{
    if (!lcd.mcu->is_cm300 && !lcd.mcu->is_st && !lcd.mcu->is_scb55)
    {
        MCU_WorkThread_Lock(*lcd.mcu);

        uint8_t contrast = lcd.contrast;

        if (!lcd.enable && !lcd.mcu->is_jv880)
        {
            contrast = 1;
            memset(lcd.LCD_Data, ' ', sizeof(lcd.LCD_Data));
            for (size_t i = 0; i < lcd.height; i++) 
            {
                for (size_t j = 0; j < lcd.width; j++) 
                {
                    lcd.buffer[i][j] = (lcd.background[i][j] & 0xF0F000) >> 2;
                }
            }
        }
        else
        {
            if (lcd.mcu->is_jv880)
            {
                for (size_t i = 0; i < lcd.height; i++) {
                    for (size_t j = 0; j < lcd.width; j++) {
                        lcd.buffer[i][j] = 0xFF03be51;
                    }
                }
            }
            else
            {
                for (size_t i = 0; i < lcd.height; i++) {
                    for (size_t j = 0; j < lcd.width; j++) {
                        lcd.buffer[i][j] = lcd.background[i][j];
                    }
                }
            }

            uint32_t con = 0x11 * (contrast - 1);
            con = (con * con) >> 8;
            lcd.color2 = LCD_MixColor(lcd.buffer[0][0], 0xFF - (con / 4 + 4));
            lcd.color1 = LCD_MixColor(lcd.color2, 0x11 * (16 - (((contrast + 1) >> 1) + 4)));

            if (lcd.mcu->is_jv880)
            {
                for (int i = 0; i < 2; i++)
                {
                    for (int j = 0; j < 24; j++)
                    {
                        uint8_t ch = lcd.LCD_Data[i * 40 + j];
                        LCD_FontRenderStandard(lcd, 4 + i * 50, 4 + j * 34, ch);
                    }
                }
                
                // cursor
                int j = lcd.LCD_DD_RAM % 0x40;
                int i = lcd.LCD_DD_RAM / 0x40;
                if (i < 2 && j < 24 && lcd.LCD_C)
                    LCD_FontRenderStandard(lcd, 4 + i * 50, 4 + j * 34, '_', true);
            }
            else
            {
                for (int i = 0; i < 3; i++)
                {
                    uint8_t ch = lcd.LCD_Data[0 + i];
                    LCD_FontRenderStandard(lcd, 11, 34 + i * 35, ch);
                }
                for (int i = 0; i < 16; i++)
                {
                    uint8_t ch = lcd.LCD_Data[3 + i];
                    LCD_FontRenderStandard(lcd, 11, 153 + i * 35, ch);
                }
                for (int i = 0; i < 3; i++)
                {
                    uint8_t ch = lcd.LCD_Data[40 + i];
                    LCD_FontRenderStandard(lcd, 75, 34 + i * 35, ch);
                }
                for (int i = 0; i < 3; i++)
                {
                    uint8_t ch = lcd.LCD_Data[43 + i];
                    LCD_FontRenderStandard(lcd, 75, 153 + i * 35, ch);
                }
                for (int i = 0; i < 3; i++)
                {
                    uint8_t ch = lcd.LCD_Data[49 + i];
                    LCD_FontRenderStandard(lcd, 139, 34 + i * 35, ch);
                }
                for (int i = 0; i < 3; i++)
                {
                    uint8_t ch = lcd.LCD_Data[46 + i];
                    LCD_FontRenderStandard(lcd, 139, 153 + i * 35, ch);
                }
                for (int i = 0; i < 3; i++)
                {
                    uint8_t ch = lcd.LCD_Data[52 + i];
                    LCD_FontRenderStandard(lcd, 203, 34 + i * 35, ch);
                }
                for (int i = 0; i < 3; i++)
                {
                    uint8_t ch = lcd.LCD_Data[55 + i];
                    LCD_FontRenderStandard(lcd, 203, 153 + i * 35, ch);
                }

                LCD_FontRenderLR(lcd, lcd.LCD_Data[58]);

                for (int i = 0; i < 2; i++)
                {
                    for (int j = 0; j < 4; j++)
                    {
                        uint8_t ch = lcd.LCD_Data[20 + j + i * 40];
                        LCD_FontRenderLevel(lcd, 71 + i * 88, 293 + j * 130, ch, j == 3 ? 1 : 5);
                    }
                }
            }
        }

        MCU_WorkThread_Unlock(*lcd.mcu);

        SDL_Rect rect;
        rect.x = 0;
        rect.y = 0;
        rect.w = lcd.width;
        rect.h = lcd.height;
        SDL_UpdateTexture(lcd.texture, &rect, lcd.buffer, lcd_width_max * 4);

        if (lcd.mcu->romset == Romset::MK2 && lcd.background_enabled) {
            SDL_Rect srcrect, dstrect;
            srcrect.x = 0;
            srcrect.y = 0;
            srcrect.w = 2240;
            srcrect.h = 466;
            SDL_RenderCopy(lcd.renderer, lcd.background_image, &srcrect, NULL);
            if ((lcd.button_enable & 1) != 0 || (lcd.button_enable & 2) != 0) {
                srcrect.x = 0;
                srcrect.y = 466;
                srcrect.w = 52;
                srcrect.h = 52;
                dstrect.x = 754;
                dstrect.w = 26;
                dstrect.h = 26;
                if ((lcd.button_enable & 1) != 0) { // ALL
                    dstrect.y = 35;
                    SDL_RenderCopy(lcd.renderer, lcd.background_image, &srcrect, &dstrect);
                }
                if ((lcd.button_enable & 2) != 0) { // MUTE
                    dstrect.y = 82;
                    SDL_RenderCopy(lcd.renderer, lcd.background_image, &srcrect, &dstrect);
                }
            }
            if ((lcd.button_enable & 4) != 0) { // STANDBY
                srcrect.x = 0;
                srcrect.y = 518;
                srcrect.w = 20;
                srcrect.h = 20;
                dstrect.x = 118;
                dstrect.y = 42;
                dstrect.w = 10;
                dstrect.h = 10;
                SDL_RenderCopy(lcd.renderer, lcd.background_image, &srcrect, &dstrect);
            }
            { // Volume
                srcrect.x = 54;
                srcrect.y = 468;
                srcrect.w = 118;
                srcrect.h = 118;
                dstrect.x = 153;
                dstrect.y = 42;
                dstrect.w = 59;
                dstrect.h = 59;
                SDL_RenderCopyEx(lcd.renderer, lcd.background_image, &srcrect, &dstrect, (lcd.volume - 0.5f) * 300.0, NULL, SDL_FLIP_NONE);
            }
            srcrect.x = 0;
            srcrect.y = 0;
            srcrect.w = 740;
            srcrect.h = 268;
            dstrect.x = 283;
            dstrect.y = 49;
            dstrect.w = 370;
            dstrect.h = 134;
            SDL_RenderCopy(lcd.renderer, lcd.texture, &srcrect, &dstrect);
        } else {
            SDL_RenderCopy(lcd.renderer, lcd.texture, NULL, NULL);
        }
        SDL_RenderPresent(lcd.renderer);
    }
}

void LCD_HandleEvent(lcd_t& lcd, const SDL_Event& sdl_event)
{
    if (lcd.mcu->romset == Romset::MK2 && lcd.background_enabled) 
    {
        switch (sdl_event.type)
        {
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEBUTTONDOWN: {
            if (sdl_event.button.button == 1) {
                if (lcd.drag_volume_knob || (sdl_event.button.x >= 153 && sdl_event.button.x <= 212 && sdl_event.button.y >= 42 && sdl_event.button.y <= 101)) {
                    lcd.drag_volume_knob = (sdl_event.type == SDL_MOUSEBUTTONDOWN) || (lcd.drag_volume_knob && sdl_event.type != SDL_MOUSEBUTTONUP);
                }
            }
            int32_t x = sdl_event.button.x;
            int32_t y = sdl_event.button.y;
            int mask = 0;
            uint32_t button_pressed = lcd.mcu->button_pressed;
            for (int i = 0; i < 32; i++) {
                const SDL_Rect *rect = &lcd_button_regions_sc55[i];
                if (rect->x == 0 && rect->y == 0 && rect->w == 0 && rect->h == 0) continue;
                if (x >= rect->x && x <= rect->x + rect->w && y >= rect->y && y <= rect->y + rect->h) {
                    mask |= 1 << i;
                }
            }
            if (sdl_event.type == SDL_MOUSEBUTTONDOWN)
                button_pressed |= mask;
            else
                button_pressed &= ~mask;
            lcd.mcu->button_pressed = button_pressed;
            break;
        }
        case SDL_MOUSEMOTION:
            if (lcd.drag_volume_knob) {
                int32_t relval = abs(sdl_event.motion.xrel) > abs(sdl_event.motion.yrel) ? sdl_event.motion.xrel : (lcd.volume > 0.5f ? sdl_event.motion.yrel : -sdl_event.motion.yrel);
                if (relval > 50) { // Maximum ±10dB incremental
                    relval = 50;
                }
                if (relval < -50) {
                    relval = -50;
                }
                lcd.volume += relval / (lcd.volume > 0.775f ? 10000.0f : 400.0f); // Prevent someone make sound too loud (like me)
                if (lcd.volume > 1.0f) {
                    lcd.volume = 1.0f;
                }
                if (lcd.volume < 0.0f) {
                    lcd.volume = 0.0f;
                }
                if (lcd.volume != 0.0f) {
                    float vol = powf(10.0f, (-80.0f * (1.0f - lcd.volume)) / 20.0f); // or volume ^ 8 (0 < volume < 1)
                    MCU_SetVolume(*lcd.mcu, (uint16_t) (vol * 2 *UINT16_MAX));
                } else {
                    MCU_SetVolume(*lcd.mcu, 0);
                }
            }
            break;
        case SDL_MOUSEWHEEL:
            if (sdl_event.wheel.mouseX >= 153 && sdl_event.wheel.mouseX <= 212 && sdl_event.wheel.mouseY >= 42 && sdl_event.wheel.mouseY <= 101) {
                int32_t relval = sdl_event.wheel.y;
                if (relval > 10) { // Maximum ±2dB incremental
                    relval = 10;
                }
                if (relval < -10) {
                    relval = -10;
                }
                lcd.volume += relval / 400.0f;
                if (lcd.volume > 1.0f) {
                    lcd.volume = 1.0f;
                }
                if (lcd.volume < 0.0f) {
                    lcd.volume = 0.0f;
                }
                if (lcd.volume != 0.0f) {
                    float vol = powf(10.0f, (-80.0f * (1.0f - lcd.volume)) / 20.0f); // or volume ^ 8 (0 < volume < 1)
                    MCU_SetVolume(*lcd.mcu, (uint16_t) (vol * 2 * UINT16_MAX));
                } else {
                    MCU_SetVolume(*lcd.mcu, 0);
                }
            }
            break;

        default:
            break;
        }
    }

    switch (sdl_event.type)
    {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (sdl_event.key.windowID != SDL_GetWindowID(lcd.window))
            {
                return;
            }
            break;
        case SDL_WINDOWEVENT:
            if (sdl_event.window.windowID != SDL_GetWindowID(lcd.window))
            {
                return;
            }
            break;
        default:
            break;
    }

    if (sdl_event.type == SDL_KEYDOWN)
    {
        if (sdl_event.key.keysym.scancode == SDL_SCANCODE_COMMA)
            MCU_EncoderTrigger(*lcd.mcu, 0);
        if (sdl_event.key.keysym.scancode == SDL_SCANCODE_PERIOD)
            MCU_EncoderTrigger(*lcd.mcu, 1);
    }

    switch (sdl_event.type)
    {
        case SDL_WINDOWEVENT:
            if (sdl_event.window.event == SDL_WINDOWEVENT_CLOSE)
            {
                lcd.quit_requested = true;
            }
            break;

        case SDL_KEYDOWN:
        case SDL_KEYUP:
            {
                if (sdl_event.key.repeat)
                    break;

                int mask = 0;
                uint32_t button_pressed = lcd.mcu->button_pressed;

                auto button_map = lcd.mcu->is_jv880 ? button_map_jv880 : button_map_sc55;
                auto button_size = (lcd.mcu->is_jv880 ? sizeof(button_map_jv880) : sizeof(button_map_sc55)) / sizeof(button_map_sc55[0]);
                for (size_t i = 0; i < button_size; i++)
                {
                    if (button_map[i][0] == sdl_event.key.keysym.scancode)
                        mask |= (1 << button_map[i][1]);
                }

                if (sdl_event.type == SDL_KEYDOWN)
                    button_pressed |= mask;
                else
                    button_pressed &= ~mask;

                lcd.mcu->button_pressed = button_pressed;

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
