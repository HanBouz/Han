#include <stdio.h>        // 包含標準輸入輸出函數的頭文件。
#include <stdlib.h>       // 包含標準庫函數的頭文件，例如exit()和atoi()。
#include <fcntl.h>        // 包含文件控制函數的頭文件，例如open()。
#include <unistd.h>       // 包含POSIX操作系統API的頭文件，例如read()和close()。
#include <string.h>       // 包含字符串處理函數的頭文件，例如strlen()和strncpy()。
#include <signal.h>       // 包含信號處理函數的頭文件，例如sigaction()。
#include <errno.h>        // 包含錯誤碼定義的頭文件。
#include <time.h>         // 包含時間處理函數的頭文件，例如time()和sleep()。

#define DEVICE_FILE "/dev/gas_detector" // 定義字符設備檔案的路徑。
#define BUFFER_SIZE 16          // 定義讀取數據的緩衝區大小。
#define RED_LED   17           // 定義紅色LED的GPIO腳位。
#define GREEN_LED 27           // 定義綠色LED的GPIO腳位。
#define BLUE_LED  22           // 定義藍色LED的GPIO腳位。
#define BUZZER    24           // 定義蜂鳴器的GPIO腳位。
#define TONE_FREQUENCY1 1000   // 定義蜂鳴器的頻率1為1000Hz。
#define TONE_FREQUENCY2 1500   // 定義蜂鳴器的頻率2為1500Hz。

volatile sig_atomic_t stop = 0; // 定義一個可變的標誌變數，用於控制信號處理。

void inthand(int signum) {   // 信號處理函數，當接收到中斷信號時執行。
    stop = 1;   // 設置標誌變數以停止主循環。
    printf("\nInterrupt received. Stopping the monitor...\n"); // 打印中斷信息。
}

int is_gpio_exported(int gpio) { // 判斷GPIO是否已經被導出。
    char path[64];  // 定義路徑緩衝區。
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", gpio); // 構造GPIO的路徑。
    return access(path, F_OK) == 0; // 如果路徑存在，返回1，否則返回0。
}

void export_gpio(int gpio) {  // 導出指定的GPIO。
    if (is_gpio_exported(gpio)) {  // 如果GPIO已經被導出。
        printf("GPIO %d is already exported\n", gpio); // 打印已經導出的信息。
        return;
    }

    int fd = open("/sys/class/gpio/export", O_WRONLY); // 打開GPIO導出文件。
    if (fd == -1) {  // 如果打開文件失敗。
        perror("Unable to open /sys/class/gpio/export"); // 打印錯誤信息。
        exit(EXIT_FAILURE); // 終止程序。
    }

    char buf[8];  // 定義緩衝區以存儲GPIO號。
    snprintf(buf, sizeof(buf), "%d", gpio); // 將GPIO號轉換為字符串。
    if (write(fd, buf, strlen(buf)) != (ssize_t)strlen(buf)) { // 向導出文件寫入GPIO號。
        perror("Error writing to /sys/class/gpio/export"); // 打印錯誤信息。
        close(fd); // 關閉文件描述符。
        exit(EXIT_FAILURE); // 終止程序。
    }

    close(fd); // 關閉文件描述符。
}

void set_gpio_direction(int gpio, const char* direction) { // 設置GPIO的方向（輸入或輸出）。
    char path[64];  // 定義路徑緩衝區。
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio); // 構造GPIO方向文件的路徑。

    int fd = open(path, O_WRONLY); // 打開GPIO方向文件。
    if (fd == -1) {  // 如果打開文件失敗。
        perror("Unable to open gpio direction file"); // 打印錯誤信息。
        exit(EXIT_FAILURE); // 終止程序。
    }

    if (write(fd, direction, strlen(direction)) != (ssize_t)strlen(direction)) { // 向文件寫入方向。
        perror("Error setting gpio direction"); // 打印錯誤信息。
        close(fd); // 關閉文件描述符。
        exit(EXIT_FAILURE); // 終止程序。
    }

    close(fd); // 關閉文件描述符。
}

