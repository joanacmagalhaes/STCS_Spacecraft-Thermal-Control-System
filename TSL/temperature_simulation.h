#ifndef TEMPERATURE_SIMULATION_H
#define TEMPERATURE_SIMULATION_H

void temperature_simulation_main(int write_pipe, int read_pipe);
float get_thermistor_temperature(int thermistor_id);
int get_heater_status(int heater_id);

#endif // TEMPERATURE_SIMULATION_H
