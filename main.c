

#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_uart.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/uart.h"
#include "driverlib/udma.h"
#include "utils/cpu_usage.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"



//****************************************************************************
//
// System clock rate in Hz.
//
//****************************************************************************
uint32_t g_ui32SysClock;



//*****************************************************************************
//
// The size of the UART receive buffer.
//
//*****************************************************************************

#define UART_RXBUF_SIZE         1



//*****************************************************************************
//
// The receive buffer used for the UART transfers.
//
//*****************************************************************************

static uint8_t g_ui8RxBuf[UART_RXBUF_SIZE];



//*****************************************************************************
//
// The count of uDMA errors.  This value is incremented by the uDMA error
// handler.
//
//*****************************************************************************
static uint32_t g_ui32uDMAErrCount = 0;


//*****************************************************************************

//*****************************************************************************
//
// The count of UART uDMA transfer blocks.  This value is incremented by the
// uDMA interrupt handler whenever a UART block transfer is completed.
//
//*****************************************************************************
static uint32_t UARTXferCount = 0;



//*****************************************************************************
//
// The control table used by the uDMA controller.  This table must be aligned
// to a 1024 byte boundary.
//
//*****************************************************************************
#if defined(ewarm)
#pragma data_alignment=1024
uint8_t pui8ControlTable[1024];
#elif defined(ccs)
#pragma DATA_ALIGN(pui8ControlTable, 1024)
uint8_t pui8ControlTable[1024];
#else
uint8_t pui8ControlTable[1024] __attribute__ ((aligned(1024)));
#endif

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif


//*****************************************************************************
//
// The interrupt handler for uDMA errors.  This interrupt will occur if the
// uDMA encounters a bus error while trying to perform a transfer.  This
// handler just increments a counter if an error occurs.
//
//*****************************************************************************
void
uDMAErrorHandler(void)
{
    uint32_t ui32Status;

    //
    // Check for uDMA error bit
    //
    ui32Status = uDMAErrorStatusGet();

    //
    // If there is a uDMA error, then clear the error and increment
    // the error counter.
    //
    if(ui32Status)
    {
        uDMAErrorStatusClear();
        g_ui32uDMAErrCount++;
    }
}



//*****************************************************************************
//
// The interrupt handler for UART0.  This interrupt will occur when a DMA
// transfer is complete using the UART0 uDMA channel.  It will also be
// triggered if the peripheral signals an error.  This interrupt handler will
//  restart a TX and RX uDMA transfer if the prior transfer is complete.
//  This will keep the UART  running continuously
//
//
//*****************************************************************************
void
UART0IntHandler(void)
{
    uint32_t ui32Status;
    uint32_t ui32Mode;

    //
    // Read the interrupt status of the UART.
    //
    ui32Status = UARTIntStatus(UART0_BASE, 1);

    //
    // Clear any pending status,
    //
    UARTIntClear(UART0_BASE, ui32Status);


//*****************************************************************************
//
// If the UART0 DMA TX channel is in stop mode, that means the TX DMA transfer
// is done. so re enable it again
//
//*****************************************************************************

   ui32Mode = uDMAChannelModeGet(UDMA_CHANNEL_UART0TX | UDMA_PRI_SELECT);
    if(ui32Mode == UDMA_MODE_STOP)
    {

    	 uDMAChannelTransferSet(UDMA_CHANNEL_UART0TX | UDMA_PRI_SELECT,
    	                                   UDMA_MODE_BASIC, g_ui8RxBuf,
    	                                   (void *)(UART0_BASE + UART_O_DR),
    	                                   sizeof(g_ui8RxBuf));
    	    uDMAChannelEnable(UDMA_CHANNEL_UART0TX);

    }





//*****************************************************************************
//
// If the UART0 DMA RX channel is in stop mode, that means the TX DMA transfer
// is done. so re enable it again
//
//*****************************************************************************




    if(!uDMAChannelIsEnabled(UDMA_CHANNEL_UART0RX))
   {
    	UARTXferCount++;



        uDMAChannelTransferSet(UDMA_CHANNEL_UART0RX | UDMA_PRI_SELECT,
                                   UDMA_MODE_BASIC,
                                   (void *)(UART0_BASE + UART_O_DR),g_ui8RxBuf,
                                   sizeof(g_ui8RxBuf));

        uDMAChannelEnable(UDMA_CHANNEL_UART0RX);



    }

}