void set_gpio_value(int gpio, int value) { // 設置GPIO的值（高或低）。
    char path[64];  // 定義路徑緩衝區。
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio); // 構造GPIO值文件的路徑。

    int fd = open(path, O_WRONLY); // 打開GPIO值文件。
    if (fd == -1) {  // 如果打開文件失敗。
        perror("Unable to open gpio value file"); // 打印錯誤信息。
        exit(EXIT_FAILURE); // 終止程序。
    }

    char buf[2];  // 定義緩衝區以存儲GPIO值。
    snprintf(buf, sizeof(buf), "%d", value); // 將GPIO值轉換為字符串。
    if (write(fd, buf, strlen(buf)) != (ssize_t)strlen(buf)) { // 向文件寫入值。
        perror("Error writing to gpio value file"); // 打印錯誤信息。
        close(fd); // 關閉文件描述符。
        exit(EXIT_FAILURE); // 終止程序。
    }

    close(fd); // 關閉文件描述符。
}

void setup_gpio() {  // 設置GPIO腳位。
    export_gpio(RED_LED);   // 導出紅色LED的GPIO。
    export_gpio(GREEN_LED); // 導出綠色LED的GPIO。
    export_gpio(BLUE_LED);  // 導出藍色LED的GPIO。
    export_gpio(BUZZER);    // 導出蜂鳴器的GPIO。

    set_gpio_direction(RED_LED, "out");   // 設置紅色LED為輸出。
    set_gpio_direction(GREEN_LED, "out"); // 設置綠色LED為輸出。
    set_gpio_direction(BLUE_LED, "out");  // 設置藍色LED為輸出。
    set_gpio_direction(BUZZER, "out");    // 設置蜂鳴器為輸出。

    // 初始化所有LED和蜂鳴器為關閉狀態
    set_gpio_value(RED_LED, 0);   // 關閉紅色LED。
    set_gpio_value(GREEN_LED, 0); // 關閉綠色LED。
    set_gpio_value(BLUE_LED, 0);  // 關閉藍色LED。
    set_gpio_value(BUZZER, 0);    // 關閉蜂鳴器。
}

void set_led(int led) {  // 根據指定的LED腳位設置LED狀態。
    set_gpio_value(RED_LED, led == RED_LED ? 1 : 0);   // 如果指定為紅色LED，打開紅色LED，否則關閉。
    set_gpio_value(GREEN_LED, led == GREEN_LED ? 1 : 0); // 如果指定為綠色LED，打開綠色LED，否則關閉。
    set_gpio_value(BLUE_LED, led == BLUE_LED ? 1 : 0);  // 如果指定為藍色LED，打開藍色LED，否則關閉。
}

void sound_tone(int frequency, int duration_ms) {  // 發出指定頻率和持續時間的音調。
    int half_period_us = 500000 / frequency;  // 計算半週期（微秒）。
    int cycles = (duration_ms * 1000) / (2 * half_period_us);  // 計算總週期數。

    for (int i = 0; i < cycles && !stop; i++) {  // 在指定的週期內發出音調，直到信號處理程序停止。
        set_gpio_value(BUZZER, 1);    // 打開蜂鳴器。
        usleep(half_period_us);      // 延遲半週期。
        set_gpio_value(BUZZER, 0);    // 關閉蜂鳴器。
        usleep(half_period_us);      // 延遲半週期。
    }
}

void sound_alarm(int duration_ms) {  // 發出警報音，持續指定的時間。
    int single_tone_duration = 500;  // 每個音調的持續時間（毫秒）。
    int cycles = duration_ms / (2 * single_tone_duration);  // 計算總週期數。

    for (int i = 0; i < cycles && !stop; i++) {  // 在指定的週期內發出音調，直到信號處理程序停止。
        sound_tone(TONE_FREQUENCY1, single_tone_duration);  // 發出頻率1的音調。
        sound_tone(TONE_FREQUENCY2, single_tone_duration);  // 發出頻率2的音調。
    }
}

