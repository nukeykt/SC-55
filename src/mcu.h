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
#include <atomic>
#include <thread>
#include <mutex>
#include "mcu_interrupt.h"
#include "cast.h"
#include "audio.h"

struct submcu_t;
struct pcm_t;
struct mcu_timer_t;
struct lcd_t;

enum {
    DEV_P1DDR = 0x00,
    DEV_P5DDR = 0x08,
    DEV_P6DDR = 0x09,
    DEV_P7DDR = 0x0c,
    DEV_P7DR = 0x0e,
    DEV_FRT1_TCR = 0x10,
    DEV_FRT1_TCSR = 0x11,
    DEV_FRT1_FRCH = 0x12,
    DEV_FRT1_FRCL = 0x13,
    DEV_FRT1_OCRAH = 0x14,
    DEV_FRT1_OCRAL = 0x15,
    DEV_FRT2_TCR = 0x20,
    DEV_FRT2_TCSR = 0x21,
    DEV_FRT2_FRCH = 0x22,
    DEV_FRT2_FRCL = 0x23,
    DEV_FRT2_OCRAH = 0x24,
    DEV_FRT2_OCRAL = 0x25,
    DEV_FRT3_TCR = 0x30,
    DEV_FRT3_TCSR = 0x31,
    DEV_FRT3_FRCH = 0x32,
    DEV_FRT3_FRCL = 0x33,
    DEV_FRT3_OCRAH = 0x34,
    DEV_FRT3_OCRAL = 0x35,
    DEV_PWM1_TCR = 0x40,
    DEV_PWM1_DTR = 0x41,
    DEV_PWM2_TCR = 0x44,
    DEV_PWM2_DTR = 0x45,
    DEV_PWM3_TCR = 0x48,
    DEV_PWM3_DTR = 0x49,
    DEV_TMR_TCR = 0x50,
    DEV_TMR_TCSR = 0x51,
    DEV_TMR_TCORA = 0x52,
    DEV_TMR_TCORB = 0x53,
    DEV_TMR_TCNT = 0x54,
    DEV_SMR = 0x58,
    DEV_BRR = 0x59,
    DEV_SCR = 0x5a,
    DEV_TDR = 0x5b,
    DEV_SSR = 0x5c,
    DEV_RDR = 0x5d,
    DEV_ADDRAH = 0x60,
    DEV_ADDRAL = 0x61,
    DEV_ADDRBH = 0x62,
    DEV_ADDRBL = 0x63,
    DEV_ADDRCH = 0x64,
    DEV_ADDRCL = 0x65,
    DEV_ADDRDH = 0x66,
    DEV_ADDRDL = 0x67,
    DEV_ADCSR = 0x68,
    DEV_IPRA = 0x70,
    DEV_IPRB = 0x71,
    DEV_IPRC = 0x72,
    DEV_IPRD = 0x73,
    DEV_DTEA = 0x74,
    DEV_DTEB = 0x75,
    DEV_DTEC = 0x76,
    DEV_DTED = 0x77,
    DEV_WCR = 0x78,
    DEV_RAME = 0x79,
    DEV_P1CR = 0x7c,
    DEV_P9DDR = 0x7e,
    DEV_P9DR = 0x7f,
};

const uint16_t sr_mask = 0x870f;
enum {
    STATUS_T = 0x8000,
    STATUS_N = 0x08,
    STATUS_Z = 0x04,
    STATUS_V = 0x02,
    STATUS_C = 0x01,
    STATUS_INT_MASK = 0x700
};

