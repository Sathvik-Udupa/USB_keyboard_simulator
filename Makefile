# EEL4732/5733 Advanced System Programming
# Assignment 7 Makefile
# to run this, type $make 
# the input should be processed with input/output redirection.
#			./usbkbdSim < input.txt


all:	usbkbdSim

usbkbdSim: usbkbdSim.c
	gcc -o usbkbdSim usbkbdSim.c -lpthread
clean:
	rm -rf usbkbdSim
