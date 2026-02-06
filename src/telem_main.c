/*
 ============================================================================
 Name        : telem_main.c
 Author      : VE2TCP Chris Thompson chrisethompson@gmail.com
 Version     : 1.0
 Copyright   : Copyright 2025 Chris Thompson
 Description : Example telemetry program for pacsat
 ============================================================================

 This program reads reads sensors, sends telemetry and writes the data to files.
 - The WOD telemetry file, which is appended until the file is rolled or a max size as
   a safety precaution.

 The collection period for the WOD file is in the header file.  Completing the file
 involves renaming the temporary file to its final name.  Usually this will be in a
 queue folder for ingestion into the pacsat directory.

 All telem files should be raw bytes suitable for reading back into a C structure

 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>
//#include <lgpio.h>
#include <pthread.h>

#include "pacsat_telem.h"
#include "sensor_telemetry.h"
#include "pacsat_log.h"
#include "agw_tnc.h"

#define MAX_FILE_PATH_LEN 256
#define ADC_O2_CHAN 2
#define ADC_METHANE_CHAN 0
#define ADC_AIR_QUALITY_CHAN 1
#define ADC_BUS_V_CHAN 3


int g_run_self_test;    /* true when the self test is running */
int g_verbose = 0;
char g_log_filename[MAX_FILE_PATH_LEN];
sensor_telemetry_t g_sensor_telemetry;

/* Forward functions */
void help(void);
void signal_exit (int sig);
void signal_load_config (int sig);
int read_sensors(uint32_t now);
double linear_interpolation(double x, double x0, double x1, double y0, double y1);
int tlm_send_time();
int tlm_send_sensor_telem();

/* Local Variables */
pthread_t tnc_listen_pthread;
int gpio_hd = -1;
float board_temperature = 0.0;
char data_folder_path[MAX_FILE_PATH_LEN] = "/tmp";

sensor_telemetry_t g_sensor_telemetry;

time_t last_time_checked_wod = 0;
time_t last_time_checked_period_to_sample_telem = 0;

int g_num_of_file_io_errors = 0; // the cumulative number of file io errors


int main(int argc, char *argv[]) {
	signal (SIGQUIT, signal_exit);
	signal (SIGTERM, signal_exit);
	signal (SIGHUP, signal_load_config);
	signal (SIGINT, signal_exit);

	struct option long_option[] = {
			{"help", no_argument, NULL, 'h'},
			{"dir", required_argument, NULL, 'd'},
			{"verbose", no_argument, NULL, 'v'},
			{NULL, 0, NULL, 0},
	};

	int more_help = false;

	while (1) {
		int c;
		if ((c = getopt_long(argc, argv, "hd:v", long_option, NULL)) < 0)
			break;
		switch (c) {
		case 'h': // help
			more_help = true;
			break;
		case 'v': // verbose
			g_verbose = true;
			break;
		case 'd': // data folder
			strlcpy(data_folder_path, optarg, sizeof(data_folder_path));
			break;

		default:
			break;
		}
	}

	if (more_help) {
		help();
		return 0;
	}

	char wod_telem_path[MAX_FILE_PATH_LEN];
	strlcpy(wod_telem_path, data_folder_path,MAX_FILE_PATH_LEN);
	strlcat(wod_telem_path,"/",MAX_FILE_PATH_LEN);
	strlcat(wod_telem_path,WOD_PATH,MAX_FILE_PATH_LEN);


	if (g_verbose) {
		printf("Example Pacsat Telemetry Capture\n");
	}

	/**
		 * Start a thread to listen to the TNC.  This will write all received frames into
		 * a circular buffer.  This thread runs in the background and is always ready to
		 * receive data from the TNC.
		 *
		 * The receive loop reads frames from the buffer and processes
		 * them when we have time.
		 *
		 */
		char *name = "TNC PACSAT Listen Thread";
		int rc = pthread_create( &tnc_listen_pthread, NULL, tnc_listen_process, (void*) name);
		if (rc != EXIT_SUCCESS) {
			error_print("FATAL. Could not start the TNC listen thread.\n");
			exit(rc);
		}

		sleep(3); // Let TNC Connect


//	gpio_hd = sensors_gpio_init();

	/* Now read the sensors until we get an interrupt to exit */
	time_t now = time(0);
	last_time_checked_wod = now;

	while (1) {
		now = time(0);

		if (PERIOD_TO_SAMPLE_TELEM_IN_SECONDS > 0) {

			if (PERIOD_TO_STORE_WOD_IN_SECONDS > 0) { /* Then WOD is enabled */
				if ((now - last_time_checked_wod) > PERIOD_TO_STORE_WOD_IN_SECONDS) {
					last_time_checked_wod = now;

					long size = log_append(wod_telem_path,(unsigned char *)&g_sensor_telemetry, sizeof(g_sensor_telemetry));
					if (size < sizeof(g_sensor_telemetry)) {
						if (g_verbose)
							printf("ERROR, could not save data to filename: %s\n",wod_telem_path);
						g_num_of_file_io_errors++;
					} else {
						if (g_verbose)
							printf("Wrote WOD file: %s at %d\n",wod_telem_path, g_sensor_telemetry.timestamp);
					}

					/* If we have exceeded the WOD size threshold then roll the WOD file */
					if (size/1024 > MAX_WOD_FILE_SIZE_IN_KB) {
						debug_print("Rolling SENSOR WOD file as it is: %.1f KB\n", size/1024.0);
						log_add_to_directory(wod_telem_path);
					}

				}
			} /* If Wod is enabled */

			if ((now - last_time_checked_period_to_sample_telem) > PERIOD_TO_SAMPLE_TELEM_IN_SECONDS) {
				last_time_checked_period_to_sample_telem = now;

				read_sensors(now);

				/* Put in latest data from the CosmicWatches if we have it */

				tlm_send_sensor_telem();
			} /* if time to sample sensors */
		} /* if sensors enabled */


		if (g_num_of_file_io_errors > MAX_NUMBER_FILE_IO_ERRORS) {
			printf("ERROR: Too many file io/errors.  Exiting\n");
			signal_exit(0);
		}

	} /* while (1) */
}

