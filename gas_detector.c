#include <linux/module.h>          // 包含內核模組所需的基本頭文件。
#include <linux/kernel.h>          // 包含內核函數和宏定義的頭文件。
#include <linux/init.h>            // 包含初始化和清理宏定義的頭文件。
#include <linux/fs.h>              // 包含檔案系統相關函數和結構的頭文件。
#include <linux/uaccess.h>         // 包含用戶空間訪問相關函數的頭文件。
#include <linux/spi/spi.h>         // 包含SPI相關函數和結構的頭文件。
#include <linux/device.h>          // 包含設備模型相關函數和結構的頭文件。
#include <linux/kthread.h>         // 包含內核線程相關函數的頭文件。
#include <linux/delay.h>           // 包含延遲函數的頭文件。
#include <linux/of.h>              // 包含設備樹相關函數和結構的頭文件。

#define DEVICE_NAME "gas_detector" // 定義字符設備的名稱為 "gas_detector"。
#define CLASS_NAME "gas"           // 定義設備類別名稱為 "gas"。
#define GAS_DETECTOR_NODE "gas_detector" // 定義設備樹節點的名稱為 "gas_detector"。
#define READ_INTERVAL_MS 1000      // 定義讀取氣體濃度的間隔為 1000 毫秒（1 秒）。

static struct spi_device *spi_device = NULL; // 指向SPI設備結構的指針，初始化為NULL。
static int major_number;                     // 存儲字符設備的主設備號。
static struct class* gas_detector_class = NULL; // 指向設備類別結構的指針，初始化為NULL。
static struct device* gas_detector_device = NULL; // 指向設備結構的指針，初始化為NULL。
static int gas_level = 0;                  // 存儲氣體濃度的變數，初始化為0。
static struct task_struct *task = NULL;   // 指向內核線程結構的指針，初始化為NULL。
static int should_stop = 0;               // 標誌變數，用於指示線程是否應該停止，初始化為0。

static int read_mcp3008(int channel) {      // 定義讀取MCP3008模擬轉數器的函數，參數為通道號。
    u8 tx[] = { 1, (8 + channel) << 4, 0 }; // 要發送的數據（SPI協議格式），初始化為讀取指定通道。
    u8 rx[3];                              // 接收數據的緩衝區。
    struct spi_transfer t = {              // 定義SPI傳輸結構。
        .tx_buf = tx,                      // 設置發送數據的緩衝區。
        .rx_buf = rx,                      // 設置接收數據的緩衝區。
        .len = sizeof(tx),                // 設置傳輸數據的長度。
    };
    struct spi_message m;                  // 定義SPI消息結構。
    int value;                             // 存儲計算得到的值。

    spi_message_init(&m);                  // 初始化SPI消息結構。
    spi_message_add_tail(&t, &m);          // 將SPI傳輸結構添加到SPI消息中。
    spi_sync(spi_device, &m);             // 執行SPI同步傳輸，等待傳輸完成。

    value = ((rx[1] & 3) << 8) + rx[2];   // 從接收數據中提取並計算最終的數值。
    printk(KERN_DEBUG "Gas Detector: SPI transfer - tx: %02x %02x %02x, rx: %02x %02x %02x\n",
           tx[0], tx[1], tx[2], rx[0], rx[1], rx[2]); // 打印SPI傳輸的調試信息。
    printk(KERN_INFO "Gas Detector: Calculated value %d from channel %d\n", value, channel); // 打印計算得到的氣體濃度。

    return value;                          // 返回計算得到的數值。
}

static int gas_detector_thread(void *data) {  // 定義內核線程函數，用於定期讀取氣體濃度。
    while (!kthread_should_stop() && !should_stop) { // 當線程不應該停止且標誌未設置時執行。
        gas_level = read_mcp3008(0);  // 從通道0讀取氣體濃度。
        msleep(READ_INTERVAL_MS);     // 延遲指定的時間（1000毫秒）。
    }
    printk(KERN_INFO "Gas Detector: Thread stopped\n"); // 打印線程停止的信息。
    return 0;                              // 返回0，表示線程正常退出。
}

static int gas_detector_open(struct inode *inode, struct file *file) {  // 打開設備時調用的函數。
    printk(KERN_INFO "Gas Detector: Device opened\n"); // 打印設備打開的信息。
    return 0;  // 返回0，表示打開操作成功。
}

static int gas_detector_release(struct inode *inode, struct file *file) {  // 關閉設備時調用的函數。
    printk(KERN_INFO "Gas Detector: Device closed\n"); // 打印設備關閉的信息。
    return 0;  // 返回0，表示關閉操作成功。
}

static ssize_t gas_detector_read(struct file *file, char __user *buf, size_t count, loff_t *offset) {  // 讀取設備時調用的函數。
    char tmp[16];  // 定義臨時字符數組來存儲氣體濃度的字符串表示。
    int len = snprintf(tmp, sizeof(tmp), "%d\n", gas_level);  // 將氣體濃度格式化為字符串並存入tmp中。
    
    if (count < len)  // 如果用戶提供的緩衝區大小不足以容納數據，返回錯誤。
        return -EINVAL;
    
    if (copy_to_user(buf, tmp, len))  // 將數據從內核空間拷貝到用戶空間。
        return -EFAULT;
    
    return len;  // 返回實際拷貝的字節數。
}

