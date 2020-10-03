/* This file is based on output_tdm from
 *   https://github.com/PaulStoffregen/Audio
 *
 * Audio Library for Teensy 3.X
 * Copyright (c) 2017, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <Arduino.h>
#include "output_ad5754_dual.h"
#include "memcpy_audio.h"
#include "utility/imxrt_hw.h"
#include <SPI.h>
#include <imxrt.h>
#include <cstdint>

/* AD5754R Register Map */
#define AD5754R_REG_DAC             0x00 // DAC register
#define AD5754R_REG_RANGE_SELECT    0x01 // Output range select register
#define AD5754R_REG_POWER_CONTROL   0x02 // Power control register
#define AD5754R_REG_CONTROL         0x03 // Control register

/* AD5754R Channel Address */
#define AD5754R_DAC_A               0x00 // Address of channel A
#define AD5754R_DAC_B               0x01 // Address of channel B
#define AD5754R_DAC_C               0x02 // Address of channel C
#define AD5754R_DAC_D               0x03 // Address of channel D
#define AD5754R_DAC_ALL             0x04 // All four DACs


/* AD5754R Range Bits */
#define AD5754R_UNIPOLAR_5_RANGE    0x00 // 0..+5(V)
#define AD5754R_UNIPOLAR_10_RANGE   0x01 // 0..+10(V)
#define AD5754R_UNIPOLAR_10_8_RANGE 0x02 // 0..+10.8(V)
#define AD5754R_BIPOLAR_5_RANGE     0x03 // -5..+5(V)
#define AD5754R_BIPOLAR_10_RANGE    0x04 // -10...+10(V)
#define AD5754R_BIPOLAR_10_8_RANGE  0x05 // -10.8...+10.8(V)

audio_block_t * AudioOutputAD5754Dual::block_input[8] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};
bool AudioOutputAD5754Dual::update_responsibility = false;

unsigned int AudioOutputAD5754Dual::read_index = 0;
unsigned int AudioOutputAD5754Dual::DA_SYNC = 16;
volatile uint8_t AudioOutputAD5754Dual::buf[6] = {0,0,0,0,0,0};
int AudioOutputAD5754Dual::voltages[8] = {0,0,0,0,0,0,0,0};
unsigned int AudioOutputAD5754Dual::bytesTransmitted = 0;

DMAChannel AudioOutputAD5754Dual::dma(false);

void AudioOutputAD5754Dual::begin(void)
{
    pinMode(DA_SYNC, OUTPUT);
    digitalWrite(DA_SYNC, HIGH);
	SPI1.begin();
    delay(1);

    // Initialize DAC0, DAC1
    SPI1.beginTransaction(SPISettings());
    digitalWrite(DA_SYNC, LOW);
    uint8_t configureDac[] = {
            0x10,
            0x00,
            0x0f,
            0x10,
            0x00,
            0x0f
    };
    SPI1.transfer(configureDac, 6);
    SPI1.endTransaction();
    digitalWrite(DA_SYNC, HIGH);
    delayMicroseconds(10);

    // Set voltage range for DAC0, DAC1
    digitalWrite(DA_SYNC, LOW);
    uint8_t configureDacVoltageRange[] = {
            0x28,
            0x00,
            AD5754R_UNIPOLAR_10_RANGE,
            0x28,
            0x00,
            AD5754R_UNIPOLAR_10_RANGE
    };
    SPI1.beginTransaction(SPISettings());
    SPI1.transfer(configureDacVoltageRange, 6);
    SPI1.endTransaction();
    digitalWrite(DA_SYNC, HIGH);
    delayMicroseconds(10);

    // Disable SPI ouput from DAC (not needed - AD5754R_SDO_DISABLE) :
    digitalWrite(DA_SYNC, LOW);
    uint8_t configureDataOutputDisabled[] = {
            0x0C,
            0x00,
            0x00,
            0x0C,
            0x00,
            0x00
    };
    SPI1.beginTransaction(SPISettings());
    SPI1.transfer(configureDataOutputDisabled, 6);
    SPI1.endTransaction();
    digitalWrite(DA_SYNC, HIGH);
    delayMicroseconds(10);

    config_dma();

	for (int i=0; i < 8; i++) {
		block_input[i] = NULL;
	}
}


