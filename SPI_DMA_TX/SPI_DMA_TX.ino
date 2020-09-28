#include <DMAChannel.h>
#include <SPI.h>

DMAChannel dma(false);

static volatile uint16_t sinetable[] = {
0,   1,    2,    3,    4,    5,    6,    7,    8,  9, 
10,   11,    12,    13,    14,    15,    16,    17,    18,  19, 
20,   21,    22,    23,    24,    25,    26,    27,    28,  29, 
30,   31,    32,    33,    34,    35,    36,    37,    38,  39, 
40,   41,    42,    43,    44,    45,    46,    47,    48,  49, 
50,   51,    52,    53,    54,    55,    56,    57,    58,  59, 
60,   61,    62,    63,    64,    65,    66,    67,    68,  69, 
70,   71,    72,    73,    74,    75,    76,    77,    78,  79, 
80,   81,    82,    83,    84,    85,    86,    87,    88,  89, 
90,   91,    92,    93,    94,    95,    96,    97,    98,  99, 
100,  101,   102,   103,   104,   105,   106,   107,   108,  109, 
0,   1,    2,    3,    4,    5,    6,    7,    8,  9, 
10,   11,    12,    13,    14,    15,    16,    17,    18,  19, 
20,   21,    22,    23,    24,    25,    26,    27,    28,  29, 
30,   31,    32,    33,    34,    35,    36,    37,    38,  39, 
40,   41,    42,    43,    44,    45,    46,    47,    48,  49, 
50,   51,    52,    53,    54,    55,    56,    57,    58,  59, 
60,   61,    62,    63,    64,    65,    66,    67,    68,  69, 
70,   71,    72,    73,    74,    75,    76,    77,    78,  79, 
80,   81,    82,    83,    84,    85,    86,    87,    88,  89, 
90,   91,    92,    93,    94,    95,    96,    97,    98,  99, 
100,  101,   102,   103,   104,   105,   106,   107,   108,  109};


SPISettings _spiSettings;

void debugDMA() {
  Serial.printf("SA:%x SO:%d AT:%x NB:%x SL:%d DA:%x DO: %d CI:%x DL:%x CS:%x BI:%x\n", (uint32_t)dma.TCD->SADDR,
    dma.TCD->SOFF, dma.TCD->ATTR, dma.TCD->NBYTES, dma.TCD->SLAST, (uint32_t)dma.TCD->DADDR, 
    dma.TCD->DOFF, dma.TCD->CITER, dma.TCD->DLASTSGA, dma.TCD->CSR, dma.TCD->BITER);  
}

void isr(void)
{
  dma.clearInterrupt(); 
  debugDMA();
    
  Serial.print(";");
 
}

void setup() {

  Serial.begin(9600);
  while(!Serial) {
    delay(100);
  }
  delay(100);
  Serial.printf("Starting...\n");
  
  dma.begin(true); // allocate the DMA channel first

  SPI.begin();

  dma.sourceBuffer(sinetable, sizeof(sinetable));
  dma.destination(*(volatile uint16_t *)&(IMXRT_LPSPI4_S.TDR));
  dma.triggerAtHardwareEvent(DMAMUX_SOURCE_LPSPI4_TX);
  dma.attachInterrupt(isr);
  dma.transferCount(16);
  dma.transferSize(4);
  dma.interruptAtCompletion();
  dma.enable();
  
  // Lets try to output the first byte to make sure that we are in 8 bit mode...
  IMXRT_LPSPI4_S.DER = LPSPI_DER_TDDE; //| LPSPI_DER_RDDE; //enable DMA on both TX and RX

  debugDMA();
}

void loop() {
  //dma.triggerManual();
  //delay(1);
}