enum {
    VECTOR_RESET = 0,
    VECTOR_RESERVED1, // UNUSED
    VECTOR_INVALID_INSTRUCTION,
    VECTOR_DIVZERO,
    VECTOR_TRAP,
    VECTOR_RESERVED2, // UNUSED
    VECTOR_RESERVED3, // UNUSED
    VECTOR_RESERVED4, // UNUSED
    VECTOR_ADDRESS_ERROR,
    VECTOR_TRACE,
    VECTOR_RESERVED5, // UNUSED
    VECTOR_NMI,
    VECTOR_RESERVED6, // UNUSED
    VECTOR_RESERVED7, // UNUSED
    VECTOR_RESERVED8, // UNUSED
    VECTOR_RESERVED9, // UNUSED
    VECTOR_TRAPA_0,
    VECTOR_TRAPA_1,
    VECTOR_TRAPA_2,
    VECTOR_TRAPA_3,
    VECTOR_TRAPA_4,
    VECTOR_TRAPA_5,
    VECTOR_TRAPA_6,
    VECTOR_TRAPA_7,
    VECTOR_TRAPA_8,
    VECTOR_TRAPA_9,
    VECTOR_TRAPA_A,
    VECTOR_TRAPA_B,
    VECTOR_TRAPA_C,
    VECTOR_TRAPA_D,
    VECTOR_TRAPA_E,
    VECTOR_TRAPA_F,
    VECTOR_IRQ0,
    VECTOR_IRQ1,
    VECTOR_INTERNAL_INTERRUPT_88, // UNUSED
    VECTOR_INTERNAL_INTERRUPT_8C, // UNUSED
    VECTOR_INTERNAL_INTERRUPT_90, // FRT1 ICI
    VECTOR_INTERNAL_INTERRUPT_94, // FRT1 OCIA
    VECTOR_INTERNAL_INTERRUPT_98, // FRT1 OCIB
    VECTOR_INTERNAL_INTERRUPT_9C, // FRT1 FOVI
    VECTOR_INTERNAL_INTERRUPT_A0, // FRT2 ICI
    VECTOR_INTERNAL_INTERRUPT_A4, // FRT2 OCIA
    VECTOR_INTERNAL_INTERRUPT_A8, // FRT2 OCIB
    VECTOR_INTERNAL_INTERRUPT_AC, // FRT2 FOVI
    VECTOR_INTERNAL_INTERRUPT_B0, // FRT3 ICI
    VECTOR_INTERNAL_INTERRUPT_B4, // FRT3 OCIA
    VECTOR_INTERNAL_INTERRUPT_B8, // FRT3 OCIB
    VECTOR_INTERNAL_INTERRUPT_BC, // FRT3 FOVI
    VECTOR_INTERNAL_INTERRUPT_C0, // CMIA
    VECTOR_INTERNAL_INTERRUPT_C4, // CMIB
    VECTOR_INTERNAL_INTERRUPT_C8, // OVI
    VECTOR_INTERNAL_INTERRUPT_CC, // UNUSED
    VECTOR_INTERNAL_INTERRUPT_D0, // ERI
    VECTOR_INTERNAL_INTERRUPT_D4, // RXI
    VECTOR_INTERNAL_INTERRUPT_D8, // TXI
    VECTOR_INTERNAL_INTERRUPT_DC, // UNUSED
    VECTOR_INTERNAL_INTERRUPT_E0, // ADI
};

static const int ROM1_SIZE = 0x8000;
static const int ROM2_SIZE = 0x80000;
static const int RAM_SIZE = 0x400;
static const int SRAM_SIZE = 0x8000;
static const int NVRAM_SIZE = 0x8000; // JV880 only
static const int CARDRAM_SIZE = 0x8000; // JV880 only
static const int ROMSM_SIZE = 0x1000;

static const uint32_t uart_buffer_size = 8192;

enum class Romset {
    MK2,
    ST,
    MK1,
    CM300,
    JV880,
    SCB55,
    RLP3237,
    SC155,
    SC155MK2,
};

constexpr size_t ROMSET_COUNT = 9;

typedef void(*mcu_sample_callback)(void* userdata, const AudioFrame<int32_t>& frame);

void MCU_DefaultSampleCallback(void* userdata, const AudioFrame<int32_t>& frame);

