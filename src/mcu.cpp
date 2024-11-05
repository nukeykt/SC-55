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
#include "mcu.h"
#include "mcu_timer.h"
#include "mcu_opcodes.h"
#include "submcu.h"
#include "pcm.h"
#include "lcd.h"

void MCU_ErrorTrap(mcu_t& mcu)
{
    fprintf(stderr, "%.2x %.4x\n", mcu.cp, mcu.pc);
}

uint8_t RCU_Read(void)
{
    return 0;
}

enum {
    ANALOG_LEVEL_RCU_LOW = 0,
    ANALOG_LEVEL_RCU_HIGH = 0,
    ANALOG_LEVEL_SW_0 = 0,
    ANALOG_LEVEL_SW_1 = 0x155,
    ANALOG_LEVEL_SW_2 = 0x2aa,
    ANALOG_LEVEL_SW_3 = 0x3ff,
    ANALOG_LEVEL_BATTERY = 0x2a0,
};

uint16_t MCU_SC155Sliders(mcu_t& mcu, uint32_t index)
{
    (void)mcu;
    (void)index;
    // 0 - 1/9
    // 1 - 2/10
    // 2 - 3/11
    // 3 - 4/12
    // 4 - 5/13
    // 5 - 6/14
    // 6 - 7/15
    // 7 - 8/16
    // 8 - ALL
    return 0x0;
}

uint16_t MCU_AnalogReadPin(mcu_t& mcu, uint32_t pin)
{
    if (mcu.is_cm300)
        return 0;
    if (mcu.is_jv880)
    {
        if (pin == 1)
            return ANALOG_LEVEL_BATTERY;
        return 0x3ff;
    }
    if (0)
    {
READ_RCU:
        uint8_t rcu = RCU_Read();
        if (rcu & (1 << pin))
            return ANALOG_LEVEL_RCU_HIGH;
        else
            return ANALOG_LEVEL_RCU_LOW;
    }
    if (mcu.is_mk1)
    {
        if (mcu.is_sc155 && (mcu.dev_register[DEV_P9DR] & 1) != 0)
        {
            return MCU_SC155Sliders(mcu, pin);
        }
        if (pin == 7)
        {
            if (mcu.is_sc155 && (mcu.dev_register[DEV_P9DR] & 2) != 0)
                return MCU_SC155Sliders(mcu, 8);
            else
                return ANALOG_LEVEL_BATTERY;
        }
        else
            goto READ_RCU;
    }
    else
    {
        if (mcu.is_sc155 && (mcu.io_sd & 16) != 0)
        {
            return MCU_SC155Sliders(mcu, pin);
        }
        if (pin == 7)
        {
            if (mcu.is_mk1)
                return ANALOG_LEVEL_BATTERY;
            switch ((mcu.io_sd >> 2) & 3)
            {
            case 0: // Battery voltage
                return ANALOG_LEVEL_BATTERY;
            case 1: // NC
                if (mcu.is_sc155)
                    return MCU_SC155Sliders(mcu, 8);
                return 0;
            case 2: // SW
                switch (mcu.sw_pos)
                {
                case 0:
                default:
                    return ANALOG_LEVEL_SW_0;
                case 1:
                    return ANALOG_LEVEL_SW_1;
                case 2:
                    return ANALOG_LEVEL_SW_2;
                case 3:
                    return ANALOG_LEVEL_SW_3;
                }
            case 3: // RCU
                goto READ_RCU;
            }
        }
        else
            goto READ_RCU;
    }
    // TODO: really unreachable? maybe addressed upstream later?
    fprintf(stderr, "PANIC: reached end of MCU_AnalogReadPin\n");
    exit(1);
}

void MCU_AnalogSample(mcu_t& mcu, int channel)
{
    int value = MCU_AnalogReadPin(mcu, channel);
    int dest = (channel << 1) & 6;
    mcu.dev_register[DEV_ADDRAH + dest] = value >> 2;
    mcu.dev_register[DEV_ADDRAL + dest] = (value << 6) & 0xc0;
}