static struct file_operations fops = {  // 定義文件操作結構，用於字符設備的操作函數。
    .open = gas_detector_open,      // 設置打開操作函數。
    .release = gas_detector_release, // 設置關閉操作函數。
    .read = gas_detector_read,      // 設置讀取操作函數。
};

static int __init gas_detector_init(void) {  // 模塊初始化函數。
    int ret;
    struct device_node *node;

    printk(KERN_INFO "Gas Detector: Initializing module\n"); // 打印模塊初始化的信息。

    gas_detector_class = class_create(THIS_MODULE, CLASS_NAME); // 創建字符設備的類別。
    if (IS_ERR(gas_detector_class)) {  // 如果創建類別失敗。
        printk(KERN_ALERT "Gas Detector: Failed to register device class\n"); // 打印錯誤信息。
        return PTR_ERR(gas_detector_class); // 返回錯誤代碼。
    }

    major_number = register_chrdev(0, DEVICE_NAME, &fops); // 註冊字符設備，分配一個主設備號。
    if (major_number < 0) {  // 如果註冊字符設備失敗。
        printk(KERN_ALERT "Gas Detector: Failed to register a major number\n"); // 打印錯誤信息。
        class_destroy(gas_detector_class); // 銷毀設備類別。
        return major_number; // 返回錯誤代碼。
    }

    gas_detector_device = device_create(gas_detector_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME); // 創建字符設備。
    if (IS_ERR(gas_detector_device)) {  // 如果創建設備失敗。
        printk(KERN_ALERT "Gas Detector: Failed to create the device\n"); // 打印錯誤信息。
        unregister_chrdev(major_number, DEVICE_NAME); // 注銷字符設備。
        class_destroy(gas_detector_class); // 銷毀設備類別。
        return PTR_ERR(gas_detector_device); // 返回錯誤代碼。
    }

    node = of_find_node_by_name(NULL, GAS_DETECTOR_NODE); // 查找設備樹中指定名稱的節點。
    if (!node) {  // 如果找不到節點。
        printk(KERN_ALERT "Gas Detector: Could not find '%s' node\n", GAS_DETECTOR_NODE); // 打印錯誤信息。
        goto error_cleanup; // 跳轉到錯誤處理部分。
    }

    spi_device = of_find_spi_device_by_node(node); // 從設備樹節點中查找SPI設備。
    of_node_put(node); // 釋放設備樹節點的引用。

    if (!spi_device) {  // 如果找不到SPI設備。
        printk(KERN_ALERT "Gas Detector: Could not find SPI device\n"); // 打印錯誤信息。
        goto error_cleanup; // 跳轉到錯誤處理部分。
    }

    ret = spi_setup(spi_device); // 設置SPI設備參數。
    if (ret) {  // 如果設置SPI設備失敗。
        printk(KERN_ALERT "Gas Detector: Failed to setup SPI device: %d\n", ret); // 打印錯誤信息。
        goto error_cleanup; // 跳轉到錯誤處理部分。
    }

    task = kthread_run(gas_detector_thread, NULL, "gas_detector_thread"); // 創建內核線程，執行氣體檢測函數。
    if (IS_ERR(task)) {  // 如果創建線程失敗。
        printk(KERN_ALERT "Gas Detector: Failed to create kernel thread\n"); // 打印錯誤信息。
        goto error_cleanup; // 跳轉到錯誤處理部分。
    }

    printk(KERN_INFO "Gas Detector: Module loaded successfully\n"); // 打印模塊加載成功的信息。
    return 0;  // 返回0，表示初始化成功。

error_cleanup:  // 錯誤處理部分。
    if (spi_device) {  // 如果SPI設備存在。
        spi_unregister_device(spi_device); // 注銷SPI設備。
    }
    if (gas_detector_device) {  // 如果字符設備存在。
        device_destroy(gas_detector_class, MKDEV(major_number, 0)); // 銷毀字符設備。
    }
    if (major_number >= 0) {  // 如果主設備號有效。
        unregister_chrdev(major_number, DEVICE_NAME); // 注銷字符設備。
    }
    if (gas_detector_class) {  // 如果設備類別存在。
        class_destroy(gas_detector_class); // 銷毀設備類別。
    }
    return -ENODEV;  // 返回設備錯誤代碼。
}

static void __exit gas_detector_exit(void) {  // 模塊卸載函數。
    printk(KERN_INFO "Gas Detector: Unloading module\n"); // 打印模塊卸載的信息。

    should_stop = 1;  // 設置標誌變數，指示線程應該停止。
    if (task) {  // 如果內核線程存在。
        kthread_stop(task); // 停止內核線程。
        task = NULL; // 設置線程指針為NULL。
    }

    device_destroy(gas_detector_class, MKDEV(major_number, 0)); // 銷毀字符設備。
    unregister_chrdev(major_number, DEVICE_NAME); // 注銷字符設備。
    class_destroy(gas_detector_class); // 銷毀設備類別。

    printk(KERN_INFO "Gas Detector: Module unloaded\n"); // 打印模塊卸載成功的信息。
}

module_init(gas_detector_init);  // 註冊模塊初始化函數。
module_exit(gas_detector_exit);  // 註冊模塊卸載函數。

MODULE_LICENSE("GPL");  // 設置模塊的授權協議為GPL。
MODULE_AUTHOR("Your Name");  // 設置模塊的作者信息。
MODULE_DESCRIPTION("Gas Detector Driver");  // 設置模塊的描述信息。
