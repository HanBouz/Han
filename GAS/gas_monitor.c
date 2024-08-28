#include <stdio.h>        // �]�t�зǿ�J��X��ƪ��Y���C
#include <stdlib.h>       // �]�t�зǮw��ƪ��Y���A�Ҧpexit()�Matoi()�C
#include <fcntl.h>        // �]�t��󱱨��ƪ��Y���A�Ҧpopen()�C
#include <unistd.h>       // �]�tPOSIX�ާ@�t��API���Y���A�Ҧpread()�Mclose()�C
#include <string.h>       // �]�t�r�Ŧ�B�z��ƪ��Y���A�Ҧpstrlen()�Mstrncpy()�C
#include <signal.h>       // �]�t�H���B�z��ƪ��Y���A�Ҧpsigaction()�C
#include <errno.h>        // �]�t���~�X�w�q���Y���C
#include <time.h>         // �]�t�ɶ��B�z��ƪ��Y���A�Ҧptime()�Msleep()�C

#define DEVICE_FILE "/dev/gas_detector" // �w�q�r�ų]���ɮת����|�C
#define BUFFER_SIZE 16          // �w�qŪ���ƾڪ��w�İϤj�p�C
#define RED_LED   17           // �w�q����LED��GPIO�}��C
#define GREEN_LED 27           // �w�q���LED��GPIO�}��C
#define BLUE_LED  22           // �w�q�Ŧ�LED��GPIO�}��C
#define BUZZER    24           // �w�q���ﾹ��GPIO�}��C
#define TONE_FREQUENCY1 1000   // �w�q���ﾹ���W�v1��1000Hz�C
#define TONE_FREQUENCY2 1500   // �w�q���ﾹ���W�v2��1500Hz�C

volatile sig_atomic_t stop = 0; // �w�q�@�ӥi�ܪ��лx�ܼơA�Ω󱱨�H���B�z�C

void inthand(int signum) {   // �H���B�z��ơA�����줤�_�H���ɰ���C
    stop = 1;   // �]�m�лx�ܼƥH����D�`���C
    printf("\nInterrupt received. Stopping the monitor...\n"); // ���L���_�H���C
}

int is_gpio_exported(int gpio) { // �P�_GPIO�O�_�w�g�Q�ɥX�C
    char path[64];  // �w�q���|�w�İϡC
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", gpio); // �c�yGPIO�����|�C
    return access(path, F_OK) == 0; // �p�G���|�s�b�A��^1�A�_�h��^0�C
}

void export_gpio(int gpio) {  // �ɥX���w��GPIO�C
    if (is_gpio_exported(gpio)) {  // �p�GGPIO�w�g�Q�ɥX�C
        printf("GPIO %d is already exported\n", gpio); // ���L�w�g�ɥX���H���C
        return;
    }

    int fd = open("/sys/class/gpio/export", O_WRONLY); // ���}GPIO�ɥX���C
    if (fd == -1) {  // �p�G���}��󥢱ѡC
        perror("Unable to open /sys/class/gpio/export"); // ���L���~�H���C
        exit(EXIT_FAILURE); // �פ�{�ǡC
    }

    char buf[8];  // �w�q�w�İϥH�s�xGPIO���C
    snprintf(buf, sizeof(buf), "%d", gpio); // �NGPIO���ഫ���r�Ŧ�C
    if (write(fd, buf, strlen(buf)) != (ssize_t)strlen(buf)) { // �V�ɥX���g�JGPIO���C
        perror("Error writing to /sys/class/gpio/export"); // ���L���~�H���C
        close(fd); // �������y�z�šC
        exit(EXIT_FAILURE); // �פ�{�ǡC
    }

    close(fd); // �������y�z�šC
}

void set_gpio_direction(int gpio, const char* direction) { // �]�mGPIO����V�]��J�ο�X�^�C
    char path[64];  // �w�q���|�w�İϡC
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio); // �c�yGPIO��V��󪺸��|�C

    int fd = open(path, O_WRONLY); // ���}GPIO��V���C
    if (fd == -1) {  // �p�G���}��󥢱ѡC
        perror("Unable to open gpio direction file"); // ���L���~�H���C
        exit(EXIT_FAILURE); // �פ�{�ǡC
    }

    if (write(fd, direction, strlen(direction)) != (ssize_t)strlen(direction)) { // �V���g�J��V�C
        perror("Error setting gpio direction"); // ���L���~�H���C
        close(fd); // �������y�z�šC
        exit(EXIT_FAILURE); // �פ�{�ǡC
    }

    close(fd); // �������y�z�šC
}