//*****************************************************************************
//
// Initializes the UART0 peripheral and sets up the TX and RX uDMA channels.
// The uDMA channels are configured so that the RX channel
// will copy data to a buffer and then transmit again these data from that buffer
// to TX channel
//*****************************************************************************
void
InitUART0Transfer(void)
{


	//
	// Enable the UART for operation, and enable the uDMA interface for both TX
    // and RX channels.
	//


    UARTEnable(UART0_BASE);
    UARTDMAEnable(UART0_BASE, UART_DMA_RX|UART_DMA_TX);




    //
    // Put the attributes in a known state for the uDMA UART0RX channel.  These
    // should already be disabled by default.
    //
    uDMAChannelAttributeDisable(UDMA_CHANNEL_UART0RX,
                                    UDMA_ATTR_ALTSELECT |
                                    UDMA_ATTR_HIGH_PRIORITY |
                                    UDMA_ATTR_REQMASK);



    //
    // Configure the control parameters for the UART RX.  The uDMA UART RX
    // channel is used to transfer a block of data from the UART to a buffer.
    // The data size is 8 bits.  The source address increment is none
    // since the data is coming from the UART0.The destination address increment is
    // NONE since the data is to be written to a single buffer.  The
    // arbitration size is set to 1, which matches the UART RX FIFO trigger
    // threshold.
    //
    uDMAChannelControlSet(UDMA_CHANNEL_UART0RX | UDMA_PRI_SELECT,
                              UDMA_SIZE_8 | UDMA_SRC_INC_NONE |
                              UDMA_DST_INC_NONE |
                              UDMA_ARB_1);

    //
    // Set up the transfer parameters for the uDMA UART RX channel.  This will
    // configure the transfer source and destination and the transfer size.
    // Basic mode is used because the peripheral is making the uDMA transfer
    // request.  The source is the URAT data register and the destination is a buffer
    //
    uDMAChannelTransferSet(UDMA_CHANNEL_UART0RX | UDMA_PRI_SELECT,
                               UDMA_MODE_BASIC,(void *)(UART0_BASE + UART_O_DR),g_ui8RxBuf,
                               sizeof(g_ui8RxBuf));

    ////////////////////////////////////////////////////


    //
    // Put the attributes in a known state for the uDMA UART0TX channel.  These
    // should already be disabled by default.
    //

    uDMAChannelAttributeDisable(UDMA_CHANNEL_UART0TX,
                                       UDMA_ATTR_ALTSELECT |
                                       UDMA_ATTR_HIGH_PRIORITY |
                                       UDMA_ATTR_REQMASK);



    //
    // Configure the control parameters for the UART TX.  The uDMA UART TX
    // channel is used to transfer a block of data from a buffer to the UART.
    // The data size is 8 bits.  The source address increment is NONE
    // since the data is coming from a buffer.  The destination increment is
    // none since the data is to be written to the UART data register.  The
    // arbitration size is set to 1, which matches the UART TX FIFO trigger
    // threshold.
    //
    uDMAChannelControlSet(UDMA_CHANNEL_UART0TX | UDMA_PRI_SELECT,
                                 UDMA_SIZE_8 | UDMA_SRC_INC_NONE |
                                 UDMA_DST_INC_NONE |
                                 UDMA_ARB_1);
    //
    // Set up the transfer parameters for the uDMA UART TX channel.  This will
    // configure the transfer source and destination and the transfer size.
    // Basic mode is used because the peripheral is making the uDMA transfer
    // request.  The source is the buffer and the destination is the UART
    // data register.
    //
    uDMAChannelTransferSet(UDMA_CHANNEL_UART0TX | UDMA_PRI_SELECT,
                                   UDMA_MODE_BASIC, g_ui8RxBuf,
                                   (void *)(UART0_BASE + UART_O_DR),
                                   sizeof(g_ui8RxBuf));

    //////////////////////////////////////////////////////////





    // Now both the uDMA UART RX and TX  channels are primed to start a
    // transfer.  As soon as the channels are enabled, the peripheral will
    // issue a transfer request and the data transfers will begin.
    //

    uDMAChannelEnable(UDMA_CHANNEL_UART0RX);
    uDMAChannelEnable(UDMA_CHANNEL_UART0TX);


    //
    // Enable the UART DMA RX interrupt.
    //
   UARTIntEnable(UART0_BASE, UART_INT_DMARX);

    //
    // Enable the UART peripheral interrupts.
    //
    IntEnable(INT_UART0);
}



//*****************************************************************************
//
// Configure the UART and its pins.
//
//*****************************************************************************
void
ConfigureUART(void)
{
    //
    // Enable the GPIO Peripheral used by the UART.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    //
    // Enable UART0
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    //
    // Configure GPIO Pins for UART mode.
    //
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

   UARTConfigSetExpClk(UART0_BASE, g_ui32SysClock, 115200,
                            UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                            UART_CONFIG_PAR_NONE);
}


int
main(void)
{

    //
    // Set the clocking to run directly from the crystal at 120MHz.
    //
    g_ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
                                             SYSCTL_OSC_MAIN |
                                             SYSCTL_USE_PLL |
                                             SYSCTL_CFG_VCO_480), 120000000);


    //
    // Enable the GPIO port that is used for the on-board LED.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);

    //
    // Enable the GPIO pins for the LED (PN0).
    //
    GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_0);



    //
    // Initialize the UART.
    //
    ConfigureUART();






    // Enable the uDMA controller at the system level.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UDMA);

    //
    // Enable the uDMA controller error interrupt.  This interrupt will occur
    // if there is a bus error during a transfer.
    //
    IntEnable(INT_UDMAERR);

    //
    // Enable the uDMA controller.
    //
    uDMAEnable();

    //
    // Point at the control table to use for channel control structures.
    //
    uDMAControlBaseSet(pui8ControlTable);


    // Initialize the uDMA UART transfers.
    //
    InitUART0Transfer();



    //


    while(1)
    {
			  //
        // Turn on the LED.
        //
      GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, 1);

        //
        // Delay for a bit.
        //
        SysCtlDelay(g_ui32SysClock / 20 / 3);

        //
        // Turn off the LED.
        //
        GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, 0);

        //
        // Delay for a bit.
        //
        SysCtlDelay(g_ui32SysClock / 20 / 3);





    }
}
