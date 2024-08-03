#include <stdlib.h>
#include <stdio.h>
#include "vl53l0x_api.h"
#include "vl53l0x_platform.h"

#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/time.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define wheel_circumference 10
#define BUZZER_PIN 24  // GPIO pin where the buzzer is connected

#define VERSION_REQUIRED_MAJOR 1
#define VERSION_REQUIRED_MINOR 0
#define VERSION_REQUIRED_BUILD 1

///////////////////////////////////////////////////////////////////////////////////////////////////
/*                                   MEASUREMENT FUNCTIONS                                       */
///////////////////////////////////////////////////////////////////////////////////////////////////
unsigned long start_time;
unsigned long current_time;
unsigned long last_measurement_time = 0;
unsigned long measurement_time = 0;
bool running = false;
bool buzzer_on_local = false;
bool buzzer_on_external = false;
int measurements = 0;
pthread_mutex_t mutex;
bool is_detecting_spoke = false;
int connected_socket = 0;
int server_socket = 0, client_socket = 0;

unsigned long get_current_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void print_measurement() {
    pthread_mutex_lock(&mutex);
    printf("Last measurement: %lu\t Current measurement: %lu\t Time between: %lu\n",
           last_measurement_time, measurement_time, measurement_time - last_measurement_time);
    pthread_mutex_unlock(&mutex);
}

void record_measurement() {
    pthread_mutex_lock(&mutex);
    last_measurement_time = measurement_time;
    measurement_time = current_time - start_time;
    measurements++;
    pthread_mutex_unlock(&mutex);
}

int get_measurements() {
    int measurement_ = 0;
    pthread_mutex_lock(&mutex);
    measurement_ = measurements;
    pthread_mutex_unlock(&mutex);
    return measurement_;
}

