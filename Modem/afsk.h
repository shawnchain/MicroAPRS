//////////////////////////////////////////////////////
// First things first, all the includes we need     //
//////////////////////////////////////////////////////

#ifndef FSK_MODEM_H
#define FSK_MODEM_H

#include "config.h"             // Various configuration values
#include "hardware.h"           // Hardware functions

#include <cfg/compiler.h>       // Compiler info from BertOS
#include <struct/fifobuf.h>     // FIFO buffer implementation from BertOS
#include <io/kfile.h>           // The BertOS KFile interface. This is
                                // used for letting other functions read
                                // from or write to the modem like a
                                // file descriptor.

//////////////////////////////////////////////////////
// Our type definitions and function declarations   //
//////////////////////////////////////////////////////

#define SAMPLERATE 9600            				// The rate at which we are sampling and synthesizing
#define BITRATE    1200            				// The actual bitrate at baseband. This is the baudrate.
#define SAMPLESPERBIT (SAMPLERATE / BITRATE)	// How many DAC/ADC samples constitute one bit (8).

// This defines an errortype for a receive-
// buffer overrun.
#define RX_OVERRUN BV(0)

// This struct defines a Hdlc parser. It will let
// us parse the raw bits coming in from the modem
// and synchronise to byte boundaries.
typedef struct Hdlc
{
    uint8_t demodulatedBits;	  // Incoming bitstream from demodulator
    uint8_t bitIndex;       	 // The current received bit in the current received byte
    uint8_t currentByte;    	// The byte we're currently receiving
    bool receiving;            // Whether or not where actually receiving data (or just noise ;P)
} Hdlc;

// This is our primary modem struct. It defines
// all the values we need to modulate and
// demodulate data from the physical medium.
typedef struct Afsk
{
    KFile fd;                               // A file descriptor for reading from and
                                            // writing to the modem

    // I/O hardware pins
    int adcPin;                             // Pin for incoming signal

    // General values
    Hdlc hdlc;                              // We need a link control structure
    uint16_t preambleLength;                // Length of sync preamble
    uint16_t tailLength;                    // Length of transmission tail

    // Modulation values
    uint8_t sampleIndex;                    // Current sample index for outgoing bit 
    uint8_t currentOutputByte;              // Current byte to be modulated
    uint8_t txBit;                          // Mask of current modulated bit
    bool bitStuff;                          // Whether bitstuffing is allowed

    uint8_t bitstuffCount;                  // Counter for bit-stuffing

    uint16_t phaseAcc;                      // Phase accumulator
    uint16_t phaseInc;                      // Phase increment per sample

    FIFOBuffer txFifo;                      // FIFO for transmit data
    uint8_t txBuf[CONFIG_AFSK_TX_BUFLEN];   // Actial data storage for said FIFO

    volatile bool sending;                  // Set when modem is sending

    // Demodulation values
    FIFOBuffer delayFifo;                   // Delayed FIFO for frequency discrimination
    int8_t delayBuf[SAMPLESPERBIT / 2 + 1]; // Actual data storage for said FIFO

    FIFOBuffer rxFifo;                      // FIFO for received data
    uint8_t rxBuf[CONFIG_AFSK_RX_BUFLEN];   // Actual data storage for said FIFO

    int16_t iirX[2];                        // IIR Filter X cells
    int16_t iirY[2];                        // IIR Filter Y cells

    uint8_t sampledBits;                    // Bits sampled by the demodulator (at ADC speed)
    int8_t currentPhase;                    // Current phase of the demodulator
    uint8_t actualBits;                     // Actual found bits at correct bitrate

    volatile int status;                    // Status of the modem, 0 means OK
} Afsk;

// Explanation nessecary for this. BertOS uses an
// object-oriented approach for handling "file-like"
// transactions (yes, we are using C :P). What we are
// doing here is defining a specific "file type" for
// the standard KFile to identify the modem as a "file"
// that can be read from and written to.
#define KFT_AFSK MAKE_ID('F', 'S', 'K', 'M')

// We then make a macro that can "typecast" a generic
// KFile file-pointer to an Afsk "object". This lets
// other pieces of code read from and write to the AFSK
// "objects" buffers with the standard KFile operations.
// If this seems weird and confusing, check out the
// BertOS KFile explanation at:
// http://www.bertos.org/use/tutorial-front-page/drivers-kfile-interface
INLINE Afsk *AFSK_CAST(KFile *fd) {
  // We need to assert that the what we are trying
  // to read/write is actually an AFSK "object",
  // identified by the KFT_AFSK constant
  ASSERT(fd->_type == KFT_AFSK);
  return (Afsk *)fd;
}

// Declare Interrupt Service Routines
// and initialization functions
void afsk_adc_isr(Afsk *af, int8_t sample);
uint8_t afsk_dac_isr(Afsk *af);
void afsk_init(Afsk *af, int adc_ch);

#endif