void MCU_DeviceWrite(mcu_t& mcu, uint32_t address, uint8_t data)
{
    address &= 0x7f;
    if (address >= 0x10 && address < 0x40)
    {
        TIMER_Write(*mcu.timer, address, data);
        return;
    }
    if (address >= 0x50 && address < 0x55)
    {
        TIMER2_Write(*mcu.timer, address, data);
        return;
    }
    switch (address)
    {
    case DEV_P1DDR: // P1DDR
        break;
    case DEV_P5DDR:
        break;
    case DEV_P6DDR:
        break;
    case DEV_P7DDR:
        break;
    case DEV_SCR:
        break;
    case DEV_WCR:
        break;
    case DEV_P9DDR:
        break;
    case DEV_RAME: // RAME
        break;
    case DEV_P1CR: // P1CR
        break;
    case DEV_DTEA:
        break;
    case DEV_DTEB:
        break;
    case DEV_DTEC:
        break;
    case DEV_DTED:
        break;
    case DEV_SMR:
        break;
    case DEV_BRR:
        break;
    case DEV_IPRA:
        break;
    case DEV_IPRB:
        break;
    case DEV_IPRC:
        break;
    case DEV_IPRD:
        break;
    case DEV_PWM1_DTR:
        break;
    case DEV_PWM1_TCR:
        break;
    case DEV_PWM2_DTR:
        break;
    case DEV_PWM2_TCR:
        break;
    case DEV_PWM3_DTR:
        break;
    case DEV_PWM3_TCR:
        break;
    case DEV_P7DR:
        break;
    case DEV_TMR_TCNT:
        break;
    case DEV_TMR_TCR:
        break;
    case DEV_TMR_TCSR:
        break;
    case DEV_TMR_TCORA:
        break;
    case DEV_TDR:
        break;
    case DEV_ADCSR:
    {
        mcu.dev_register[address] &= ~0x7f;
        mcu.dev_register[address] |= data & 0x7f;
        if ((data & 0x80) == 0 && mcu.adf_rd)
        {
            mcu.dev_register[address] &= ~0x80;
            MCU_Interrupt_SetRequest(mcu, INTERRUPT_SOURCE_ANALOG, 0);
        }
        if ((data & 0x40) == 0)
            MCU_Interrupt_SetRequest(mcu, INTERRUPT_SOURCE_ANALOG, 0);
        return;
    }
    case DEV_SSR:
    {
        if ((data & 0x80) == 0 && (mcu.ssr_rd & 0x80) != 0)
        {
            mcu.dev_register[address] &= ~0x80;
            mcu.uart_tx_delay = mcu.cycles + 3000;
            MCU_Interrupt_SetRequest(mcu, INTERRUPT_SOURCE_UART_TX, 0);
        }
        if ((data & 0x40) == 0 && (mcu.ssr_rd & 0x40) != 0)
        {
            mcu.uart_rx_delay = mcu.cycles + 3000;
            mcu.dev_register[address] &= ~0x40;
            MCU_Interrupt_SetRequest(mcu, INTERRUPT_SOURCE_UART_RX, 0);
        }
        if ((data & 0x20) == 0 && (mcu.ssr_rd & 0x20) != 0)
        {
            mcu.dev_register[address] &= ~0x20;
        }
        if ((data & 0x10) == 0 && (mcu.ssr_rd & 0x10) != 0)
        {
            mcu.dev_register[address] &= ~0x10;
        }
        break;
    }
    default:
        address += 0;
        break;
    }
    mcu.dev_register[address] = data;
}

