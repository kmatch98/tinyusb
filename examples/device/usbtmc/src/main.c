/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"
#include "usbtmc_app.h"
#include "scpi_parser.h"
#include "logic_capture.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 0 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 0,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);

/*------------- MAIN -------------*/
int main(void)
{

  // static uint32_t start_ms = 0;
  static bool led_state = true;

  board_init();
  tusb_init();


  board_led_write(led_state);
  // led_state = 1 - led_state; // toggle

  scpi_init();

  logic_capture_init();

  while (1)
  {
    tud_task(); // tinyusb device task
    // led_blinking_task();
    usbtmc_app_task_iter();

    // // Blink every interval ms
    // if ( (board_millis() - start_ms) > blink_interval_ms) { // time has elapsed
    // //if ( (board_millis() - start_ms) > blink_interval_ms) { // time has elapsed
    //   start_ms += blink_interval_ms;

    //   board_led_write(led_state);
    //   led_state = 1 - led_state; // toggle
    // }
  }

  return 0;
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}



//--------------------------------------------------------------------+
// TinyLogicFriend communication
//--------------------------------------------------------------------+

static const uint32_t tlf_data_buffer_length=64;
static uint8_t tlf_data_buffer[64];
static uint32_t tlf_data_buffer_count=0;

// static uint32_t counter=0;

void tlf_queue_data(uint16_t value, uint32_t timestamp) {

    // store timestamp as 16 bit
    tlf_data_buffer[tlf_data_buffer_count]   = (uint16_t) timestamp;
    tlf_data_buffer[tlf_data_buffer_count+2] = (uint16_t) value;
    tlf_data_buffer_count += 4;

    if (tlf_data_buffer_count > tlf_data_buffer_length - 4) {
      // tud_usbtmc_transmit_dev_msg_data(tlf_data_buffer, tlf_data_buffer_count, (counter > 5), false);  // this is a way to end the transmission
      tud_usbtmc_transmit_dev_msg_data(tlf_data_buffer, tlf_data_buffer_count, false, false);
      tlf_data_buffer_count=0;
      // counter += 1;
    }

    // // trial with 32 bit timestamp
    // tlf_data_buffer[tlf_data_buffer_count]   = timestamp;
    // tlf_data_buffer[tlf_data_buffer_count+4] = (uint16_t) value;
    // tlf_data_buffer_count += 6;

    // if (tlf_data_buffer_count > tlf_data_buffer_length - 6) {
    //   // tud_usbtmc_transmit_dev_msg_data(tlf_data_buffer, tlf_data_buffer_count, (counter > 5), false);  // this is a way to end the transmission
    //   tud_usbtmc_transmit_dev_msg_data(tlf_data_buffer, tlf_data_buffer_count, false, false);
    //   tlf_data_buffer_count=0;
    //   // counter += 1;
    // }

}


void tlf_queue_sample(uint8_t* sample, uint32_t sample_len) {

    // this sends one measurement packet at a time.  This is totally inefficient
    // it should use a buffer and then only transmit with the third
    // argument (endOfMessage) when the buffer is full
    //
    // Is there enough time to check for full buffer and send?
    //
    // TODO ** clean this up

    tud_usbtmc_transmit_dev_msg_data(sample, sample_len, false, false);

    // (void) sample;
    // (void) sample_len;

    // char test[]="test";
    // tud_usbtmc_transmit_dev_msg_data(test, 4, false, false);


    //tud_cdc_write(sample, sample_len);

    // if (sent == 0) {
    //   asm("bkpt");
    // }
}


//--------------------------------------------------------------------+
// BLINKING TASK + Indicator pulse
//--------------------------------------------------------------------+


volatile uint8_t doPulse = false;
// called from USB context
void led_indicator_pulse(void) {
	doPulse = true;
}

void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;
  if(blink_interval_ms == BLINK_MOUNTED) // Mounted
  {
    if(doPulse)
    {
      led_state = true;
      board_led_write(true);
      start_ms = board_millis();
      doPulse = false;
    }
    else if (led_state == true)
    {
      if ( board_millis() - start_ms < 750) //Spec says blink must be between 500 and 1000 ms.
      {
        return; // not enough time
      }
      led_state = false;
      board_led_write(false);
    }
  }
  else
  {
    // Blink every interval ms
    if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
    start_ms += blink_interval_ms;

    board_led_write(led_state);
    led_state = 1 - led_state; // toggle
  }
}