float calculate_speed_cm_s() {
    if (measurements < 1)
        return 0;
    pthread_mutex_lock(&mutex);
    unsigned long current_measurement_difference = (current_time - start_time) - measurement_time;
    unsigned long difference_between_measurements = measurement_time - last_measurement_time;
    pthread_mutex_unlock(&mutex);

    if (current_measurement_difference > difference_between_measurements) {
        return (float)wheel_circumference / ((float)current_measurement_difference / 1000);
    } else {
        return (float)wheel_circumference / ((float)difference_between_measurements / 1000);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/*                                    BUZZER FUNCTIONALITY                                       */
///////////////////////////////////////////////////////////////////////////////////////////////////

void start_buzzer() {
    buzzer_on_external = true;
}

void stop_buzzer() {
    buzzer_on_external = false;
}

void* buzzer_controller(void *param) {
    while (running) {
        while (buzzer_on_local || buzzer_on_external) {
            printf("BZZT \n");
            // digitalWrite(BUZZER_PIN, HIGH);
            // Sleep(1);
            // digitalWrite(BUZZER_PIN, LOW);
            // Sleep(1);
            usleep(100);
        }
        usleep(100);
    }
    pthread_exit(NULL);
}

void monitor_buzzer() {
    buzzer_on_local = (calculate_speed_cm_s() > 40.0);
}

int init_server_socket(int port) {
    printf("Trying to connect\n");
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind the socket to the network address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Accept a connection
    if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    return client_socket;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/*                                        COMMUNICATION                                          */
///////////////////////////////////////////////////////////////////////////////////////////////////



void handle_client_disconnect(void) {
    close(connected_socket);
    connected_socket = 0;
}

void* send_communiaction_controller(void* param) {
    while (running) {
        send_update_to_client(calculate_speed_cm_s(), get_measurements());
        usleep(100000);
    }
    pthread_exit(NULL);
}

void* recieve_communiaction_controller(void* param) {
    while (running) {
        receive_msg_from_client();
        usleep(100000);
    }
    pthread_exit(NULL);
}

void send_update_to_client(float wheel_speed, int measure_index) {
    char buffer[64];
    int wheel_speed_int = wheel_speed * 10;
    int n = snprintf(buffer, sizeof(buffer), "M %d %d;", wheel_speed_int, measure_index);
    if (n > 0) {
        send(client_socket, buffer, n, 0);
    }
}

void receive_msg_from_client(void) {
    // Implementation for receiving messages from client (if needed)
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/*                                          VL53L0X API                                          */
///////////////////////////////////////////////////////////////////////////////////////////////////

void print_pal_error(VL53L0X_Error Status) {
    char buf[VL53L0X_MAX_STRING_LENGTH];
    VL53L0X_GetPalErrorString(Status, buf);
    printf("API Status: %i : %s\n", Status, buf);
}

void print_range_status(VL53L0X_RangingMeasurementData_t* pRangingMeasurementData) {
    char buf[VL53L0X_MAX_STRING_LENGTH];
    uint8_t RangeStatus;

    RangeStatus = pRangingMeasurementData->RangeStatus;

    VL53L0X_GetRangeStatusString(RangeStatus, buf);
    printf("Range Status: %i : %s\n", RangeStatus, buf);
}

VL53L0X_Error WaitMeasurementDataReady(VL53L0X_DEV Dev) {
    VL53L0X_Error Status = VL53L0X_ERROR_NONE;
    uint8_t NewDatReady = 0;
    uint32_t LoopNb;

    // Wait until it finished
    // use timeout to avoid deadlock
    if (Status == VL53L0X_ERROR_NONE) {
        LoopNb = 0;
        do {
            Status = VL53L0X_GetMeasurementDataReady(Dev, &NewDatReady);
            if ((NewDatReady == 0x01) || Status != VL53L0X_ERROR_NONE) {
                break;
            }
            LoopNb = LoopNb + 1;
            VL53L0X_PollingDelay(Dev);
        } while (LoopNb < VL53L0X_DEFAULT_MAX_LOOP);

        if (LoopNb >= VL53L0X_DEFAULT_MAX_LOOP) {
            Status = VL53L0X_ERROR_TIME_OUT;
        }
    }

    return Status;
}

VL53L0X_Error WaitStopCompleted(VL53L0X_DEV Dev) {
    VL53L0X_Error Status = VL53L0X_ERROR_NONE;
    uint32_t StopCompleted = 0;
    uint32_t LoopNb;

    // Wait until it finished
    // use timeout to avoid deadlock
    if (Status == VL53L0X_ERROR_NONE) {
        LoopNb = 0;
        do {
            Status = VL53L0X_GetStopCompletedStatus(Dev, &StopCompleted);
            if ((StopCompleted == 0x00) || Status != VL53L0X_ERROR_NONE) {
                break;
            }
            LoopNb = LoopNb + 1;
            VL53L0X_PollingDelay(Dev);
        } while (LoopNb < VL53L0X_DEFAULT_MAX_LOOP);

        if (LoopNb >= VL53L0X_DEFAULT_MAX_LOOP) {
            Status = VL53L0X_ERROR_TIME_OUT;
        }
    }

    return Status;
}

VL53L0X_Error rangingTest(VL53L0X_Dev_t *pMyDevice) {
    VL53L0X_RangingMeasurementData_t RangingMeasurementData;
    VL53L0X_RangingMeasurementData_t *pRangingMeasurementData = &RangingMeasurementData;
    VL53L0X_Error Status = VL53L0X_ERROR_NONE;
    uint32_t refSpadCount;
    uint8_t isApertureSpads;
    uint8_t VhvSettings;
    uint8_t PhaseCal;

    if (Status == VL53L0X_ERROR_NONE) {
        printf("Call of VL53L0X_StaticInit\n");
        Status = VL53L0X_StaticInit(pMyDevice); // Device Initialization
        // StaticInit will set interrupt by default
        print_pal_error(Status);
    }
    if (Status == VL53L0X_ERROR_NONE) {
        printf("Call of VL53L0X_PerformRefCalibration\n");
        Status = VL53L0X_PerformRefCalibration(pMyDevice,
                                               &VhvSettings, &PhaseCal); // Device Initialization
        print_pal_error(Status);
    }

    if (Status == VL53L0X_ERROR_NONE) {
        printf("Call of VL53L0X_PerformRefSpadManagement\n");
        Status = VL53L0X_PerformRefSpadManagement(pMyDevice,
                                                  &refSpadCount, &isApertureSpads); // Device Initialization
        print_pal_error(Status);
    }

    if (Status == VL53L0X_ERROR_NONE) {
        printf("Call of VL53L0X_SetDeviceMode\n");
        Status = VL53L0X_SetDeviceMode(pMyDevice, VL53L0X_DEVICEMODE_CONTINUOUS_RANGING); // Setup in single ranging mode
        print_pal_error(Status);
    }

    if (Status == VL53L0X_ERROR_NONE) {
        printf("Call of VL53L0X_StartMeasurement\n");
        Status = VL53L0X_StartMeasurement(pMyDevice);
        print_pal_error(Status);
    }

    if (Status == VL53L0X_ERROR_NONE) {
        uint32_t measurement;
        uint32_t no_of_measurements = 5000;

        uint16_t *pResults = (uint16_t *)malloc(sizeof(uint16_t) * no_of_measurements);

        bool is_detecting_spoke = false;

        for (measurement = 0; measurement < no_of_measurements; measurement++) {

            Status = WaitMeasurementDataReady(pMyDevice);

            if (Status == VL53L0X_ERROR_NONE) {
                Status = VL53L0X_GetRangingMeasurementData(pMyDevice, pRangingMeasurementData);

                *(pResults + measurement) = pRangingMeasurementData->RangeMilliMeter;
                //printf("In loop measurement %d: %d\n", measurement, pRangingMeasurementData->RangeMilliMeter);

                current_time = get_current_time();

                if (pRangingMeasurementData->RangeMilliMeter < 110 && is_detecting_spoke == false) {
                    is_detecting_spoke = true;
                    record_measurement();
                    print_measurement();
                } else if (pRangingMeasurementData->RangeMilliMeter > 110) {
                    is_detecting_spoke = false;
                }
                float get_speed = calculate_speed_cm_s();
                printf("Estimated wheel speed: %.2fcm/s\n", get_speed);
                monitor_buzzer();
                send_update_to_client(get_speed, get_measurements());

                // Clear the interrupt
                VL53L0X_ClearInterruptMask(pMyDevice, VL53L0X_REG_SYSTEM_INTERRUPT_GPIO_NEW_SAMPLE_READY);
                // VL53L0X_PollingDelay(pMyDevice);
            } else {
                break;
            }
        }

        if (Status == VL53L0X_ERROR_NONE) {
            for (measurement = 0; measurement < no_of_measurements; measurement++) {
                printf("measurement %d: %d\n", measurement, *(pResults + measurement));
            }
        }

        free(pResults);
    }

    if (Status == VL53L0X_ERROR_NONE) {
        printf("Call of VL53L0X_StopMeasurement\n");
        Status = VL53L0X_StopMeasurement(pMyDevice);
    }

    if (Status == VL53L0X_ERROR_NONE) {
        printf("Wait Stop to be competed\n");
        Status = WaitStopCompleted(pMyDevice);
    }

    if (Status == VL53L0X_ERROR_NONE)
        Status = VL53L0X_ClearInterruptMask(pMyDevice,
                                            VL53L0X_REG_SYSTEM_INTERRUPT_GPIO_NEW_SAMPLE_READY);

    return Status;
}

int main(int argc, char **argv) {
    printf("Start\n");
    VL53L0X_Error Status = VL53L0X_ERROR_NONE;
    VL53L0X_Dev_t MyDevice;
    VL53L0X_Dev_t *pMyDevice = &MyDevice;
    VL53L0X_Version_t Version;
    VL53L0X_Version_t *pVersion = &Version;
    VL53L0X_DeviceInfo_t DeviceInfo;

    int port = 12345;  // You can choose any available port
    printf("Trying to connect\n");
    client_socket = init_server_socket(port);
    printf("End connecting \n");

    // running = true;
    // pthread_t thread_id_send;
    // if (pthread_create(&thread_id_send, NULL, send_communiaction_controller, NULL) != 0) {
    //     fprintf(stderr, "Error creating thread\n");
    //     return 1;
    // }

    running = true;
    pthread_t thread_id_buzzer;
    if (pthread_create(&thread_id_buzzer, NULL, buzzer_controller, NULL) != 0)
    {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }

    int32_t status_int;

    printf("VL53L0X PAL Continuous Ranging example\n\n");
    pMyDevice->I2cDevAddr = 0x29;

    pMyDevice->fd = VL53L0X_i2c_init("/dev/i2c-1", pMyDevice->I2cDevAddr);
    if (MyDevice.fd < 0) {
        Status = VL53L0X_ERROR_CONTROL_INTERFACE;
        printf("Failed to init\n");
    }
    if (Status == VL53L0X_ERROR_NONE) {
        status_int = VL53L0X_GetVersion(pVersion);
        if (status_int != 0)
            Status = VL53L0X_ERROR_CONTROL_INTERFACE;
    }

    if (Status == VL53L0X_ERROR_NONE) {
        if (pVersion->major != VERSION_REQUIRED_MAJOR ||
            pVersion->minor != VERSION_REQUIRED_MINOR ||
            pVersion->build != VERSION_REQUIRED_BUILD) {
            printf("VL53L0X API Version Error: Your firmware has %d.%d.%d (revision %d). This example requires %d.%d.%d.\n",
                   pVersion->major, pVersion->minor, pVersion->build, pVersion->revision,
                   VERSION_REQUIRED_MAJOR, VERSION_REQUIRED_MINOR, VERSION_REQUIRED_BUILD);
        }
    }

    if (Status == VL53L0X_ERROR_NONE) {
        printf("Call of VL53L0X_DataInit\n");
        Status = VL53L0X_DataInit(&MyDevice); // Data initialization
        print_pal_error(Status);
    }

    if (Status == VL53L0X_ERROR_NONE) {
        Status = VL53L0X_GetDeviceInfo(&MyDevice, &DeviceInfo);
    }
    if (Status == VL53L0X_ERROR_NONE) {
        printf("VL53L0X_GetDeviceInfo:\n");
        printf("Device Name : %s\n", DeviceInfo.Name);
        printf("Device Type : %s\n", DeviceInfo.Type);
        printf("Device ID : %s\n", DeviceInfo.ProductId);
        printf("ProductRevisionMajor : %d\n", DeviceInfo.ProductRevisionMajor);
        printf("ProductRevisionMinor : %d\n", DeviceInfo.ProductRevisionMinor);

        if ((DeviceInfo.ProductRevisionMinor != 1) && (DeviceInfo.ProductRevisionMinor != 1)) {
            printf("Error expected cut 1.1 but found cut %d.%d\n",
                   DeviceInfo.ProductRevisionMajor, DeviceInfo.ProductRevisionMinor);
            Status = VL53L0X_ERROR_NOT_SUPPORTED;
        }
    }

    if (Status == VL53L0X_ERROR_NONE) {
        Status = rangingTest(pMyDevice);
    }

    print_pal_error(Status);
    printf("Close Comms\n");
    VL53L0X_i2c_close();
    print_pal_error(Status);

    running = false;
    pthread_join(thread_id_buzzer, NULL);
    return (0);
}
