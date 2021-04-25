#ifndef MAIN_H
#define MAIN_H
void led_indicator_pulse(void);

void led_blinking_task(void);
void tlf_fifo_task(void);
void tlf_fifo_init(void);
void tlf_send_buffer(void);

#endif