struct mcu_t {
    uint16_t r[8]{};
    uint16_t pc = 0;
    uint16_t sr = 0;
    uint8_t cp = 0, dp = 0, ep = 0, tp = 0, br = 0;
    uint8_t sleep = 0;
    uint8_t ex_ignore = 0;
    int32_t exception_pending = 0;
    uint8_t interrupt_pending[INTERRUPT_SOURCE_MAX]{};
    uint8_t trapa_pending[16]{};
    uint64_t cycles = 0;

    uint8_t rom1[ROM1_SIZE]{};
    uint8_t rom2[ROM2_SIZE]{};
    uint8_t ram[RAM_SIZE]{};
    uint8_t sram[SRAM_SIZE]{};
    uint8_t nvram[NVRAM_SIZE]{};
    uint8_t cardram[CARDRAM_SIZE]{};

    uint8_t dev_register[0x80]{};

    uint16_t ad_val[4]{};
    uint8_t ad_nibble = 0;
    uint8_t sw_pos = 3;
    uint8_t io_sd = 0;

    submcu_t* sm = nullptr;
    pcm_t* pcm = nullptr;
    mcu_timer_t* timer = nullptr;
    lcd_t* lcd = nullptr;

    uint32_t uart_write_ptr = 0;
    uint32_t uart_read_ptr = 0;
    uint8_t uart_buffer[uart_buffer_size]{};

    uint8_t uart_rx_byte = 0;
    uint64_t uart_rx_delay = 0;
    uint64_t uart_tx_delay = 0;

    Romset romset = Romset::MK2;

    int is_mk1 = 0; // 0 - SC-55mkII, SC-55ST. 1 - SC-55, CM-300/SCC-1
    int is_cm300 = 0; // 0 - SC-55, 1 - CM-300/SCC-1
    int is_st = 0; // 0 - SC-55mk2, 1 - SC-55ST
    int is_jv880 = 0; // 0 - SC-55, 1 - JV880
    int is_scb55 = 0; // 0 - sub mcu (e.g SC-55mk2), 1 - no sub mcu (e.g SCB-55)
    int is_sc155 = 0; // 0 - SC-55(MK2), 1 - SC-155(MK2)

    int rom2_mask = ROM2_SIZE - 1;

    int ga_int[8]{};
    int ga_int_enable = 0;
    int ga_int_trigger = 0;
    int ga_lcd_counter = 0;

    std::atomic<uint32_t> button_pressed;

    uint8_t p0_data = 0;
    uint8_t p1_data = 0;

    int adf_rd = 0;

    uint64_t analog_end_time = 0;

    int ssr_rd = 0;

    uint32_t operand_type = 0;
    uint16_t operand_ea = 0;
    uint8_t operand_ep = 0;
    uint8_t operand_size = 0;
    uint8_t operand_reg = 0;
    uint8_t operand_status = 0;
    uint16_t operand_data = 0;
    uint8_t opcode_extended = 0;

    void* callback_userdata = nullptr;
    mcu_sample_callback sample_callback = MCU_DefaultSampleCallback;

    std::mutex work_thread_lock;
};

bool MCU_Init(mcu_t& mcu, submcu_t& sm, pcm_t& pcm, mcu_timer_t& timer, lcd_t& lcd);
void MCU_Reset(mcu_t& mcu);
void MCU_PatchROM(mcu_t& mcu);
void MCU_Step(mcu_t& mcu);

void MCU_ErrorTrap(mcu_t& mcu);

uint8_t MCU_Read(mcu_t& mcu, uint32_t address);
uint16_t MCU_Read16(mcu_t& mcu, uint32_t address);
uint32_t MCU_Read32(mcu_t& mcu, uint32_t address);
void MCU_Write(mcu_t& mcu, uint32_t address, uint8_t value);
void MCU_Write16(mcu_t& mcu, uint32_t address, uint16_t value);

inline uint32_t MCU_GetAddress(uint8_t page, uint16_t address) {
    return ((uint32_t)page << 16) + address;
}