/**
 * Print this help if the -h or --help command line options are used
 */
void help(void) {
	printf(
			"Usage: sensors [OPTION]... \n"
			"-h,--help                        help\n"
			"-d,--dir                         use this data directory, rather than default\n"
			"-v,--verbose                     print additional status and progress messages\n"
	);
	exit(EXIT_SUCCESS);
}


void signal_exit (int sig) {
	if(g_verbose && sig > 0)
		printf (" Signal received, exiting ...\n");
//	lguSleep(2/1000);
	usleep(2/1000);
	
	exit (0);
}

void signal_load_config (int sig) {

}


int read_sensors(uint32_t now) {
	g_sensor_telemetry.timestamp = now;
	/* This is where you would the PI sensors */

		g_sensor_telemetry.SHTC3_temp = 11;
		g_sensor_telemetry.SHTC3_humidity = 55;
		g_sensor_telemetry.TempHumidityValid = 1;
		g_sensor_telemetry.LPS22_pressure = 66;
		g_sensor_telemetry.LPS22_temp = 22;
		g_sensor_telemetry.PressureValid = 1;

	return EXIT_SUCCESS;
}

/**
 * Standard algorithm for straight line interpolation
 * @param x - the key we want to find the value for
 * @param x0 - lower key
 * @param x1 - higher key
 * @param y0
 * @param y1
 * @return
 */
double linear_interpolation(double x, double x0, double x1, double y0, double y1) {
	double y = y0 + (y1 - y0) * ((x - x0)/(x1 - x0));
	return y;
}

int tlm_send_time() {
	int rc = EXIT_SUCCESS;
	char status[4];
	time_t now = time(0);
	status[0] = now & 0xff;
	status[1] = (now >> 8) & 0xff;
	status[2] = (now >> 16) & 0xff;
	status[3] = (now >> 24) & 0xff;
	rc = send_raw_packet(BROADCAST_CALLSIGN, TIME_CALL, PID_NO_PROTOCOL, (unsigned char *)status, sizeof(status));

	return rc;
}

int tlm_send_sensor_telem() {
	int rc = EXIT_SUCCESS;

	debug_print("Sending Sensor Telem: %d\n", g_sensor_telemetry.timestamp  );
	rc = send_raw_packet(BROADCAST_CALLSIGN, TELEM_TYPE_1_CALL, PID_NO_PROTOCOL, (unsigned char *)&g_sensor_telemetry, sizeof(g_sensor_telemetry));

	return rc;
}

