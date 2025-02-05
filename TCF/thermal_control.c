#define _POSIX_C_SOURCE 199309L
#include "thermal_control.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

float thermistor_temps[4];
static float thermistor_setpoints[4] = {0, 0, 0, 0};
static float kp = 0.0, ki = 0.0, kd = 0.0; 
static int thermal_control_enabled = 0;
static float data_acquisition_frequency = 5.0; // Default frequency 5Hz

static float integral[4] = {0, 0, 0, 0};
static float previous_error[4] = {0, 0, 0, 0};

int set_thermistor_setpoint(int thermistor_id, float setpoint) {
    if (thermistor_id >= 0 && thermistor_id < 4 && setpoint >= -20 && setpoint <= 20) {
        thermistor_setpoints[thermistor_id] = setpoint;
        return 0;
    } else {
        fprintf(stderr, "Error: Invalid thermistor ID or setpoint value\n");
        return -1;
    }
}

void set_pid_parameters(float new_kp, float new_ki, float new_kd) {
    kp = new_kp;
    ki = new_ki;
    kd = new_kd;
}

void enable_thermal_control() {
    thermal_control_enabled = 1;
}

void disable_thermal_control() {
    thermal_control_enabled = 0;
}

void set_data_acquisition_frequency(float frequency) {
    if (frequency >= 1.0 && frequency <= 5.0) {
        data_acquisition_frequency = frequency;
    } else {
        fprintf(stderr, "Error: Invalid data acquisition frequency\n");
    }
}

void change_setpoint_temperature(int thermistor_id, float setpoint) {
    if (thermistor_id == -1) {
        // Change setpoint for all thermistors
        for (int i = 0; i < 4; i++) {
            set_thermistor_setpoint(i, setpoint);
        }
    } else {
        // Change setpoint for a specific thermistor
        set_thermistor_setpoint(thermistor_id, setpoint);
    }
}

void pid_controller(int heater_status[], int indice){
    float error = thermistor_setpoints[indice] - thermistor_temps[indice];
    integral[indice] += error;
    float derivative = error - previous_error[indice];
    float output = kp * error + ki * integral[indice] + kd * derivative;
    previous_error[indice] = error;
    if (output > 0) {
        heater_status[indice] = 1; // Turn heater ON
    } else {
        heater_status[indice] = 0; // Turn heater OFF
    }
}

int check_interval_temperatures(int indice){
    if((thermistor_temps[indice]<-25.0) || (thermistor_temps[indice]>25.0)){
        return 1; //bangbang
    }
    else return 0; //PID

}


void thermal_control_main(int read_pipe, int write_pipe) {
    char buffer[256];
    int heater_status[4] = {0, 0, 0, 0}; // All heaters OFF
    int cycle_interval_tc = 0; 
    struct timespec start_time, end_time, last_pid_time, sleep_time;
    long elapsed_time, pid_elapsed_time;

    // Initialize last_pid_time for the first time
    clock_gettime(CLOCK_MONOTONIC, &last_pid_time);
    
    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        cycle_interval_tc = 1000000 / data_acquisition_frequency; // Calculate interval based on frequency
        
        // Read temperature data from TSL
        if (read(read_pipe, buffer, sizeof(buffer)) == -1) {
            perror("read");
            continue;
        }

        // Parse temperature data
        if (sscanf(buffer, "%f-%d;%f-%d;%f-%d;%f-%d;",
            &thermistor_temps[0], &heater_status[0],
            &thermistor_temps[1], &heater_status[1],
            &thermistor_temps[2], &heater_status[2],
            &thermistor_temps[3], &heater_status[3]) != 8) {
            fprintf(stderr, "Error: Corrupted data received from TSL\n");
            continue;
        }

        // Get the current time to check if PID control should be performed
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        pid_elapsed_time = (end_time.tv_sec - last_pid_time.tv_sec) * 1000000 + (end_time.tv_nsec - last_pid_time.tv_nsec) / 1000;

        if (pid_elapsed_time >= cycle_interval_tc && thermal_control_enabled){
        // Perform thermal control using PID controller approach
            for (int i = 0; i < 4; i++) {
                if(check_interval_temperatures(i)==0) {
                    pid_controller(heater_status, i);
                }
                else{
                    if(thermistor_temps[i]>thermistor_setpoints[i]) heater_status[i]=0;
                    else heater_status[i]=1;
                }
            }
            // Update last_pid_time
            last_pid_time = end_time;
        }
        else if(thermal_control_enabled==0){
            for (int i = 0; i < 4; i++) {
                heater_status[i] = 0;
            }
        }

        // Format the heater status response
        snprintf(buffer, sizeof(buffer), "%d;%d;%d;%d;",
        heater_status[0], heater_status[1], heater_status[2], heater_status[3]);

        // Write heater status to RESPONSE_PIPE
        if (write(write_pipe, buffer, sizeof(buffer)) == -1) {
            perror("write");
        }
        
        // Get the end time
        clock_gettime(CLOCK_MONOTONIC, &end_time);

        // Calculate the elapsed time in microseconds
        elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000000 + (end_time.tv_nsec - start_time.tv_nsec) / 1000;

        // Sleep for the remaining time to maintain the cycle interval
        if (elapsed_time < 2000000) {
            sleep_time.tv_sec = 0;
            sleep_time.tv_nsec = (2000000 - elapsed_time) * 1000; // Convert microseconds to nanoseconds
            nanosleep(&sleep_time, NULL);
        }
    }
}
