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
#include "output_spi.h"
#include "memcpy_audio.h"
#include "utility/imxrt_hw.h"
#include <SPI.h>
#include <imxrt.h>

audio_block_t * AudioOutputSPI::block_input[16] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};
bool AudioOutputSPI::update_responsibility = false;
static uint32_t zeros[AUDIO_BLOCK_SAMPLES/2];
static uint32_t tdm_tx_buffer[AUDIO_BLOCK_SAMPLES*16];
DMAChannel AudioOutputSPI::dma(false);

void AudioOutputSPI::begin(void)
{
	Serial.begin(9600);
	while(!Serial) {
		delay(200);
	}
	delay(200);

	SPI.begin();

	Serial.print("void AudioOutputSPI::begin(void)\n");

    config_spi();
    config_dma();

	for (int i=0; i < 16; i++) {
		block_input[i] = NULL;
	}
}


void AudioOutputSPI::isr(void)
{
    dma.clearInterrupt();
    SPI.endTransaction();

    IMXRT_LPSPI4_S.FCR = LPSPI_FCR_TXWATER(15); // _spi_fcr_save; // restore the FSR status...
    IMXRT_LPSPI4_S.DER = 0;
    IMXRT_LPSPI4_S.CR = LPSPI_CR_MEN | LPSPI_CR_RRF | LPSPI_CR_RTF; // actually clear both...
    IMXRT_LPSPI4_S.SR = 0x3f00;    // clear out all of the other status...

    while (IMXRT_LPSPI4_S.FSR & 0x1f);
    while (IMXRT_LPSPI4_S.SR & LPSPI_SR_MBF) ;  //Status Register? Module Busy flag
    digitalWrite(DA_SYNC, HIGH);

    bytesReceived++;

    if (bytesReceived < 4) {
        buf[0] = bytesReceived;                   //DAC0, channel=count
        buf[1] = voltages[bytesReceived] >> 8;
        buf[2] = voltages[bytesReceived] & 0xff;
        buf[3] = bytesReceived;                   //DAC1, channel=count
        buf[4] = voltages[bytesReceived+4] >> 8;
        buf[5] = voltages[bytesReceived+4] & 0xff;
    } else {
        bytesReceived = 0;
    }

    beginTransfer();
}

void AudioOutputSPI::beginTransfer()
{
    if (bytesReceived == 0) {

        read_index++;
        if (read_index == 128) return;

        voltages[0] = (block_input[0] != NULL) ? block_input[0]->data[read_index] : 0;
        voltages[1] = (block_input[1] != NULL) ? block_input[1]->data[read_index] : 0;
        voltages[2] = (block_input[2] != NULL) ? block_input[2]->data[read_index] : 0;
        voltages[3] = (block_input[3] != NULL) ? block_input[3]->data[read_index] : 0;
        voltages[4] = (block_input[4] != NULL) ? block_input[4]->data[read_index] : 0;
        voltages[5] = (block_input[5] != NULL) ? block_input[5]->data[read_index] : 0;
        voltages[6] = (block_input[6] != NULL) ? block_input[6]->data[read_index] : 0;
        voltages[7] = (block_input[7] != NULL) ? block_input[7]->data[read_index] : 0;

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

    IMXRT_LPSPI4_S.TCR = (IMXRT_LPSPI4_S.TCR & ~(LPSPI_TCR_FRAMESZ(31))) | LPSPI_TCR_FRAMESZ(7);
    IMXRT_LPSPI4_S.FCR = 0;
    // Lets try to output the first byte to make sure that we are in 8 bit mode...
    IMXRT_LPSPI4_S.DER = LPSPI_DER_TDDE;//| LPSPI_DER_RDDE; //enable DMA on both TX and RX
    IMXRT_LPSPI4_S.SR = 0x3f00; // clear out all of the other status...
    SPI.beginTransaction(SPISettings(14000000, MSBFIRST, SPI_MODE0));
    dma.enable();
}


void AudioOutputSPI::update(void)
{
	audio_block_t *prev[16];
	unsigned int i;

	__disable_irq();
	for (i=0; i < 16; i++) {
		prev[i] = block_input[i];
		block_input[i] = receiveReadOnly(i);
	}
	read_index = 0;
	__enable_irq();
	for (i=0; i < 16; i++) {
		if (prev[i]) release(prev[i]);
	}
}

void AudioOutputSPI::config_dma(void)
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
    dma.TCD->DADDR = (void *)((uint32_t)&(IMXRT_LPSPI4_S.TDR));
    dma.triggerAtHardwareEvent(DMAMUX_SOURCE_LPSPI4_TX);
    dma.disableOnCompletion();
    dma.attachInterrupt(isr);
    dma.interruptAtCompletion();
}

void AudioOutputSPI::config_spi(void)
{
}

