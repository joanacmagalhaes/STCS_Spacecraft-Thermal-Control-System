#define _POSIX_C_SOURCE 199309L
#include "temperature_simulation.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

static float thermistor_temps[4];
static int heater_status[4] = {0, 0, 0, 0}; // All heaters OFF
static FILE *log_file;

void log_simulation_data(uint16_t monotonic_clock, const char *current_period) { //function to log simulation data on the csv file
    fprintf(log_file, "%u, %s, %.1f, %.1f, %.1f, %.1f, %d, %d, %d, %d\n",
            monotonic_clock, current_period,
            thermistor_temps[0], thermistor_temps[1], thermistor_temps[2], thermistor_temps[3],
            heater_status[0], heater_status[1], heater_status[2], heater_status[3]);
    fflush(log_file);
}

void temperature_simulation_main(int write_pipe, int read_pipe) {
    char buffer[256];
    const char *current_period = "NORMAL";
    uint16_t monotonic_clock = 0;
    const int cycle_interval_tsl = 200000; // 5Hz -> 200ms per cycle
    struct timespec start_time, end_time, sleep_time;
    long elapsed_time;

    // Initialize random number generator
    srand(time(NULL));

    // Initialize thermistor temperatures with random values between -5 and 7
    for (int i = 0; i < 4; i++) {
        thermistor_temps[i] = (rand() % 13) - 5; // Random value between -5 and 7
    }

    // Open log file
    log_file = fopen("simulation_log.csv", "w");
    if (log_file == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    fprintf(log_file, "Clock, Period, Thermistor1, Thermistor2, Thermistor3, Thermistor4, Heater1, Heater2, Heater3, Heater4\n");

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        // Increment monotonic clock
        monotonic_clock++;

        // Determine the current period based on the lower 8 bits of the monotonic clock
        uint8_t clock_lower_8_bits = monotonic_clock & 0xFF;
        if (clock_lower_8_bits <= 0x1F || clock_lower_8_bits >= 0x60) {
            current_period = "NORMAL";
        } else if (clock_lower_8_bits >= 0x20 && clock_lower_8_bits <= 0x3F) {
            current_period = "ECLIPSE";
        } else if (clock_lower_8_bits >= 0x40 && clock_lower_8_bits <= 0x5F) {
            current_period = "SUN_EXPOSURE";
        }

        // Update thermistor temperatures based on the current period and heater status
        for (int i = 0; i < 4; i++) {
            if (heater_status[i]) {
                if (current_period == "NORMAL") {
                    thermistor_temps[i] += 1;
                } else if (current_period == "ECLIPSE") {
                    thermistor_temps[i] += 4;
                } else if (current_period == "SUN_EXPOSURE") {
                    thermistor_temps[i] += 7;
                }
            } else {
                if (current_period == "NORMAL") {
                    thermistor_temps[i] -= 1;
                } else if (current_period == "ECLIPSE") {
                    thermistor_temps[i] -= 7;
                } else if (current_period == "SUN_EXPOSURE") {
                    thermistor_temps[i] -= 1;
                }
            }
        }

        // Log simulation data on the csv file
        log_simulation_data(monotonic_clock, current_period);

        // Format the temperature data
        snprintf(buffer, sizeof(buffer), "%.1f-%d;%.1f-%d;%.1f-%d;%.1f-%d;",
                 thermistor_temps[0], heater_status[0],
                 thermistor_temps[1], heater_status[1],
                 thermistor_temps[2], heater_status[2],
                 thermistor_temps[3], heater_status[3]);

        // Write temperature data to INFO_PIPE
        if (write(write_pipe, buffer, sizeof(buffer)) == -1) {
            perror("write");
            fprintf(log_file, "Error: Failed to write to INFO_PIPE\n");
            fflush(log_file);
        }

        // Read response from TCF
        if (read(read_pipe, buffer, sizeof(buffer)) == -1) {
            perror("read");
            fprintf(log_file, "Error: Failed to read from RESPONSE_PIPE\n");
            fflush(log_file);
        } else {
            // Update heater status based on TCF response
            if (sscanf(buffer, "%d;%d;%d;%d;", &heater_status[0], &heater_status[1], &heater_status[2], &heater_status[3]) != 4) {
                fprintf(stderr, "Error: Corrupted data received from TCF\n");
                fprintf(log_file, "Error: Corrupted data received from TCF\n");
                fflush(log_file);
            }
        }

        // Sleep for the cycle interval
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        // Calculate the elapsed time in microseconds
        elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000000 + (end_time.tv_nsec - start_time.tv_nsec) / 1000;
        // Sleep for the remaining time to maintain the cycle interval
        if (elapsed_time < cycle_interval_tsl) {
            sleep_time.tv_sec = 0;
            sleep_time.tv_nsec = (cycle_interval_tsl - elapsed_time) * 1000; // Convert microseconds to nanoseconds
            nanosleep(&sleep_time, NULL);
        }
    }
    // Close log file
    fclose(log_file);
}