void set_gpio_value(int gpio, int value) { // �]�mGPIO���ȡ]���ΧC�^�C
    char path[64];  // �w�q���|�w�İϡC
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio); // �c�yGPIO�Ȥ�󪺸��|�C

    int fd = open(path, O_WRONLY); // ���}GPIO�Ȥ��C
    if (fd == -1) {  // �p�G���}��󥢱ѡC
        perror("Unable to open gpio value file"); // ���L���~�H���C
        exit(EXIT_FAILURE); // �פ�{�ǡC
    }

    char buf[2];  // �w�q�w�İϥH�s�xGPIO�ȡC
    snprintf(buf, sizeof(buf), "%d", value); // �NGPIO���ഫ���r�Ŧ�C
    if (write(fd, buf, strlen(buf)) != (ssize_t)strlen(buf)) { // �V���g�J�ȡC
        perror("Error writing to gpio value file"); // ���L���~�H���C
        close(fd); // �������y�z�šC
        exit(EXIT_FAILURE); // �פ�{�ǡC
    }

    close(fd); // �������y�z�šC
}

void setup_gpio() {  // �]�mGPIO�}��C
    export_gpio(RED_LED);   // �ɥX����LED��GPIO�C
    export_gpio(GREEN_LED); // �ɥX���LED��GPIO�C
    export_gpio(BLUE_LED);  // �ɥX�Ŧ�LED��GPIO�C
    export_gpio(BUZZER);    // �ɥX���ﾹ��GPIO�C

    set_gpio_direction(RED_LED, "out");   // �]�m����LED����X�C
    set_gpio_direction(GREEN_LED, "out"); // �]�m���LED����X�C
    set_gpio_direction(BLUE_LED, "out");  // �]�m�Ŧ�LED����X�C
    set_gpio_direction(BUZZER, "out");    // �]�m���ﾹ����X�C

    // ��l�ƩҦ�LED�M���ﾹ���������A
    set_gpio_value(RED_LED, 0);   // ��������LED�C
    set_gpio_value(GREEN_LED, 0); // �������LED�C
    set_gpio_value(BLUE_LED, 0);  // �����Ŧ�LED�C
    set_gpio_value(BUZZER, 0);    // �������ﾹ�C
}

void set_led(int led) {  // �ھګ��w��LED�}��]�mLED���A�C
    set_gpio_value(RED_LED, led == RED_LED ? 1 : 0);   // �p�G���w������LED�A���}����LED�A�_�h�����C
    set_gpio_value(GREEN_LED, led == GREEN_LED ? 1 : 0); // �p�G���w�����LED�A���}���LED�A�_�h�����C
    set_gpio_value(BLUE_LED, led == BLUE_LED ? 1 : 0);  // �p�G���w���Ŧ�LED�A���}�Ŧ�LED�A�_�h�����C
}

void sound_tone(int frequency, int duration_ms) {  // �o�X���w�W�v�M����ɶ������աC
    int half_period_us = 500000 / frequency;  // �p��b�g���]�L��^�C
    int cycles = (duration_ms * 1000) / (2 * half_period_us);  // �p���`�g���ơC

    for (int i = 0; i < cycles && !stop; i++) {  // �b���w���g�����o�X���աA����H���B�z�{�ǰ���C
        set_gpio_value(BUZZER, 1);    // ���}���ﾹ�C
        usleep(half_period_us);      // ����b�g���C
        set_gpio_value(BUZZER, 0);    // �������ﾹ�C
        usleep(half_period_us);      // ����b�g���C
    }
}

void sound_alarm(int duration_ms) {  // �o�Xĵ�����A������w���ɶ��C
    int single_tone_duration = 500;  // �C�ӭ��ժ�����ɶ��]�@��^�C
    int cycles = duration_ms / (2 * single_tone_duration);  // �p���`�g���ơC

    for (int i = 0; i < cycles && !stop; i++) {  // �b���w���g�����o�X���աA����H���B�z�{�ǰ���C
        sound_tone(TONE_FREQUENCY1, single_tone_duration);  // �o�X�W�v1�����աC
        sound_tone(TONE_FREQUENCY2, single_tone_duration);  // �o�X�W�v2�����աC
    }
}

