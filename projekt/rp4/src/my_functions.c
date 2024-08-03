#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/time.h>
#include <termios.h>
#include <fcntl.h>
//#include <wiringPi.h> Do buzzera ( sudo apt-get install wiringpi )

#define wheel_circumference 10
#define BUZZER_PIN 24  // GPIO pin where the buzzer is connected

unsigned long start_time;
unsigned long current_time;
unsigned long last_measurement_time = 0;
unsigned long measurement_time = 0;
bool running = false;
bool buzzer_on_local = false;
bool buzzer_on_external = false;
bool is_detecting_spoke = false;
int measurements = 0;
pthread_mutex_t mutex;


unsigned long get_current_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void set_nonblocking_input() {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag &= ~ICANON;
    ttystate.c_lflag &= ~ECHO;
    ttystate.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

void reset_input() {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag |= ICANON;
    ttystate.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/*                                   MEASUREMENT FUNCTIONS                                       */
///////////////////////////////////////////////////////////////////////////////////////////////////

void print_measurement()
{
    pthread_mutex_lock(&mutex);
    printf("Last measurement: %lu\t Current measurement: %lu\t Time between: %lu\n", 
           last_measurement_time, measurement_time, measurement_time - last_measurement_time);
    pthread_mutex_unlock(&mutex);
}

void record_measurement()
{
    pthread_mutex_lock(&mutex);
    last_measurement_time = measurement_time; 
    measurement_time = current_time - start_time;
    measurements++;
    pthread_mutex_unlock(&mutex);
}

int get_measurements()
{
    int measurement_ = 0;
    pthread_mutex_lock(&mutex);
    measurement_ = measurements;
    pthread_mutex_unlock(&mutex);
    return measurement_;
}

float calculate_speed_cm_s()
{
    if(measurements < 1)
        return 0;
    pthread_mutex_lock(&mutex);
    unsigned long current_measurement_difference = (current_time - start_time) - measurement_time;
    unsigned long difference_between_measurements = measurement_time - last_measurement_time;
    pthread_mutex_unlock(&mutex);

    if(current_measurement_difference > difference_between_measurements)
    {
        return (float)wheel_circumference / ((float)current_measurement_difference / 1000);
    }
    else
    {
        return (float)wheel_circumference / ((float)difference_between_measurements/ 1000);
    }
}



///////////////////////////////////////////////////////////////////////////////////////////////////
/*                                    BUZZER FUNCTIONALITY                                       */
///////////////////////////////////////////////////////////////////////////////////////////////////

void start_buzzer()
{
    buzzer_on_external = true;
}

void stop_buzzer()
{
    buzzer_on_external = false;
}

void* buzzer_controller(void *param)
{
    while(running)
    {
        while(buzzer_on_local || buzzer_on_external)
        {    
            printf("BZZT \n");
            // digitalWrite(BUZZER_PIN, HIGH);
            // Sleep(1);                      
            // digitalWrite(BUZZER_PIN, LOW);  
            // Sleep(1);
        }
        usleep(100000);
    }
    pthread_exit(NULL);
}

void monitor_buzzer()
{
    buzzer_on_local = (calculate_speed_cm_s() > 20.0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/*                                        COMMUNICATION                                          */
///////////////////////////////////////////////////////////////////////////////////////////////////

void send_update_to_client(float wheel_speed, int measure_index) {
    // if (connected_socket <= 0) return;

    // // Convert the data to a string
    // char msg_buf[64];
    // int msg_str_len = snprintf(msg_buf, 64, "M %u %u;", measure_index, wheel_speed);
    // if (msg_str_len < 0 || msg_str_len >= 64) {
    //     fputs("Net: format string failed", stderr);
    //     exit(EXIT_FAILURE);
    // }

    // // Send the string to the client
    // // Poll with a timeout is required to detect clients disconnecting.
    // // Without this check the send blocks forever, which hangs the whole server.
    // struct pollfd poll_descriptor = {.fd = connected_socket, .events = POLLOUT};
    // int poll_result = poll(&poll_descriptor, 1, 1500);

    // if (poll_result < 0) {
    //     // Poll error
    //     perror("Net: send poll");
    //     exit(EXIT_FAILURE);
    // } else if (poll_result == 0 || poll_descriptor.revents & POLLHUP) {
    //     handle_client_disconnect();
    // } else {
    //     // Ready to send data
    //     assert(poll_descriptor.revents & POLLOUT);
    //     ssize_t sent_len = send(connected_socket, msg_buf, msg_str_len, MSG_DONTWAIT);
    //     if (sent_len != msg_str_len) {
    //         perror("Net: send");
    //         exit(EXIT_FAILURE);
    //     }
    // }
}

void receive_msg_from_client(void) {
    // char msg_buff[64] = {0};
    // ssize_t bytes_read = read(connected_socket, msg_buff, 64);
    // if (bytes_read < 0) {
    //     perror("read(connected_socket)");
    // } else if (bytes_read == 2 && msg_buff[0] == 'N' && msg_buff[1] == ';') {
    //     stop_buzzer();
    // } else if (bytes_read == 2 && msg_buff[0] == 'B' && msg_buff[1] == ';') {
    //     start_buzzer();
    // }
}

void handle_client_disconnect(void) {
    // close(connected_socket);
    // connected_socket = 0;
}

void* send_communiaction_controller(void* param)
{
    while(running)
    {
        send_update_to_client(calculate_speed_cm_s(), get_measurements());
        usleep(100000);
    }
    pthread_exit(NULL);
}

void* recieve_communiaction_controller(void* param)
{
    while(running)
    {
        receive_msg_from_client();
        usleep(100000);
    }
    pthread_exit(NULL);
}


int main() {
    start_time = get_current_time();

    pthread_mutex_init(&mutex, NULL);
    pthread_t thread_id_buzzer;
    pthread_t thread_id_send;
    pthread_t thread_id_recieve;
    running = true;

    if (pthread_create(&thread_id_buzzer, NULL, buzzer_controller, NULL) != 0)
    {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }
    if (pthread_create(&thread_id_send, NULL, send_communiaction_controller, NULL) != 0)
    {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }
    if (pthread_create(&thread_id_recieve, NULL, recieve_communiaction_controller, NULL) != 0)
    {   
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }

    // BUZZER FUNCTIONALITY

    // if (wiringPiSetupGpio() == -1) {
    //     fprintf(stderr, "Failed to initialize wiringPi\n");
    //     return 1;
    // }
    
    // pinMode(BUZZER_PIN, OUTPUT);
    // digitalWrite(BUZZER_PIN, LOW);

    printf("Press the space bar to print 'Hello'. Press 'q' to quit.\n");
    set_nonblocking_input();
    while (1) {
        current_time = get_current_time();
        int ch = getchar();
        if (ch!=EOF) {
            if (ch == ' ') {
                if(is_detecting_spoke == false)
                {
                    record_measurement();
                    print_measurement();
                    is_detecting_spoke = true;
                }
            } else if (ch == 'q') {
                break;
            }
        }
        else
        {
            is_detecting_spoke = false;
        }
        usleep(100000);
        printf("Estimated wheel speed: %.2fcm/s\n", calculate_speed_cm_s());
        monitor_buzzer();
        send_update_to_client(calculate_speed_cm_s(), measurements);
    }
    reset_input();

    running = false;

    pthread_join(thread_id_buzzer, NULL);
    pthread_join(thread_id_send, NULL);
    pthread_join(thread_id_recieve, NULL);

    handle_client_disconnect();
    return 0;
}