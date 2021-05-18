/*
File:   usbkbdSim.c
Author: Sathvik Subramanya Udupa
Date:   4/22/2021
Desc:   USB keyboard simulator for better 
		understanding of the usb keyboard 
		device driver.
    
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <errno.h>
extern int errno;

//Type of urb
#define IRQ 1
#define LED 2

//define variable to indicate threads to close pipes
#define DONE 3

//status of LEDs
#define ON 1
#define OFF 0

//struct for led buffer to be sent to keyboard
struct kbd{
	int led;
	int new_led;
	pthread_mutex_t mtx;
	bool led_submitted;
};

struct kbd *kbd_buf;

//mmaped memory
struct map{
	int led_status;
};

struct map *map_buf;

//variable,lock and cond variable for irq_urb to wait for 
//callback to resubmit irq urb
pthread_mutex_t irq_mtx;
pthread_cond_t irq_cond;
int rd_nxt_flag = 0;

//variable,lock and cond variable for irq_urb to wait for 
//callback to resubmit irq urb
pthread_mutex_t led_mtx;
pthread_cond_t led_cond;
int send_ctrl = 0;

//create pipes for IPC
int irq_pipe[2];
int ack_pipe[2];
int ctrl_pipe[2];


//This function takes type of urb as parameters and sends the info to respective endpoints
int usb_submit_urb(int endpoint)
{
	int status;

	switch (endpoint)
	{
		case IRQ: 

			status = pthread_mutex_lock(&irq_mtx);
			if(status) printf("URB: Errror locking irq_mtx\n");

			rd_nxt_flag = 1;

			status = pthread_cond_signal(&irq_cond);
			if(status) printf("URB: Errror signalling go\n");

			status = pthread_mutex_unlock(&irq_mtx);
			if(status) printf("URB: Errror locking irq_mtx\n");

			break;

		case LED:

			status = pthread_mutex_lock(&led_mtx);
			if(status) printf("URB: Errror locking led_mtx\n");

			send_ctrl = 1;

			status = pthread_cond_signal(&led_cond);
			if(status) printf("URB: Errror signalling led go\n");

			status = pthread_mutex_unlock(&led_mtx);
			if(status) printf("URB: Errror locking led_mtx\n");

			break;

		case DONE:

			status = pthread_mutex_lock(&led_mtx);
			if(status) printf("URB: Errror locking led_mtx\n");

			send_ctrl = DONE;

			status = pthread_cond_signal(&led_cond);
			if(status) printf("URB: Errror signalling led go\n");

			status = pthread_mutex_unlock(&led_mtx);
			if(status) printf("URB: Errror locking led_mtx\n");

			break;

		default: 

			printf("Unknown arguments for usb_submit_urb\n");
			break;
	}

	return 0;
}


//This function sets new led values and triggers a ctrl urb to set led in keybooard
void* usb_kbd_event(void* buf)
{
	char led_val = (char) buf;
	int status;

	pthread_mutex_lock(&kbd_buf->mtx);
	if(status) printf("Errror locking event mtx\n");

	kbd_buf->new_led = (led_val[0] == '@') ? 1 : 0;

	if(kbd_buf->led_submitted)
	{		
		pthread_mutex_unlock(&kbd_buf->mtx);
		if(status) printf("Errror unlocking event mtx\n");
		return NULL;
	}

	if(kbd_buf->led == kbd_buf->new_led)
	{		
		pthread_mutex_unlock(&kbd_buf->mtx);
		if(status) printf("Errror unlocking event mtx\n");
		return NULL;
	}

	kbd_buf->led = kbd_buf->new_led;
	map_buf->led_status = kbd_buf->new_led;

	kbd_buf->led_submitted = true;
	usb_submit_urb(LED);

	pthread_mutex_unlock(&kbd_buf->mtx);
	if(status) printf("Errror unlocking event mtx\n");
}


//This function reports the pressed key and starts an event if CAPSLOCK is pressed
int input_report_key(char *buf)
{
	static int caps = 0;
	pthread_t usb_kbd_event_id;
	char p = buf[0];

	switch (p)
	{
		case '@':

			if(caps) caps = 0;
			else caps = 1;

			pthread_create(&usb_kbd_event_id, NULL, usb_kbd_event, buf);
			pthread_join(usb_kbd_event_id, NULL);
			break;

		case '&':

			pthread_create(&usb_kbd_event_id, NULL, usb_kbd_event, buf);
			pthread_join(usb_kbd_event_id, NULL);
			break;

		default:

			if(caps)
			{
				if(p >= 'a' && p <= 'z')
				{
					printf("%c", p-32);
				}
				else if (p >= 'A' && p <= 'Z')
				{
					printf("%c", p+32);
				}
				else printf("%c", p);
			}
			else printf("%c", p);
			break;
	}

	return 0;
}


//callback thread for irq urb submit
void usb_kbd_led(void temp)
{
	int status;

	pthread_mutex_lock(&kbd_buf->mtx);
	if(status) printf("Errror locking kbd_led mtx\n");

	if(kbd_buf->led == kbd_buf->new_led)
	{
		kbd_buf->led_submitted = false;		
		pthread_mutex_unlock(&kbd_buf->mtx);
		if(status) printf("Errror unlocking kbd_led mtx\n");
		return NULL;
	}

	kbd_buf->led = kbd_buf->new_led;
	map_buf->led_status = kbd_buf->new_led;

	usb_submit_urb(LED);

	pthread_mutex_unlock(&kbd_buf->mtx);
	if(status) printf("Errror unlocking kbd_led mtx\n");	
} 


//callback thread for irq urb submit
void usb_kbd_irq(void buf)
{
	input_report_key((char*) buf);
	usb_submit_urb(IRQ);
} 


//thread to handle usb core functionality that interacts with irq endpoint
void irq_thread(void temp)
{
	int status;
	char buf[1];
	pthread_t usb_kbd_irq_id;

	while(1)
	{
		status = pthread_mutex_lock(&irq_mtx);
		if(status) printf("Errror locking thread irq_mtx\n");

		while(rd_nxt_flag == 0)
		{
			pthread_cond_wait(&irq_cond, &irq_mtx);
		}

		status = pthread_mutex_unlock(&irq_mtx);
		if(status) printf("Errror locking thread irq_mtx\n");

		if(read(irq_pipe[0], buf, 1) == 0) break;
		
		if (buf[0] != '#')
		{
			status = pthread_mutex_lock(&irq_mtx);
			if(status) printf("Errror locking thread irq_mtx\n");

			rd_nxt_flag = 0;

			status = pthread_mutex_unlock(&irq_mtx);
			if(status) printf("Errror locking thread irq_mtx\n");

			pthread_create(&usb_kbd_irq_id, NULL, usb_kbd_irq, buf);
		}
	}
}


//thread to handle usb core functionality that interacts with ctrl endpoint
void led_thread(void temp)
{
	int status;
	char out_buf = 'C';
	char *in_buf;
	pthread_t usb_kbd_led_id;

	while(1)
	{
		status = pthread_mutex_lock(&led_mtx);
		if(status) printf("Errror locking thread led_mtx\n");

		while(send_ctrl == 0)
		{
			pthread_cond_wait(&led_cond, &led_mtx);
		}

		if(send_ctrl == DONE)
		{
			status = pthread_mutex_unlock(&led_mtx);
			if(status) printf("Errror locking thread led_mtx\n");
			close(ctrl_pipe[1]);
			break;
		}

		write(ctrl_pipe[1], &out_buf, 1);

		if(read(ack_pipe[0], in_buf, 1) == 0) break;

		send_ctrl = 0;

		status = pthread_mutex_unlock(&led_mtx);
		if(status) printf("Errror locking thread led_mtx\n");

		pthread_create(&usb_kbd_led_id, NULL, usb_kbd_led, NULL);

	}
}


//opening keyboard. This is done after all initializations are completed
int usb_kbd_open(void)
{	
	int status;
	pthread_t irq_id, led_id;
	pthread_create(&irq_id, NULL, irq_thread, NULL);
	pthread_create(&led_id, NULL, led_thread, NULL);

	usb_submit_urb(IRQ);

	pthread_join(irq_id, NULL);
	fflush(stdout);

	while(1)
	{	
		pthread_mutex_lock(&kbd_buf->mtx);
		if(status) printf("Errror locking open mtx\n");

		if(kbd_buf->led_submitted)
		{
			pthread_mutex_unlock(&kbd_buf->mtx);
			if(status) printf("Errror unlocking open mtx\n");
		}
		else
		{
			usb_submit_urb(DONE);
			pthread_mutex_unlock(&kbd_buf->mtx);
			if(status) printf("Errror unlocking open mtx\n");
			break;
		}
	}
	pthread_join(led_id, NULL);
	
	return 0;
}


//function to simulate keypress and act as irq endpoint
void* key_thread(void* temp)
{
	char *buf = malloc(1024*sizeof(char));
	fgets(buf, 1024, stdin);
	write(irq_pipe[1], buf, strlen(buf));
	close(irq_pipe[1]);
	free(buf);
}

//thread to act as ctrl endpoint
void* caps_thread(void* temp)
{
	int status;
	char buf[1];
	int cur_led = 0;
	int count = 0;
	int errnum;
	while(1)
	{
		if(read(ctrl_pipe[0], buf, 1) == 0) break;

		if ((cur_led == 1) && (map_buf->led_status == 0))
		{
			count++;
		}
		cur_led = map_buf->led_status;

		write(ack_pipe[1], buf, 1);
	}

	close(ack_pipe[1]);
	printf("ON");

	for(int i = 1; i<count; i++)
	{
		if(i%2 == 1) printf(" OFF");
		else printf(" ON");
	}
	printf("\n");
}


//keyboard simulation
void keyboard(void)
{
	pthread_t key_id, caps_id;
	pthread_create(&key_id, NULL, key_thread, NULL);
	pthread_create(&caps_id, NULL, caps_thread, NULL);

	pthread_join(key_id, NULL);
	pthread_join(caps_id, NULL);
}


//main function
int main(int argc, char* argv[])
{	

	//initialize all variables

	map_buf = mmap(NULL, sizeof(struct map), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (map_buf == NULL)
	{
		printf("Error with mmap\n");
		return 1;
	}

	kbd_buf = malloc(sizeof(struct kbd));
	if (kbd_buf == NULL)
	{
		printf("Error with mmap\n");
		return 1;
	}

	pthread_mutex_init(&kbd_buf->mtx, NULL);
	kbd_buf->led = 0;
	kbd_buf->new_led = 0;
	kbd_buf->led_submitted = false;

	pthread_mutex_init(&irq_mtx, NULL);
	pthread_mutex_init(&led_mtx, NULL);

	pthread_cond_init(&irq_cond, NULL);
	pthread_cond_init(&led_cond, NULL);

	if ((pipe(irq_pipe) == -1) || (pipe(ack_pipe) == -1) || (pipe(ctrl_pipe) == -1))
	{
		printf("pipe\n");
		exit(1);
	}

	//call fork to create keyboard process

	switch(fork())
	{
		case -1:
		printf("fork unsucessfull\n");
		exit(1);

		case 0:
		if ((close(irq_pipe[0]) == -1) || (close(ack_pipe[0]) == -1) || (close(ctrl_pipe[1]) == -1))                  
        { 
        	printf("close pipe ends in child failed\n");
        	exit(1);
        }		
		keyboard();
		exit(0);

		default:
		if ((close(irq_pipe[1]) == -1) || (close(ack_pipe[1]) == -1) || (close(ctrl_pipe[0]) == -1))                  
        { 
        	printf("close pipe ends in parent failed\n");
        	exit(1);
        }
		break;
	}

	usb_kbd_open();

	if(wait(NULL)==-1)
    {
    	printf("wait failed 1\n");
    	exit(1);
    }

    munmap(map_buf, sizeof(struct map));
    free(kbd_buf);

    return 0;
}