int main() {  // �D��ơC
    int fd, gas_level;  // �w�q���y�z�ũM����@���ܼơC
    char buffer[BUFFER_SIZE];  // �w�q�w�İϥH�s�xŪ���ƾڡC
    ssize_t bytes_read;  // �w�q���Ū�����r�`�ơC
    time_t last_read_time = 0;  // �O���W��Ū���ɶ��C
    struct sigaction sa;  // �w�q�H���B�z���c�C

    setup_gpio();  // �]�mGPIO�}��C

    sa.sa_handler = inthand;  // �]�m�H���B�z��ơC
    sigemptyset(&sa.sa_mask); // ��l�ƫH�������šC
    sa.sa_flags = 0;  // �]�m�H���B�z�лx��0�C
    sigaction(SIGINT, &sa, NULL);  // �]�mSIGINT�H�����B�z�{�ǡC

    fd = open(DEVICE_FILE, O_RDONLY);  // ���}�]�Ƥ��C
    if (fd < 0) {  // �p�G���}�]�Ƥ�󥢱ѡC
        perror("Failed to open the device file"); // ���L���~�H���C
        set_led(BLUE_LED);  // �I�G�Ŧ�LED�C
        return EXIT_FAILURE; // ��^���Ѫ��A�C
    }

    printf("Gas Monitor started. Press Ctrl+C to exit.\n");  // ���L�ҰʫH���C

    while (!stop) {  // ��лx�ܼƬ�0�ɫ���`���C
        lseek(fd, 0, SEEK_SET);  // ���m��󰾲��q�C
        bytes_read = read(fd, buffer, sizeof(buffer) - 1);  // Ū���ƾڡC

        time_t current_time = time(NULL);  // �����e�ɶ��C

        if (bytes_read > 0) {  // �p�G���\Ū���ƾڡC
            buffer[bytes_read] = '\0';  // �פ�r�Ŧ�C
            gas_level = atoi(buffer);  // �N�r�Ŧ��ഫ����ơC
            printf("Time: %ld, Gas level: %d\n", current_time, gas_level);  // ���L����@�סC

            if (gas_level == 0) {  // �p�G����@�׬�0�C
                printf("WARNING: Gas level is zero. Sensor may be faulty or disconnected.\n");  // ���Lĵ�i�H���C
                set_led(BLUE_LED);  // �I�G�Ŧ�LED�C
                set_gpio_value(BUZZER, 0);  // �������ﾹ�C
            } else if (gas_level > 200) {  // �p�G����@�װ���200�C
                printf("ALERT: High gas concentration detected!\n");  // ���Lĵ���H���C
                set_led(RED_LED);  // �I�G����LED�C
                sound_alarm(2000);  // �o�X2������ĵ�����C
            } else {  // �p�G����@�ץ��`�C
                printf("Normal gas levels.\n");  // ���L���`�H���C
                set_led(GREEN_LED);  // �I�G���LED�C
                set_gpio_value(BUZZER, 0);  // �������ﾹ�C
            }

            last_read_time = current_time;  // ��s�W��Ū���ɶ��C
        } else if (bytes_read == 0) {  // �p�G�S���s�ƾڡC
            printf("Time: %ld, No new data available\n", current_time);  // ���L�L�ƾګH���C
        } else if (bytes_read < 0) {  // �p�GŪ���ƾڥX���C
            if (errno != EINTR) {  // ������Ctrl+C�ް_�����_�C
                perror("Failed to read from device");  // ���L���~�H���C
                set_led(BLUE_LED);  // �I�G�Ŧ�LED�C
                break;  // ���X�`���C
            }
        }

        if (current_time - last_read_time > 5) {  // �p�G�W�L5��S���s�ƾڡC
            printf("No new data for 5 seconds. Device may be unresponsive.\n");  // ���L�]�ƵL�T���H���C
            set_led(BLUE_LED);  // �I�G�Ŧ�LED�C
        }

        if (!stop) {  // �p�G�S�����찱��H���C
            sleep(1);  // �C���v1��C
        }
    }

    close(fd);  // �����]�Ƥ��C
    // �����Ҧ�LED�M���ﾹ
    set_gpio_value(RED_LED, 0);   // ��������LED�C
    set_gpio_value(GREEN_LED, 0); // �������LED�C
    set_gpio_value(BLUE_LED, 0);  // �����Ŧ�LED�C
    set_gpio_value(BUZZER, 0);    // �������ﾹ�C

    printf("Gas Monitor stopped.\n");  // ���L����H���C
    return EXIT_SUCCESS;  // ��^���\���A�C
}
