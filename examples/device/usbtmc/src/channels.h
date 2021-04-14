#include <stdio.h>
#include "instrument_types.h"

extern const int32_t channel_count;  // Total number of channels
extern const char * channel_names[]; // The name of the channels printed on the board
extern channel_status_t channel_status[]; // Store the on or off status of the pins
extern trigger_setting_t trigger_setting[]; // Store the trigger setting of the pins
extern uint32_t sample_rate; // Sampling rate
extern uint32_t samples; // Number of samples to be measured