inline uint8_t MCU_ReadCode(mcu_t& mcu) {
    return MCU_Read(mcu, MCU_GetAddress(mcu.cp, mcu.pc));
}

inline uint8_t MCU_ReadCodeAdvance(mcu_t& mcu) {
    uint8_t ret = MCU_ReadCode(mcu);
    mcu.pc++;
    return ret;
}

inline void MCU_SetRegisterByte(mcu_t& mcu, uint8_t reg, uint8_t val)
{
    mcu.r[reg] = val;
}

inline uint32_t MCU_GetVectorAddress(mcu_t& mcu, uint32_t vector)
{
    return MCU_Read32(mcu, vector * 4);
}

inline uint32_t MCU_GetPageForRegister(mcu_t& mcu, uint32_t reg)
{
    if (reg >= 6)
        return mcu.tp;
    else if (reg >= 4)
        return mcu.ep;
    return mcu.dp;
}

inline void MCU_ControlRegisterWrite(mcu_t& mcu, uint32_t reg, uint32_t siz, uint32_t data)
{
    if (siz)
    {
        if (reg == 0)
        {
            mcu.sr = (uint16_t)data;
            mcu.sr &= sr_mask;
        }
        else if (reg == 5) // FIXME: undocumented
        {
            mcu.dp = (uint8_t)(data & 0xff);
        }
        else if (reg == 4) // FIXME: undocumented
        {
            mcu.ep = (uint8_t)(data & 0xff);
        }
        else if (reg == 3) // FIXME: undocumented
        {
            mcu.br = (uint8_t)(data & 0xff);
        }
        else
        {
            MCU_ErrorTrap(mcu);
        }
    }
    else
    {
        if (reg == 1)
        {
            mcu.sr &= ~0xff;
            mcu.sr |= data & 0xff;
            mcu.sr &= sr_mask;
        }
        else if (reg == 3)
        {
            mcu.br = (uint8_t)data;
        }
        else if (reg == 4)
        {
            mcu.ep = (uint8_t)data;
        }
        else if (reg == 5)
        {
            mcu.dp = (uint8_t)data;
        }
        else if (reg == 7)
        {
            mcu.tp = (uint8_t)data;
        }
        else
        {
            MCU_ErrorTrap(mcu);
        }
    }
}

inline uint32_t MCU_ControlRegisterRead(mcu_t& mcu, uint32_t reg, uint32_t siz)
{
    uint32_t ret = 0;
    if (siz)
    {
        if (reg == 0)
        {
            ret = mcu.sr & sr_mask;
        }
        else if (reg == 5) // FIXME: undocumented
        {
            ret = (uint32_t)mcu.dp | ((uint32_t)mcu.dp << 8);
        }
        else if (reg == 4) // FIXME: undocumented
        {
            ret = (uint32_t)mcu.ep | ((uint32_t)mcu.ep << 8);
        }
        else if (reg == 3) // FIXME: undocumented
        {
            ret = (uint32_t)mcu.br | ((uint32_t)mcu.br << 8);;
        }
        else
        {
            MCU_ErrorTrap(mcu);
        }
        ret &= 0xffff;
    }
    else
    {
        if (reg == 1)
        {
            ret = mcu.sr & sr_mask;
        }
        else if (reg == 3)
        {
            ret = mcu.br;
        }
        else if (reg == 4)
        {
            ret = mcu.ep;
        }
        else if (reg == 5)
        {
            ret = mcu.dp;
        }
        else if (reg == 7)
        {
            ret = mcu.tp;
        }
        else
        {
            MCU_ErrorTrap(mcu);
        }
        ret &= 0xff;
    }
    return ret;
}

inline void MCU_SetStatus(mcu_t& mcu, uint32_t condition, uint32_t mask)
{
    if (condition)
        mcu.sr |= (uint16_t)mask;
    else
        mcu.sr &= (uint16_t)(~mask);
}

