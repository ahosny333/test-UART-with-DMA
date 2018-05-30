# simple project for EK-TM4C1294XL connected LaunchPad using UART with uDMA 



## project overview 

These instructions will give details on how to create simple project using TivaWare library and code composer studio to Initialize and use DMA with UART in EK-TM4C1294XL connected LaunchPa
We will communicate with the board through the UART, which is connected as a virtual serial port through the emulator USB connection.
in this project we will use uDMA controller to receive and transmit characters as the following :
we will use UART0 DMA RX channel to transfer data from terminal (putty) to a single buffer and then we will use UART0 DMA TX channel to transfer the same data from the buffer to the terminal (putty) 
we will select DMA RX interrupt to make the transfer re enabled every time . DMA RX interrupt is generated when complete transfer for UART0 DMA RX channel is done
we will add a visual indicator in the main function to show the role of DMA which allows peripherals to access the memory without aving to go through the processor
for more details about TIVA DMA it is sugeested to read this tutorial 
https://sites.google.com/site/luiselectronicprojects/tutorials/tiva-tutorials/tiva-dma/understanding-the-tiva-dma
 

### Prerequisites and kit initialization



```
 same as we did in test-UART-for-EK-TM4C1294XL-connected-LaunchPad project 
 please see that link
 https://github.com/ahosny333/test-UART-for-EK-TM4C1294XL-connected-LaunchPad-/blob/master/README.md
 
```