uint8_t MCU_DeviceRead(mcu_t& mcu, uint32_t address)
{
    address &= 0x7f;
    if (address >= 0x10 && address < 0x40)
    {
        return TIMER_Read(*mcu.timer, address);
    }
    if (address >= 0x50 && address < 0x55)
    {
        return TIMER_Read2(*mcu.timer, address);
    }
    switch (address)
    {
    case DEV_ADDRAH:
    case DEV_ADDRAL:
    case DEV_ADDRBH:
    case DEV_ADDRBL:
    case DEV_ADDRCH:
    case DEV_ADDRCL:
    case DEV_ADDRDH:
    case DEV_ADDRDL:
        return mcu.dev_register[address];
    case DEV_ADCSR:
        mcu.adf_rd = (mcu.dev_register[address] & 0x80) != 0;
        return mcu.dev_register[address];
    case DEV_SSR:
        mcu.ssr_rd = mcu.dev_register[address];
        return mcu.dev_register[address];
    case DEV_RDR:
        return mcu.uart_rx_byte;
    case 0x00:
        return 0xff;
    case DEV_P7DR:
    {
        if (!mcu.is_jv880) return 0xff;

        uint8_t data = 0xff;
        uint32_t button_pressed = mcu.button_pressed;

        if (mcu.io_sd == 0b11111011)
            data &= ((button_pressed >> 0) & 0b11111) ^ 0xFF;
        if (mcu.io_sd == 0b11110111)
            data &= ((button_pressed >> 5) & 0b11111) ^ 0xFF;
        if (mcu.io_sd == 0b11101111)
            data &= ((button_pressed >> 10) & 0b1111) ^ 0xFF;

        data |= 0b10000000;
        return data;
    }
    case DEV_P9DR:
    {
        int cfg = 0;
        if (!mcu.is_mk1)
            cfg = mcu.is_sc155 ? 0 : 2; // bit 1: 0 - SC-155mk2 (???), 1 - SC-55mk2

        int dir = mcu.dev_register[DEV_P9DDR];

        int val = cfg & (dir ^ 0xff);
        val |= mcu.dev_register[DEV_P9DR] & dir;
        return val;
    }
    case DEV_SCR:
    case DEV_TDR:
    case DEV_SMR:
        return mcu.dev_register[address];
    case DEV_IPRC:
    case DEV_IPRD:
    case DEV_DTEC:
    case DEV_DTED:
    case DEV_FRT2_TCSR:
    case DEV_FRT1_TCSR:
    case DEV_FRT1_TCR:
    case DEV_FRT1_FRCH:
    case DEV_FRT1_FRCL:
    case DEV_FRT3_TCSR:
    case DEV_FRT3_OCRAH:
    case DEV_FRT3_OCRAL:
        return mcu.dev_register[address];
    }
    return mcu.dev_register[address];
}

void MCU_DeviceReset(mcu_t& mcu)
{
    // mcu.dev_register[0x00] = 0x03;
    // mcu.dev_register[0x7c] = 0x87;
    mcu.dev_register[DEV_RAME] = 0x80;
    mcu.dev_register[DEV_SSR] = 0x80;
}

void MCU_UpdateAnalog(mcu_t& mcu, uint64_t cycles)
{
    int ctrl = mcu.dev_register[DEV_ADCSR];
    int isscan = (ctrl & 16) != 0;

    if (ctrl & 0x20)
    {
        if (mcu.analog_end_time == 0)
            mcu.analog_end_time = cycles + 200;
        else if (mcu.analog_end_time < cycles)
        {
            if (isscan)
            {
                int base = ctrl & 4;
                for (int i = 0; i <= (ctrl & 3); i++)
                    MCU_AnalogSample(mcu, base + i);
                mcu.analog_end_time = cycles + 200;
            }
            else
            {
                MCU_AnalogSample(mcu, ctrl & 7);
                mcu.dev_register[DEV_ADCSR] &= ~0x20;
                mcu.analog_end_time = 0;
            }
            mcu.dev_register[DEV_ADCSR] |= 0x80;
            if (ctrl & 0x40)
                MCU_Interrupt_SetRequest(mcu, INTERRUPT_SOURCE_ANALOG, 1);
        }
    }
    else
        mcu.analog_end_time = 0;
}