void AudioOutputAD5754Dual::isr(void)
{
    dma.clearInterrupt();
    SPI1.endTransaction();

    IMXRT_LPSPI3_S.FCR = LPSPI_FCR_TXWATER(15); // FIFO control register: set transmit watermark
    IMXRT_LPSPI3_S.DER = 0;                     // DMA enable register: disable DMA TX
    IMXRT_LPSPI3_S.CR = LPSPI_CR_MEN | LPSPI_CR_RRF | LPSPI_CR_RTF; // Control register... ?
    IMXRT_LPSPI3_S.SR = 0x3f00;                 // Status register: clear out all of the other status...

    while (IMXRT_LPSPI3_S.FSR & 0x1f);          //FIFO Status Register? wait until FIFO is empty before continuing...
    while (IMXRT_LPSPI3_S.SR & LPSPI_SR_MBF) ;  //Status Register? Module Busy flag, wait until SPI is not busy...
    digitalWrite(DA_SYNC, HIGH);

    bytesTransmitted++;

    if (bytesTransmitted < 4) {
        buf[0] = bytesTransmitted;                   //DAC0, channel=count
        buf[1] = voltages[bytesTransmitted] >> 8;
        buf[2] = voltages[bytesTransmitted] & 0xff;
        buf[3] = bytesTransmitted;                   //DAC1, channel=count
        buf[4] = voltages[bytesTransmitted+4] >> 8;
        buf[5] = voltages[bytesTransmitted+4] & 0xff;
    } else {
        bytesTransmitted = 0;
    }

    beginTransfer();
}
const uint32_t zero_level = 0xFFFF / 2;
void AudioOutputAD5754Dual::beginTransfer()
{
    if (bytesTransmitted == 0) {

        read_index++;
        if (read_index == 128) return;

        voltages[0] = (block_input[0] != NULL) ? block_input[0]->data[read_index] + zero_level : zero_level;
        voltages[1] = (block_input[1] != NULL) ? block_input[1]->data[read_index] + zero_level : zero_level;
        voltages[2] = (block_input[2] != NULL) ? block_input[2]->data[read_index] + zero_level : zero_level;
        voltages[3] = (block_input[3] != NULL) ? block_input[3]->data[read_index] + zero_level : zero_level;
        voltages[4] = (block_input[4] != NULL) ? block_input[4]->data[read_index] + zero_level : zero_level;
        voltages[5] = (block_input[5] != NULL) ? block_input[5]->data[read_index] + zero_level : zero_level;
        voltages[6] = (block_input[6] != NULL) ? block_input[6]->data[read_index] + zero_level : zero_level;
        voltages[7] = (block_input[7] != NULL) ? block_input[7]->data[read_index] + zero_level : zero_level;

        // first 3 bytes -> DAC0
        buf[0] = 0x00;              // channel == 0
        buf[1] = voltages[0] >> 8;
        buf[2] = voltages[0] & 0xff;

        // second 3 bytes -> DAC1
        buf[3] = 0x00;              // channel == 0
        buf[4] = voltages[4] >> 8;
        buf[5] = voltages[4] & 0xff;
    }

    digitalWrite(DA_SYNC, LOW);

    IMXRT_LPSPI3_S.TCR = (IMXRT_LPSPI3_S.TCR & ~(LPSPI_TCR_FRAMESZ(31))) | LPSPI_TCR_FRAMESZ(7); // Transmit Control Register: ?
    IMXRT_LPSPI3_S.FCR = 0; // FIFO control register

    IMXRT_LPSPI3_S.DER = LPSPI_DER_TDDE;//DMA Enable register: enable DMA on TX
    IMXRT_LPSPI3_S.SR = 0x3f00; // StatusRegister: clear out all of the other status...
    SPI1.beginTransaction(SPISettings(14000000, MSBFIRST, SPI_MODE0));
    dma.enable();
}


void AudioOutputAD5754Dual::update(void)
{
    // not needed, as tx buffer finishes before update is called again. (SPI clk 14MHz is good, 4MHz too slow, need this code below to drop frames
#ifdef SLOW_SPEED_SPI
    if (IMXRT_LPSPI3_S.SR & LPSPI_SR_MBF) return;  //already busy, drop frames...
    if (IMXRT_LPSPI3_S.FSR & 0x1f) return; // SPI fifo still busy, drop frames...
#endif
	audio_block_t *prev[8];
	unsigned int i;

	__disable_irq();
	for (i=0; i < 8; i++) {
		prev[i] = block_input[i];
		block_input[i] = receiveReadOnly(i);
	}
	__enable_irq();

	for (i=0; i < 8; i++) {
		if (prev[i]) release(prev[i]);
	}
    read_index = 0;
    beginTransfer();
}

void AudioOutputAD5754Dual::config_dma(void)
{
    dma.begin(true); // allocate the DMA channel first
    dma.TCD->SADDR = buf;
    dma.TCD->SOFF = 1;
    dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(0) | DMA_TCD_ATTR_DSIZE(0);
    dma.TCD->NBYTES_MLNO = 1;
    dma.TCD->SLAST = -sizeof(buf);
    dma.TCD->DOFF = 0;
    dma.TCD->CITER_ELINKNO = sizeof(buf);
    dma.TCD->DLASTSGA = 0;
    dma.TCD->BITER_ELINKNO = sizeof(buf);
    dma.TCD->CSR = DMA_TCD_CSR_INTMAJOR;
    dma.TCD->DADDR = (void *)((uint32_t)&(IMXRT_LPSPI3_S.TDR));
    dma.triggerAtHardwareEvent(DMAMUX_SOURCE_LPSPI3_TX);
    dma.disableOnCompletion();
    dma.attachInterrupt(isr);
    dma.interruptAtCompletion();
}