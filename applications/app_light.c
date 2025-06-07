#include "app.h"
#include "ch.h"
#include "hal.h"

// Some useful includes
#include "mc_interface.h"
#include "utils_math.h"
#include "encoder/encoder.h"
#include "terminal.h"
#include "comm_can.h"
#include "hw.h"
#include "commands.h"
#include "timeout.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

typedef enum {
    APP_LIGHT_STATE_OFF = 0,
    APP_LIGHT_STATE_ON = 1,
    APP_LIGHT_STATE_LONG = 2
} AppLightState;

typedef enum {
    APP_LIGHT_TURN_OFF = 0,
    APP_LIGHT_TURN_LEFT = 1,
    APP_LIGHT_TURN_RIGHT = 2,
    APP_LIGHT_TURN_BOTH = 3,
} AppLightTurnState;

//static AppLightState light_state = APP_LIGHT_STATE_OFF;
static AppLightTurnState light_turn_state = APP_LIGHT_TURN_OFF;

void app_light_set(int state) {
    switch (state) {
        case APP_LIGHT_STATE_OFF:
            LIGHT_OFF();
            LIGHT_LONG_OFF();
            break;
        case APP_LIGHT_STATE_ON:
            LIGHT_ON();
            LIGHT_LONG_OFF();
            break;
        case APP_LIGHT_STATE_LONG:
            LIGHT_ON();
            LIGHT_LONG_ON();
            break;
    }
}

int app_light_get(void) {
    if (LIGHT_LONG_READ()) {
       return APP_LIGHT_STATE_LONG;
    } else if (LIGHT_READ()) {
        return APP_LIGHT_STATE_ON;
    }
    return APP_LIGHT_STATE_OFF;
}


void app_light_turn_set(int state) {
    light_turn_state = state;
}

int app_light_turn_get(void) {
    return light_turn_state;
}

// Threads
static THD_FUNCTION(light_thread, arg);
static THD_WORKING_AREA(light_thread_wa, 1024);

// Private functions
static void terminal_test(int argc, const char **argv);

// Private variables
static volatile bool stop_now = true;
static volatile bool is_running = false;

// Called when the light application is started. Start our
// threads here and set up callbacks.
void app_light_start(void) {

	stop_now = false;
	chThdCreateStatic(light_thread_wa, sizeof(light_thread_wa),
			NORMALPRIO, light_thread, NULL);

	// Terminal commands for the VESC Tool terminal can be registered.
	terminal_register_command_callback(
			"light_cmd",
			"Print the number d",
			"[d]",
			terminal_test);
}

// Called when the light application is stopped. Stop our threads
// and release callbacks.
void app_light_stop(void) {
	terminal_unregister_callback(terminal_test);

	stop_now = true;
	while (is_running) {
		chThdSleepMilliseconds(1);
	}
}

void app_light_configure(app_configuration *conf) {
	(void)conf;
}

static THD_FUNCTION(light_thread, arg) {
	(void)arg;

	chRegSetThreadName("App Light");

	is_running = true;

	// Example of using the experiment plot
//	chThdSleepMilliseconds(8000);
//	commands_init_plot("Sample", "Voltage");
//	commands_plot_add_graph("Temp Fet");
//	commands_plot_add_graph("Input Voltage");
//	float samp = 0.0;
//
//	for(;;) {
//		commands_plot_set_graph(0);
//		commands_send_plot_points(samp, mc_interface_temp_fet_filtered());
//		commands_plot_set_graph(1);
//		commands_send_plot_points(samp, GET_INPUT_VOLTAGE());
//		samp++;
//		chThdSleepMilliseconds(10);
//	}

	for(;;) {
		// Check if it is time to stop.
		if (stop_now) {
			is_running = false;
			return;
		}

		timeout_reset(); // Reset timeout if everything is OK.

		switch (light_turn_state)
        {
            case APP_LIGHT_TURN_OFF:
                TURN_LEFT_OFF();
                TURN_RIGHT_OFF();
                break;
            case APP_LIGHT_TURN_LEFT:
                if (TURN_LEFT_READ()) TURN_LEFT_OFF();
                else TURN_LEFT_ON();
                TURN_RIGHT_OFF();
                break;
            case APP_LIGHT_TURN_RIGHT:
                if (TURN_RIGHT_READ()) TURN_RIGHT_OFF();
                else TURN_RIGHT_ON();
                TURN_LEFT_OFF();
                break;
            case APP_LIGHT_TURN_BOTH:
                if (TURN_LEFT_READ()) {
                    TURN_LEFT_OFF();
                    TURN_RIGHT_OFF();
                }
                else {
                    TURN_LEFT_ON();
                    TURN_RIGHT_ON();
                }
                break;
            
        }

		chThdSleepMilliseconds(500);
	}
}

// Callback function for the terminal command with arguments.
static void terminal_test(int argc, const char **argv) {
	if (argc == 2) {
		int d = -1;
		sscanf(argv[1], "%d", &d);

		commands_printf("You have entered %d", d);

		// For example, read the ADC inputs on the COMM header.
		commands_printf("ADC1: %.2f V ADC2: %.2f V",
				(double)ADC_VOLTS(ADC_IND_EXT), (double)ADC_VOLTS(ADC_IND_EXT2));
	} else {
		commands_printf("This command requires one argument.\n");
	}
}