uint8_t MCU_Read(mcu_t& mcu, uint32_t address)
{
    uint32_t address_rom = address & 0x3ffff;
    if (address & 0x80000 && !mcu.is_jv880)
        address_rom |= 0x40000;
    uint8_t page = (address >> 16) & 0xf;
    address &= 0xffff;
    uint8_t ret = 0xff;
    switch (page)
    {
    case 0:
        if (!(address & 0x8000))
            ret = mcu.rom1[address & 0x7fff];
        else
        {
            if (!mcu.is_mk1)
            {
                uint16_t base = mcu.is_jv880 ? 0xf000 : 0xe000;
                if (address >= base && address < (base | 0x400))
                {
                    ret = PCM_Read(*mcu.pcm, address & 0x3f);
                }
                else if (!mcu.is_scb55 && address >= 0xec00 && address < 0xf000)
                {
                    ret = SM_SysRead(*mcu.sm, address & 0xff);
                }
                else if (address >= 0xff80)
                {
                    ret = MCU_DeviceRead(mcu, address & 0x7f);
                }
                else if (address >= 0xfb80 && address < 0xff80
                    && (mcu.dev_register[DEV_RAME] & 0x80) != 0)
                    ret = mcu.ram[(address - 0xfb80) & 0x3ff];
                else if (address >= 0x8000 && address < 0xe000)
                {
                    ret = mcu.sram[address & 0x7fff];
                }
                else if (address == (base | 0x402))
                {
                    ret = mcu.ga_int_trigger;
                    mcu.ga_int_trigger = 0;
                    MCU_Interrupt_SetRequest(mcu, mcu.is_jv880 ? INTERRUPT_SOURCE_IRQ0 : INTERRUPT_SOURCE_IRQ1, 0);
                }
                else
                {
                    fprintf(stderr, "Unknown read %x\n", address);
                    ret = 0xff;
                }
                //
                // e402:2-0 irq source
                //
            }
            else
            {
                if (address >= 0xe000 && address < 0xe040)
                {
                    ret = PCM_Read(*mcu.pcm, address & 0x3f);
                }
                else if (address >= 0xff80)
                {
                    ret = MCU_DeviceRead(mcu, address & 0x7f);
                }
                else if (address >= 0xfb80 && address < 0xff80
                    && (mcu.dev_register[DEV_RAME] & 0x80) != 0)
                {
                    ret = mcu.ram[(address - 0xfb80) & 0x3ff];
                }
                else if (address >= 0x8000 && address < 0xe000)
                {
                    ret = mcu.sram[address & 0x7fff];
                }
                else if (address >= 0xf000 && address < 0xf100)
                {
                    mcu.io_sd = address & 0xff;

                    if (mcu.is_cm300)
                        return 0xff;

                    LCD_Enable(*mcu.lcd, (mcu.io_sd & 8) != 0);

                    uint8_t data = 0xff;
                    uint32_t button_pressed = mcu.button_pressed;

                    if ((mcu.io_sd & 1) == 0)
                        data &= ((button_pressed >> 0) & 255) ^ 255;
                    if ((mcu.io_sd & 2) == 0)
                        data &= ((button_pressed >> 8) & 255) ^ 255;
                    if ((mcu.io_sd & 4) == 0)
                        data &= ((button_pressed >> 16) & 255) ^ 255;
                    if ((mcu.io_sd & 8) == 0)
                        data &= ((button_pressed >> 24) & 255) ^ 255;
                    return data;
                }
                else if (address == 0xf106)
                {
                    ret = mcu.ga_int_trigger;
                    mcu.ga_int_trigger = 0;
                    MCU_Interrupt_SetRequest(mcu, INTERRUPT_SOURCE_IRQ1, 0);
                }
                else
                {
                    fprintf(stderr, "Unknown read %x\n", address);
                    ret = 0xff;
                }
                //
                // f106:2-0 irq source
                //
            }
        }
        break;
#if 0
    case 3:
        ret = rom2[address | 0x30000];
        break;
    case 4:
        ret = rom2[address];
        break;
    case 10:
        ret = rom2[address | 0x60000]; // FIXME
        break;
    case 1:
        ret = rom2[address | 0x10000];
        break;
#endif
    case 1:
        ret = mcu.rom2[address_rom & mcu.rom2_mask];
        break;
    case 2:
        ret = mcu.rom2[address_rom & mcu.rom2_mask];
        break;
    case 3:
        ret = mcu.rom2[address_rom & mcu.rom2_mask];
        break;
    case 4:
        ret = mcu.rom2[address_rom & mcu.rom2_mask];
        break;
    case 8:
        if (!mcu.is_jv880)
            ret = mcu.rom2[address_rom & mcu.rom2_mask];
        else
            ret = 0xff;
        break;
    case 9:
        if (!mcu.is_jv880)
            ret = mcu.rom2[address_rom & mcu.rom2_mask];
        else
            ret = 0xff;
        break;
    case 14:
    case 15:
        if (!mcu.is_jv880)
            ret = mcu.rom2[address_rom & mcu.rom2_mask];
        else
            ret = mcu.cardram[address & 0x7fff]; // FIXME
        break;
    case 10:
    case 11:
        if (!mcu.is_mk1)
            ret = mcu.sram[address & 0x7fff]; // FIXME
        else
            ret = 0xff;
        break;
    case 12:
    case 13:
        if (mcu.is_jv880)
            ret = mcu.nvram[address & 0x7fff]; // FIXME
        else
            ret = 0xff;
        break;
    case 5:
        if (mcu.is_mk1)
            ret = mcu.sram[address & 0x7fff]; // FIXME
        else
            ret = 0xff;
        break;
    default:
        ret = 0x00;
        break;
    }
    return ret;
}