inline void MCU_PushStack(mcu_t& mcu, uint16_t data)
{
    if (mcu.r[7] & 1)
        MCU_Interrupt_Exception(mcu, EXCEPTION_SOURCE_ADDRESS_ERROR);
    mcu.r[7] -= 2;
    MCU_Write16(mcu, mcu.r[7], data);
}

inline uint16_t MCU_PopStack(mcu_t& mcu)
{
    uint16_t ret;
    if (mcu.r[7] & 1)
        MCU_Interrupt_Exception(mcu, EXCEPTION_SOURCE_ADDRESS_ERROR);
    ret = MCU_Read16(mcu, mcu.r[7]);
    mcu.r[7] += 2;
    return ret;
}

enum {
    // SC55
    MCU_BUTTON_POWER = 0,
    MCU_BUTTON_INST_L = 3,
    MCU_BUTTON_INST_R = 4,
    MCU_BUTTON_INST_MUTE = 5,
    MCU_BUTTON_INST_ALL = 6,

    MCU_BUTTON_MIDI_CH_L = 8,
    MCU_BUTTON_MIDI_CH_R = 9,
    MCU_BUTTON_CHORUS_L = 10,
    MCU_BUTTON_CHORUS_R = 11,
    MCU_BUTTON_PAN_L = 12,
    MCU_BUTTON_PAN_R = 13,
    MCU_BUTTON_PART_R = 14,

    MCU_BUTTON_KEY_SHIFT_L = 16,
    MCU_BUTTON_KEY_SHIFT_R = 17,
    MCU_BUTTON_REVERB_L = 18,
    MCU_BUTTON_REVERB_R = 19,
    MCU_BUTTON_LEVEL_L = 20,
    MCU_BUTTON_LEVEL_R = 21,
    MCU_BUTTON_PART_L = 22,

    // SC155 extra buttons
    MCU_BUTTON_USER = 1,
    MCU_BUTTON_PART_SEL = 2,
    MCU_BUTTON_INST_CALL = 7,
    MCU_BUTTON_PAN = 15,
    MCU_BUTTON_LEVEL = 23,
    MCU_BUTTON_PART1 = 24,
    MCU_BUTTON_PART2 = 25,
    MCU_BUTTON_PART3 = 26,
    MCU_BUTTON_PART4 = 27,
    MCU_BUTTON_PART5 = 28,
    MCU_BUTTON_PART6 = 29,
    MCU_BUTTON_PART7 = 30,
    MCU_BUTTON_PART8 = 31,

    // JV880
    MCU_BUTTON_CURSOR_L = 0,
    MCU_BUTTON_CURSOR_R = 1,
    MCU_BUTTON_TONE_SELECT = 2,
    MCU_BUTTON_MUTE = 3,
    MCU_BUTTON_DATA = 4,
    MCU_BUTTON_MONITOR = 5,
    MCU_BUTTON_COMPARE = 6,
    MCU_BUTTON_ENTER = 7,
    MCU_BUTTON_UTILITY = 8,
    MCU_BUTTON_PREVIEW = 9,
    MCU_BUTTON_PATCH_PERFORM = 10,
    MCU_BUTTON_EDIT = 11,
    MCU_BUTTON_SYSTEM = 12,
    MCU_BUTTON_RHYTHM = 13,
};


uint8_t MCU_ReadP0(mcu_t& mcu);
uint8_t MCU_ReadP1(mcu_t& mcu);
void MCU_WriteP0(mcu_t& mcu, uint8_t data);
void MCU_WriteP1(mcu_t& mcu, uint8_t data);
void MCU_GA_SetGAInt(mcu_t& mcu, int line, int value);

void MCU_EncoderTrigger(mcu_t& mcu, int dir);

void MCU_PostSample(mcu_t& mcu, const AudioFrame<int32_t>& frame);
void MCU_PostUART(mcu_t& mcu, uint8_t data);

void MCU_WorkThread_Lock(mcu_t& mcu);
void MCU_WorkThread_Unlock(mcu_t& mcu);

