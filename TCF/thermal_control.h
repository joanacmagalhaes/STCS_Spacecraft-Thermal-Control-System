#ifndef THERMAL_CONTROL_H
#define THERMAL_CONTROL_H

void thermal_control_main(int read_pipe, int write_pipe);
int set_thermistor_setpoint(int thermistor_id, float setpoint);
void set_pid_parameters(float kp, float ki, float kd);
void enable_thermal_control();
void disable_thermal_control();
void set_data_acquisition_frequency(float frequency);
void change_setpoint_temperature(int thermistor_id, float setpoint);
int check_interval_temperatures(int indice);
void pid_controller(int heater_status[], int indice);
void adjust_pid_parameters_ziegler_nichols();
void detect_oscillations(float current_time, float error);

#endif // THERMAL_CONTROL_H