int main() {  // 主函數。
    int fd, gas_level;  // 定義文件描述符和氣體濃度變數。
    char buffer[BUFFER_SIZE];  // 定義緩衝區以存儲讀取數據。
    ssize_t bytes_read;  // 定義實際讀取的字節數。
    time_t last_read_time = 0;  // 記錄上次讀取時間。
    struct sigaction sa;  // 定義信號處理結構。

    setup_gpio();  // 設置GPIO腳位。

    sa.sa_handler = inthand;  // 設置信號處理函數。
    sigemptyset(&sa.sa_mask); // 初始化信號集為空。
    sa.sa_flags = 0;  // 設置信號處理標誌為0。
    sigaction(SIGINT, &sa, NULL);  // 設置SIGINT信號的處理程序。

    fd = open(DEVICE_FILE, O_RDONLY);  // 打開設備文件。
    if (fd < 0) {  // 如果打開設備文件失敗。
        perror("Failed to open the device file"); // 打印錯誤信息。
        set_led(BLUE_LED);  // 點亮藍色LED。
        return EXIT_FAILURE; // 返回失敗狀態。
    }

    printf("Gas Monitor started. Press Ctrl+C to exit.\n");  // 打印啟動信息。

    while (!stop) {  // 當標誌變數為0時持續循環。
        lseek(fd, 0, SEEK_SET);  // 重置文件偏移量。
        bytes_read = read(fd, buffer, sizeof(buffer) - 1);  // 讀取數據。

        time_t current_time = time(NULL);  // 獲取當前時間。

        if (bytes_read > 0) {  // 如果成功讀取數據。
            buffer[bytes_read] = '\0';  // 終止字符串。
            gas_level = atoi(buffer);  // 將字符串轉換為整數。
            printf("Time: %ld, Gas level: %d\n", current_time, gas_level);  // 打印氣體濃度。

            if (gas_level == 0) {  // 如果氣體濃度為0。
                printf("WARNING: Gas level is zero. Sensor may be faulty or disconnected.\n");  // 打印警告信息。
                set_led(BLUE_LED);  // 點亮藍色LED。
                set_gpio_value(BUZZER, 0);  // 關閉蜂鳴器。
            } else if (gas_level > 200) {  // 如果氣體濃度高於200。
                printf("ALERT: High gas concentration detected!\n");  // 打印警報信息。
                set_led(RED_LED);  // 點亮紅色LED。
                sound_alarm(2000);  // 發出2秒鐘的警報音。
            } else {  // 如果氣體濃度正常。
                printf("Normal gas levels.\n");  // 打印正常信息。
                set_led(GREEN_LED);  // 點亮綠色LED。
                set_gpio_value(BUZZER, 0);  // 關閉蜂鳴器。
            }

            last_read_time = current_time;  // 更新上次讀取時間。
        } else if (bytes_read == 0) {  // 如果沒有新數據。
            printf("Time: %ld, No new data available\n", current_time);  // 打印無數據信息。
        } else if (bytes_read < 0) {  // 如果讀取數據出錯。
            if (errno != EINTR) {  // 忽略由Ctrl+C引起的中斷。
                perror("Failed to read from device");  // 打印錯誤信息。
                set_led(BLUE_LED);  // 點亮藍色LED。
                break;  // 跳出循環。
            }
        }

        if (current_time - last_read_time > 5) {  // 如果超過5秒沒有新數據。
            printf("No new data for 5 seconds. Device may be unresponsive.\n");  // 打印設備無響應信息。
            set_led(BLUE_LED);  // 點亮藍色LED。
        }

        if (!stop) {  // 如果沒有收到停止信號。
            sleep(1);  // 每秒休眠1秒。
        }
    }

    close(fd);  // 關閉設備文件。
    // 關閉所有LED和蜂鳴器
    set_gpio_value(RED_LED, 0);   // 關閉紅色LED。
    set_gpio_value(GREEN_LED, 0); // 關閉綠色LED。
    set_gpio_value(BLUE_LED, 0);  // 關閉藍色LED。
    set_gpio_value(BUZZER, 0);    // 關閉蜂鳴器。

    printf("Gas Monitor stopped.\n");  // 打印停止信息。
    return EXIT_SUCCESS;  // 返回成功狀態。
}
