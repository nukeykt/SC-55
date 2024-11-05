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
#include <stdint.h>

struct mcu_t;

struct PCM_Config
{
    // config_reg_3c
    int noise_mask = 0;
    int orval      = 0;
    int write_mask = 0;
    int dac_mask   = 0; // unused

    bool oversampling = false;

    // config_reg_3d
    // important that this starts at 1, see derivation in PCM_Write
    int reg_slots = 1;
};

struct pcm_t {
    uint32_t ram1[32][8]{};
    uint16_t ram2[32][16]{};
    uint32_t select_channel = 0;
    uint32_t voice_mask = 0;
    uint32_t voice_mask_pending = 0;
    uint32_t voice_mask_updating = 0;
    uint32_t write_latch = 0;
    uint32_t wave_read_address = 0;
    uint8_t wave_byte_latch = 0;
    uint32_t read_latch = 0;
    uint8_t config_reg_3c = 0; // SC55:c3 JV880:c0
    uint8_t config_reg_3d = 0;
    uint32_t irq_channel = 0;
    uint32_t irq_assert = 0;
    PCM_Config config{};

    uint32_t nfs = 0;

    uint32_t tv_counter = 0;

    uint64_t cycles = 0;

    uint16_t eram[0x4000]{};

    int accum_l = 0;
    int accum_r = 0;
    int rcsum[2]{};

    mcu_t* mcu = nullptr;

    uint8_t waverom1[0x200000]{};
    uint8_t waverom2[0x200000]{};
    uint8_t waverom3[0x100000]{};
    uint8_t waverom_card[0x200000]{};
    uint8_t waverom_exp[0x800000]{};

    bool disable_oversampling = false;
};

void PCM_Write(pcm_t& pcm, uint32_t address, uint8_t data);
uint8_t PCM_Read(pcm_t& pcm, uint32_t address);
void PCM_Init(pcm_t& pcm, mcu_t& mcu);
void PCM_Update(pcm_t& pcm, uint64_t cycles);
uint32_t PCM_GetOutputFrequency(const pcm_t& pcm);
void PCM_GetConfig(PCM_Config& config, uint8_t config_byte);
