#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "TCF/thermal_control.h"
#include "TSL/temperature_simulation.h"
#include <pthread.h>
#include <fcntl.h>

#define INFO_PIPE 0
#define RESPONSE_PIPE 1

void get_pid_parameters(float *kp, float *ki, float *kd) {
    char choice;
    printf("Do you want to use default PID parameters? (y/n): ");
    scanf(" %c", &choice);
    if(choice == 'y' || choice == 'Y') {
        *kp =1.0;  //6.0; 
        *ki = 0.1; //0.7; 
        *kd = 0.01; //2.0;
        return;
    }
    else{
        printf("Enter PID parameters:\n");
        printf("Proportional gain (Kp): ");
        scanf("%f", kp);
        printf("Integral gain (Ki): ");
        scanf("%f", ki);
        printf("Derivative gain (Kd): ");
        scanf("%f", kd);
    }
}

void get_setPoint() {
    char choice;
    printf("Do you want to use default setPoint parameters? (y/n): ");
    scanf(" %c", &choice);
    if(choice == 'y' || choice == 'Y') {
       change_setpoint_temperature(-1, 0.0); // Change setpoint for all thermistors to 0.0
        return;
    }
    else{
        float setpoint;
        while(1){
            printf("Enter setPoint parameters for the first Themistor: \n");
            scanf("%f", &setpoint);
            if(set_thermistor_setpoint(0, setpoint)==0) break;
        }
        while(1){
            printf("Enter setPoint parameters for the second Themistor: \n");
            scanf("%f", &setpoint);
            if(set_thermistor_setpoint(1, setpoint)==0) break;
        }
        while(1){
            printf("Enter setPoint parameters for the third Themistor: \n");
            scanf("%f", &setpoint);
            if(set_thermistor_setpoint(2, setpoint)==0) break;
        }
        while(1){
            printf("Enter setPoint parameters for the fourth Themistor: \n");
            scanf("%f", &setpoint);
            if(set_thermistor_setpoint(3, setpoint)==0) break;
        }
    }
}

void* runtime_menu(void* arg) {
    int choice;
    char choiceDisable;
    float value;
    int state=1;
    while (1) {
        printf("\nRuntime Menu:\n");
        printf("1. Change thermistor setpoint\n");
        printf("2. Change data acquisition frequency from TCF\n");
        if(state==1)printf("3. Disable thermal control\n");
        else printf("3. Enable thermal control\n");
        printf("4. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                printf("Enter thermistor(0-3) ID (-1 for all) space and new setpoint: (ex. 1 10) "); 
                int thermistor_id;
                scanf("%d %f", &thermistor_id, &value);
                if(thermistor_id < -1 || thermistor_id > 3 ) { //ver letras
                    printf("Invalid thermistor ID. Please try again.\n");
                    break;
                }
                change_setpoint_temperature(thermistor_id, value);
                break;
            case 2:
                printf("Enter new data acquisition frequency: ");
                scanf("%f", &value);
                set_data_acquisition_frequency(value);
                break;
            case 3:
                if(state==1){
                    printf("Thermal control will stop and the temperatures will plummet.\n");
                    printf("Are you sure you want to go ahead? (y/n)");
                    scanf(" %c", &choiceDisable);
                    if(choiceDisable=='y' || choiceDisable=='Y'){
                        disable_thermal_control();
                        state=0;
                    }
                }
                else if(state==0) {
                    enable_thermal_control();
                    state=1;
                }
                break;
            case 4:
                exit(0);
            default:
                printf("Invalid choice. Please try again.\n");
        }
    }
    return NULL;
}

int main() {
    int info_pipe[2], response_pipe[2];
    pid_t pid;
    float kp, ki, kd;
    pthread_t menu_thread;

    get_pid_parameters(&kp, &ki, &kd);

    if (pipe(info_pipe) == -1 || pipe(response_pipe) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    
    // Set PID parameters and setpoints before starting the simulation
    set_pid_parameters(kp, ki, kd);
    get_setPoint();

    // Set data acquisition frequency
    set_data_acquisition_frequency(5.0); // Example frequency

    if (pthread_create(&menu_thread, NULL, runtime_menu, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Child process: TSL
        close(info_pipe[INFO_PIPE]);
        close(response_pipe[RESPONSE_PIPE]);
        
        // Call TSL main function
        temperature_simulation_main(info_pipe[RESPONSE_PIPE], response_pipe[INFO_PIPE]);
       
    } else {
        // Parent process: TCF
        close(info_pipe[RESPONSE_PIPE]);
        close(response_pipe[INFO_PIPE]);
        
        // Enable thermal control
        enable_thermal_control();
        // Call TCF main function
        thermal_control_main(info_pipe[INFO_PIPE], response_pipe[RESPONSE_PIPE]);
        
        wait(NULL); // Wait for child process to finish

    }

    return 0;
}