uint16_t MCU_Read16(mcu_t& mcu, uint32_t address)
{
    address &= ~1;
    uint8_t b0, b1;
    b0 = MCU_Read(mcu, address);
    b1 = MCU_Read(mcu, address+1);
    return (b0 << 8) + b1;
}

uint32_t MCU_Read32(mcu_t& mcu, uint32_t address)
{
    address &= ~3;
    uint8_t b0, b1, b2, b3;
    b0 = MCU_Read(mcu, address);
    b1 = MCU_Read(mcu, address+1);
    b2 = MCU_Read(mcu, address+2);
    b3 = MCU_Read(mcu, address+3);
    return (b0 << 24) + (b1 << 16) + (b2 << 8) + b3;
}

void MCU_Write(mcu_t& mcu, uint32_t address, uint8_t value)
{
    uint8_t page = (address >> 16) & 0xf;
    address &= 0xffff;
    if (page == 0)
    {
        if (address & 0x8000)
        {
            if (!mcu.is_mk1)
            {
                uint16_t base = mcu.is_jv880 ? 0xf000 : 0xe000;
                if (address >= (base | 0x400) && address < (base | 0x800))
                {
                    if (address == (base | 0x404) || address == (base | 0x405))
                        LCD_Write(*mcu.lcd, address & 1, value);
                    else if (address == (base | 0x401))
                    {
                        mcu.io_sd = value;
                        LCD_Enable(*mcu.lcd, (value & 1) == 0);
                    }
                    else if (address == (base | 0x402))
                        mcu.ga_int_enable = (value << 1);
                    else
                        fprintf(stderr, "Unknown write %x %x\n", address, value);
                    //
                    // e400: always 4?
                    // e401: SC0-6?
                    // e402: enable/disable IRQ?
                    // e403: always 1?
                    // e404: LCD
                    // e405: LCD
                    // e406: 0 or 40
                    // e407: 0, e406 continuation?
                    //
                }
                else if (address >= (base | 0x000) && address < (base | 0x400))
                {
                    PCM_Write(*mcu.pcm, address & 0x3f, value);
                }
                else if (!mcu.is_scb55 && address >= 0xec00 && address < 0xf000)
                {
                    SM_SysWrite(*mcu.sm, address & 0xff, value);
                }
                else if (address >= 0xff80)
                {
                    MCU_DeviceWrite(mcu, address & 0x7f, value);
                }
                else if (address >= 0xfb80 && address < 0xff80
                    && (mcu.dev_register[DEV_RAME] & 0x80) != 0)
                {
                    mcu.ram[(address - 0xfb80) & 0x3ff] = value;
                }
                else if (address >= 0x8000 && address < 0xe000)
                {
                    mcu.sram[address & 0x7fff] = value;
                }
                else
                {
                    fprintf(stderr, "Unknown write %x %x\n", address, value);
                }
            }
            else
            {
                if (address >= 0xe000 && address < 0xe040)
                {
                    PCM_Write(*mcu.pcm, address & 0x3f, value);
                }
                else if (address >= 0xff80)
                {
                    MCU_DeviceWrite(mcu, address & 0x7f, value);
                }
                else if (address >= 0xfb80 && address < 0xff80
                    && (mcu.dev_register[DEV_RAME] & 0x80) != 0)
                {
                    mcu.ram[(address - 0xfb80) & 0x3ff] = value;
                }
                else if (address >= 0x8000 && address < 0xe000)
                {
                    mcu.sram[address & 0x7fff] = value;
                }
                else if (address >= 0xf000 && address < 0xf100)
                {
                    mcu.io_sd = address & 0xff;
                    LCD_Enable(*mcu.lcd, (mcu.io_sd & 8) != 0);
                }
                else if (address == 0xf105)
                {
                    LCD_Write(*mcu.lcd, 0, value);
                    mcu.ga_lcd_counter = 500;
                }
                else if (address == 0xf104)
                {
                    LCD_Write(*mcu.lcd, 1, value);
                    mcu.ga_lcd_counter = 500;
                }
                else if (address == 0xf107)
                {
                    mcu.io_sd = value;
                }
                else
                {
                    fprintf(stderr, "Unknown write %x %x\n", address, value);
                }
            }
        }
        else if (mcu.is_jv880 && address >= 0x6196 && address <= 0x6199)
        {
            // nop: the jv880 rom writes into the rom at 002E77-002E7D
        }
        else
        {
            fprintf(stderr, "Unknown write %x %x\n", address, value);
        }
    }
    else if (page == 5 && mcu.is_mk1)
    {
        mcu.sram[address & 0x7fff] = value; // FIXME
    }
    else if (page == 10 && !mcu.is_mk1)
    {
        mcu.sram[address & 0x7fff] = value; // FIXME
    }
    else if (page == 12 && mcu.is_jv880)
    {
        mcu.nvram[address & 0x7fff] = value; // FIXME
    }
    else if (page == 14 && mcu.is_jv880)
    {
        mcu.cardram[address & 0x7fff] = value; // FIXME
    }
    else
    {
        fprintf(stderr, "Unknown write %x %x\n", (page << 16) | address, value);
    }
}

