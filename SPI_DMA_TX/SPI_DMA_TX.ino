#include <DMAChannel.h>
#include <SPI.h>
#define CS_PIN 0

DMAChannel dma(false);

static volatile uint16_t sinetable[] = {
0,   1,    2,    3,    4,    5};

SPISettings _spiSettings;

void debugDMA() {
  Serial.printf("SA:%x SO:%d AT:%x NB:%x SL:%d DA:%x DO: %d CI:%x DL:%x CS:%x BI:%x\n", (uint32_t)dma.TCD->SADDR,
    dma.TCD->SOFF, dma.TCD->ATTR, dma.TCD->NBYTES, dma.TCD->SLAST, (uint32_t)dma.TCD->DADDR, 
    dma.TCD->DOFF, dma.TCD->CITER, dma.TCD->DLASTSGA, dma.TCD->CSR, dma.TCD->BITER);  
}

int count=0;

void isr(void)
{
   dma.clearInterrupt(); 
   SPI.endTransaction();

  IMXRT_LPSPI4_S.FCR = LPSPI_FCR_TXWATER(15); // _spi_fcr_save; // restore the FSR status...
  IMXRT_LPSPI4_S.DER = 0;
  IMXRT_LPSPI4_S.CR = LPSPI_CR_MEN | LPSPI_CR_RRF | LPSPI_CR_RTF; // actually clear both...
  IMXRT_LPSPI4_S.SR = 0x3f00;    // clear out all of the other status...

  while (IMXRT_LPSPI4_S.FSR & 0x1f);
  while (IMXRT_LPSPI4_S.SR & LPSPI_SR_MBF) ;  //Status Register? Module Busy flag
  digitalWrite(CS_PIN, HIGH); 

  count++;
  if (count < 4) { 
    beginTransfer();
  }
}

void beginTransfer() {
  digitalWrite(CS_PIN, LOW); 
  
  IMXRT_LPSPI4_S.TCR = (IMXRT_LPSPI4_S.TCR & ~(LPSPI_TCR_FRAMESZ(31))) | LPSPI_TCR_FRAMESZ(7);  
  IMXRT_LPSPI4_S.FCR = 0; 
  // Lets try to output the first byte to make sure that we are in 8 bit mode...
  IMXRT_LPSPI4_S.DER = LPSPI_DER_TDDE;//| LPSPI_DER_RDDE; //enable DMA on both TX and RX
  IMXRT_LPSPI4_S.SR = 0x3f00; // clear out all of the other status...
  SPI.beginTransaction(SPISettings());
  dma.enable();
}

unsigned long lastMillis = 0;

void setup() {
  
  pinMode(CS_PIN,  OUTPUT);
  digitalWrite(CS_PIN, HIGH); 

  Serial.begin(9600);
  while(!Serial) {
    delay(100);
  }
  delay(100);
  Serial.printf("Starting...\n");
  
  dma.begin(true); // allocate the DMA channel first

  SPI.begin();

  dma.TCD->SADDR = sinetable;
  dma.TCD->SOFF = 2;
  dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
  dma.TCD->NBYTES_MLNO = 2;
  dma.TCD->SLAST = -sizeof(sinetable);
  dma.TCD->DOFF = 0;
  dma.TCD->CITER_ELINKNO = sizeof(sinetable)/2;
  dma.TCD->DLASTSGA = 0;
  dma.TCD->BITER_ELINKNO = sizeof(sinetable)/2;
  dma.TCD->CSR = DMA_TCD_CSR_INTMAJOR;
  dma.TCD->DADDR = (void *)((uint32_t)&(IMXRT_LPSPI4_S.TDR));
  dma.triggerAtHardwareEvent(DMAMUX_SOURCE_LPSPI4_TX);
  dma.disableOnCompletion();
  dma.attachInterrupt(isr);
  dma.interruptAtCompletion();

  lastMillis = millis();
  beginTransfer();
}


void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis > lastMillis + 1) {
    lastMillis = currentMillis;
    count = 0;
    beginTransfer();
  }
}
