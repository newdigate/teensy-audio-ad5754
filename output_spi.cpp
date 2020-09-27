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
	dma.begin(true); // Allocate the DMA channel first

    config_spi();

	for (int i=0; i < 16; i++) {
		block_input[i] = NULL;
	}

#if defined(__IMXRT1052__) || defined(__IMXRT1062__)
	//CORE_PIN11_CONFIG  = 3;  //1:TX_DATA0

	dma.TCD->SADDR = tdm_tx_buffer;
	dma.TCD->SOFF = 4;
	dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2);
	dma.TCD->NBYTES_MLNO = 4;
	dma.TCD->SLAST = -sizeof(tdm_tx_buffer);
	dma.TCD->DADDR = &IMXRT_LPSPI4_S.TDR;
	dma.TCD->DOFF = 0;
	dma.TCD->CITER_ELINKNO = sizeof(tdm_tx_buffer) / 4;
	dma.TCD->DLASTSGA = 0;
	dma.TCD->BITER_ELINKNO = sizeof(tdm_tx_buffer) / 4;
	dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_LPSPI4_TX);
	//        dma.triggerContinuously();

	update_responsibility = update_setup();
	Serial.printf("update_responsibility = %d\n", update_responsibility);



	Serial.print("LP4.DER...\n");

	//IMXRT_LPSPI4_S.TCR = (IMXRT_LPSPI4_S.TCR & ~(LPSPI_TCR_FRAMESZ(31))) | LPSPI_TCR_FRAMESZ(7);
	//IMXRT_LPSPI4_S.FCR = 0;

	// Lets try to output the first byte to make sure that we are in 8 bit mode...
	IMXRT_LPSPI4_S.DER = LPSPI_DER_TDDE | LPSPI_DER_RDDE; //enable DMA on both TX and RX
	IMXRT_LPSPI4_S.SR = 0x3f00 | LPSPI_SR_TDF; // Status Register: ??? + Transmit Data Flag



    dma.enable();

#endif
	dma.attachInterrupt(isr);
}

// TODO: needs optimization...
static void memcpy_tdm_tx(uint32_t *dest, const uint32_t *src1, const uint32_t *src2)
{
	uint32_t i, in1, in2, out1, out2;

	for (i=0; i < AUDIO_BLOCK_SAMPLES/2; i++) {
		in1 = *src1++;
		in2 = *src2++;
		out1 = (in1 << 16) | (in2 & 0xFFFF);
		out2 = (in1 & 0xFFFF0000) | (in2 >> 16);
		*dest = out1;
		*(dest + 8) = out2;
		dest += 16;
	}
}

void AudioOutputSPI::isr(void)
{
	Serial.print(";");
	uint32_t *dest;
	const uint32_t *src1, *src2;
	uint32_t i, saddr;

	saddr = (uint32_t)(dma.TCD->SADDR);
	dma.clearInterrupt();
	if (saddr < (uint32_t)tdm_tx_buffer + sizeof(tdm_tx_buffer) / 2) {
		// DMA is transmitting the first half of the buffer
		// so we must fill the second half
		dest = tdm_tx_buffer + AUDIO_BLOCK_SAMPLES*8;
	} else {
		// DMA is transmitting the second half of the buffer
		// so we must fill the first half
		dest = tdm_tx_buffer;
	}
	if (update_responsibility) AudioStream::update_all();
	for (i=0; i < 16; i += 2) {
		src1 = block_input[i] ? (uint32_t *)(block_input[i]->data) : zeros;
		src2 = block_input[i+1] ? (uint32_t *)(block_input[i+1]->data) : zeros;
		memcpy_tdm_tx(dest, src1, src2);
		dest++;
	}
	for (i=0; i < 16; i++) {
		if (block_input[i]) {
			release(block_input[i]);
			block_input[i] = NULL;
		}
	}
}


void AudioOutputSPI::update(void)
{
	//Serial.print("U");
	audio_block_t *prev[16];
	unsigned int i;

	__disable_irq();
	for (i=0; i < 16; i++) {
		prev[i] = block_input[i];
		block_input[i] = receiveReadOnly(i);
	}
	__enable_irq();
	for (i=0; i < 16; i++) {
		if (prev[i]) release(prev[i]);
	}
}

void AudioOutputSPI::config_spi(void)
{
#if defined(__IMXRT1062__)
//PLL:
    int fs = AUDIO_SAMPLE_RATE_EXACT;
    // PLL between 27*24 = 648MHz und 54*24=1296MHz
    int n1 = 4; //SAI prescaler 4 => (n1*n2) = multiple of 4
    int n2 = 1 + (24000000 * 27) / (fs * 256 * n1);

    double C = ((double)fs * 256 * n1 * n2) / 24000000;
    int c0 = C;
    int c2 = 10000;
    int c1 = C * c2 - (c0 * c2);
    set_audioClock(c0, c1, c2);
#endif
}