void MCU_Write16(mcu_t& mcu, uint32_t address, uint16_t value)
{
    address &= ~1;
    MCU_Write(mcu, address, value >> 8);
    MCU_Write(mcu, address + 1, value & 0xff);
}

void MCU_ReadInstruction(mcu_t& mcu)
{
    uint8_t operand = MCU_ReadCodeAdvance(mcu);

    MCU_Operand_Table[operand](mcu, operand);

    if (mcu.sr & STATUS_T)
    {
        MCU_Interrupt_Exception(mcu, EXCEPTION_SOURCE_TRACE);
    }
}

void MCU_DefaultSampleCallback(void* userdata, const AudioFrame<int32_t>& frame)
{
    (void)userdata;
    (void)frame;
}

bool MCU_Init(mcu_t& mcu, submcu_t& sm, pcm_t& pcm, mcu_timer_t& timer, lcd_t& lcd)
{
    mcu.sm = &sm;
    mcu.pcm = &pcm;
    mcu.timer = &timer;
    mcu.lcd = &lcd;
    return true;
}

void MCU_Deinit(mcu_t& mcu)
{
    (void)mcu;
}

void MCU_Reset(mcu_t& mcu)
{
    mcu.r[0] = 0;
    mcu.r[1] = 0;
    mcu.r[2] = 0;
    mcu.r[3] = 0;
    mcu.r[4] = 0;
    mcu.r[5] = 0;
    mcu.r[6] = 0;
    mcu.r[7] = 0;

    mcu.pc = 0;

    mcu.sr = 0x700;

    mcu.cp = 0;
    mcu.dp = 0;
    mcu.ep = 0;
    mcu.tp = 0;
    mcu.br = 0;

    uint32_t reset_address = MCU_GetVectorAddress(mcu, VECTOR_RESET);
    mcu.cp = (reset_address >> 16) & 0xff;
    mcu.pc = reset_address & 0xffff;

    mcu.exception_pending = -1;

    MCU_DeviceReset(mcu);

    if (mcu.is_mk1)
    {
        mcu.ga_int_enable = 255;
    }
}

void MCU_PostUART(mcu_t& mcu, uint8_t data)
{
    mcu.uart_buffer[mcu.uart_write_ptr] = data;
    mcu.uart_write_ptr = (mcu.uart_write_ptr + 1) % uart_buffer_size;
}

void MCU_UpdateUART_RX(mcu_t& mcu)
{
    if ((mcu.dev_register[DEV_SCR] & 16) == 0) // RX disabled
        return;
    if (mcu.uart_write_ptr == mcu.uart_read_ptr) // no byte
        return;

    if (mcu.dev_register[DEV_SSR] & 0x40)
        return;

    if (mcu.cycles < mcu.uart_rx_delay)
        return;

    mcu.uart_rx_byte = mcu.uart_buffer[mcu.uart_read_ptr];
    mcu.uart_read_ptr = (mcu.uart_read_ptr + 1) % uart_buffer_size;
    mcu.dev_register[DEV_SSR] |= 0x40;
    MCU_Interrupt_SetRequest(mcu, INTERRUPT_SOURCE_UART_RX, (mcu.dev_register[DEV_SCR] & 0x40) != 0);
}

