# USB_keyboard_simulator
A user space simulator to demonstrate the interaction between a USB keyboard and the device driver.

Two processes are created,
1. Reads input file that contains key presses where,
    #:	no	key	event
    @:	CAPSLOCK press
    &:	CAPSLOCK	release
    and sends it to the driver process.
2. The driver process recognizes keypress as a callback function and dispatches the buffered data 
   to the input layer for key processing. Depending the pressed keys, appropriate responses 
   w.r.t CAPS lock LED update is handled.
   
The project presents an exact replica of the USB keyboard device interaction with the driver.


Below is the procedure to run the file:
-----------------------------------------------------------------------------------

Run the makefile to create object files for the respective .c files.

- To run makefile type "make" on the command line.

You'll see 1 object file created in the same directory.

-----------------------------------------------------------------------------------

- To run usbkbdSim, type "./usbkbdSim < input.txt"

- input.txt has the string of key presses.

- If output needs to be saved in a file, type "./usbkbdSim < input.txt > output.txt". 
	(output.txt is the filename where the output from the program is saved)
  
 - The output will contain the input string with appropriate upper and lower cases 
   w.r.t the CAPS lock key presses. The second line will contain the sequence of 
   CAPS lock LED status assuming CAPS lock is OFF at initialization.
