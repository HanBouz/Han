#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define DEVICE_FILE "/dev/gas_detector"
#define BUFFER_SIZE 16
#define RED_LED   17
#define GREEN_LED 27
#define BLUE_LED  22
#define BUZZER    24
#define TONE_FREQUENCY1 1000
#define TONE_FREQUENCY2 1500
#define BLUE_ALARM_FREQUENCY 500  // 藍色警報的單一頻率
#define BLUE_ALARM_DURATION 30

volatile sig_atomic_t stop = 0;
volatile int blue_alarm_active = 0;
volatile time_t blue_alarm_start_time = 0;

void inthand(int signum) {
    stop = 1;
    printf("\nInterrupt received. Stopping the monitor...\n");
}

int is_gpio_exported(int gpio) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", gpio);
    return access(path, F_OK) == 0;
}

void export_gpio(int gpio) {
    if (is_gpio_exported(gpio)) {
        printf("GPIO %d is already exported\n", gpio);
        return;
    }

    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd == -1) {
        perror("Unable to open /sys/class/gpio/export");
        exit(EXIT_FAILURE);
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", gpio);
    if (write(fd, buf, strlen(buf)) != (ssize_t)strlen(buf)) {
        perror("Error writing to /sys/class/gpio/export");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
}

void set_gpio_direction(int gpio, const char* direction) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio);

    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        perror("Unable to open gpio direction file");
        exit(EXIT_FAILURE);
    }

    if (write(fd, direction, strlen(direction)) != (ssize_t)strlen(direction)) {
        perror("Error setting gpio direction");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
}

void set_gpio_value(int gpio, int value) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);

    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        perror("Unable to open gpio value file");
        exit(EXIT_FAILURE);
    }

    char buf[2];
    snprintf(buf, sizeof(buf), "%d", value);
    if (write(fd, buf, strlen(buf)) != (ssize_t)strlen(buf)) {
        perror("Error writing to gpio value file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
}

void setup_gpio() {
    export_gpio(RED_LED);
    export_gpio(GREEN_LED);
    export_gpio(BLUE_LED);
    export_gpio(BUZZER);

    set_gpio_direction(RED_LED, "out");
    set_gpio_direction(GREEN_LED, "out");
    set_gpio_direction(BLUE_LED, "out");
    set_gpio_direction(BUZZER, "out");

    set_gpio_value(RED_LED, 0);
    set_gpio_value(GREEN_LED, 0);
    set_gpio_value(BLUE_LED, 0);
    set_gpio_value(BUZZER, 0);
}

void set_led(int led) {
    set_gpio_value(RED_LED, led == RED_LED ? 1 : 0);
    set_gpio_value(GREEN_LED, led == GREEN_LED ? 1 : 0);
    set_gpio_value(BLUE_LED, led == BLUE_LED ? 1 : 0);
}

void sound_tone(int frequency, int duration_ms) {
    int half_period_us = 500000 / frequency;
    int cycles = (duration_ms * 1000) / (2 * half_period_us);

    for (int i = 0; i < cycles && !stop; i++) {
        set_gpio_value(BUZZER, 1);
        usleep(half_period_us);
        set_gpio_value(BUZZER, 0);
        usleep(half_period_us);
    }
}

void sound_alarm(int duration_ms) {
    int single_tone_duration = 500;
    int cycles = duration_ms / (2 * single_tone_duration);

    for (int i = 0; i < cycles && !stop; i++) {
        sound_tone(TONE_FREQUENCY1, single_tone_duration);
        sound_tone(TONE_FREQUENCY2, single_tone_duration);
    }
}

void blue_led_alarm_cycle() {
    if (blue_alarm_active) {
        set_led(BLUE_LED);  // 確保藍色 LED 始終亮起
        
        // 產生持續的單一頻率警報聲
        sound_tone(BLUE_ALARM_FREQUENCY, 1000);  // 持續1秒
        
        // 檢查是否需要停止警報
        time_t current_time = time(NULL);
        if (current_time - blue_alarm_start_time >= BLUE_ALARM_DURATION) {
            blue_alarm_active = 0;
            set_gpio_value(BUZZER, 0);  // 關閉蜂鳴器
        }
    } else {
        // 如果警報不活躍，確保 LED 和蜂鳴器都關閉
        set_gpio_value(BLUE_LED, 0);
        set_gpio_value(BUZZER, 0);
    }
}

void start_blue_led_alarm() {
    blue_alarm_active = 1;
    blue_alarm_start_time = time(NULL);
}

int main() {
    int fd, gas_level;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    time_t last_read_time = 0;
    struct sigaction sa;
    FILE *log_file;

    setup_gpio();

    sa.sa_handler = inthand;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    fd = open(DEVICE_FILE, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open the device file");
        start_blue_led_alarm();
        while (!stop) blue_led_alarm_cycle();
        return EXIT_FAILURE;
    }

    log_file = fopen("gas_levels.txt", "w");
    if (!log_file) {
        perror("Failed to open log file");
        close(fd);
        start_blue_led_alarm();
        while (!stop) blue_led_alarm_cycle();
        return EXIT_FAILURE;
    }

    printf("Gas Monitor started. Press Ctrl+C to exit.\n");

    while (!stop) {
        lseek(fd, 0, SEEK_SET);
        bytes_read = read(fd, buffer, sizeof(buffer) - 1);

        time_t current_time = time(NULL);

        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            gas_level = atoi(buffer);
            printf("Time: %ld, Gas level: %d\n", current_time, gas_level);

            fprintf(log_file, "%ld, %d\n", current_time, gas_level);
            fflush(log_file);

            if (gas_level == 0) {
                printf("WARNING: Gas level is zero. Sensor may be faulty or disconnected.\n");
                start_blue_led_alarm();
            } else if (gas_level > 200) {
                printf("ALERT: High gas concentration detected!\n");
                set_led(RED_LED);
                sound_alarm(2000);
            } else {
                printf("Normal gas levels.\n");
                set_led(GREEN_LED);
                blue_alarm_active = 0;  // 停止藍色警報（如果正在進行）
            }

            last_read_time = current_time;
        } else if (bytes_read == 0) {
            printf("Time: %ld, No new data available\n", current_time);
        } else if (bytes_read < 0 && errno != EINTR) {
            perror("Failed to read from device");
            start_blue_led_alarm();
        }

        if (current_time - last_read_time > 5) {
            printf("No new data for 5 seconds. Device may be unresponsive.\n");
            start_blue_led_alarm();
        }

        blue_led_alarm_cycle();  // 處理藍色 LED 警報

        if (!blue_alarm_active && !stop) {
            sleep(1);
        }
    }

    close(fd);
    fclose(log_file);

    set_gpio_value(RED_LED, 0);
    set_gpio_value(GREEN_LED, 0);
    set_gpio_value(BLUE_LED, 0);
    set_gpio_value(BUZZER, 0);

    printf("Gas Monitor stopped.\n");
    return EXIT_SUCCESS;
}