// dummy TX
void MCU_UpdateUART_TX(mcu_t& mcu)
{
    if ((mcu.dev_register[DEV_SCR] & 32) == 0) // TX disabled
        return;

    if (mcu.dev_register[DEV_SSR] & 0x80)
        return;

    if (mcu.cycles < mcu.uart_tx_delay)
        return;

    mcu.dev_register[DEV_SSR] |= 0x80;
    MCU_Interrupt_SetRequest(mcu, INTERRUPT_SOURCE_UART_TX, (mcu.dev_register[DEV_SCR] & 0x80) != 0);

    // fprintf(stderr, "tx:%x\n", mcu.dev_register[DEV_TDR]);
}

void MCU_WorkThread_Lock(mcu_t& mcu)
{
    mcu.work_thread_lock.lock();
}

void MCU_WorkThread_Unlock(mcu_t& mcu)
{
    mcu.work_thread_lock.unlock();
}

void MCU_Step(mcu_t& mcu)
{
    if (!mcu.ex_ignore)
        MCU_Interrupt_Handle(mcu);
    else
        mcu.ex_ignore = 0;

    if (!mcu.sleep)
        MCU_ReadInstruction(mcu);

    mcu.cycles += 12; // FIXME: assume 12 cycles per instruction

    // if (mcu.cycles % 24000000 == 0)
    //     fprintf(stderr, "seconds: %i\n", (int)(mcu.cycles / 24000000));

    PCM_Update(*mcu.pcm, mcu.cycles);

    TIMER_Clock(*mcu.timer, mcu.cycles);

    if (!mcu.is_mk1 && !mcu.is_jv880 && !mcu.is_scb55)
        SM_Update(*mcu.sm, mcu.cycles);
    else
    {
        MCU_UpdateUART_RX(mcu);
        MCU_UpdateUART_TX(mcu);
    }

    MCU_UpdateAnalog(mcu, mcu.cycles);

    if (mcu.is_mk1)
    {
        if (mcu.ga_lcd_counter)
        {
            mcu.ga_lcd_counter--;
            if (mcu.ga_lcd_counter == 0)
            {
                MCU_GA_SetGAInt(mcu, 1, 0);
                MCU_GA_SetGAInt(mcu, 1, 1);
            }
        }
    }
}

void MCU_PatchROM(mcu_t& mcu)
{
    (void)mcu;
    //rom2[0x1333] = 0x11;
    //rom2[0x1334] = 0x19;
    //rom1[0x622d] = 0x19;
}

uint8_t MCU_ReadP0(mcu_t& mcu)
{
    (void)mcu;
    return 0xff;
}

uint8_t MCU_ReadP1(mcu_t& mcu)
{
    uint8_t data = 0xff;
    uint32_t button_pressed = mcu.button_pressed;

    if ((mcu.p0_data & 1) == 0)
        data &= ((button_pressed >> 0) & 255) ^ 255;
    if ((mcu.p0_data & 2) == 0)
        data &= ((button_pressed >> 8) & 255) ^ 255;
    if ((mcu.p0_data & 4) == 0)
        data &= ((button_pressed >> 16) & 255) ^ 255;
    if ((mcu.p0_data & 8) == 0)
        data &= ((button_pressed >> 24) & 255) ^ 255;

    return data;
}

void MCU_WriteP0(mcu_t& mcu, uint8_t data)
{
    mcu.p0_data = data;
}

void MCU_WriteP1(mcu_t& mcu, uint8_t data)
{
    mcu.p1_data = data;
}

void MCU_PostSample(mcu_t& mcu, const AudioFrame<int32_t>& frame)
{
    mcu.sample_callback(mcu.callback_userdata, frame);
}

void MCU_GA_SetGAInt(mcu_t& mcu, int line, int value)
{
    // guesswork
    if (value && !mcu.ga_int[line] && (mcu.ga_int_enable & (1 << line)) != 0)
        mcu.ga_int_trigger = line;
    mcu.ga_int[line] = value;

    if (mcu.is_jv880)
        MCU_Interrupt_SetRequest(mcu, INTERRUPT_SOURCE_IRQ0, mcu.ga_int_trigger != 0);
    else
        MCU_Interrupt_SetRequest(mcu, INTERRUPT_SOURCE_IRQ1, mcu.ga_int_trigger != 0);
}

void MCU_EncoderTrigger(mcu_t& mcu, int dir)
{
    if (!mcu.is_jv880) return;
    MCU_GA_SetGAInt(mcu, dir == 0 ? 3 : 4, 0);
    MCU_GA_SetGAInt(mcu, dir == 0 ? 3 : 4, 1);
}

