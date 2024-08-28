#include <linux/module.h>          // �]�t���ּҲթһݪ����Y���C
#include <linux/kernel.h>          // �]�t���֨�ƩM���w�q���Y���C
#include <linux/init.h>            // �]�t��l�ƩM�M�z���w�q���Y���C
#include <linux/fs.h>              // �]�t�ɮרt�ά�����ƩM���c���Y���C
#include <linux/uaccess.h>         // �]�t�Τ�Ŷ��X�ݬ�����ƪ��Y���C
#include <linux/spi/spi.h>         // �]�tSPI������ƩM���c���Y���C
#include <linux/device.h>          // �]�t�]�Ƽҫ�������ƩM���c���Y���C
#include <linux/kthread.h>         // �]�t���ֽu�{������ƪ��Y���C
#include <linux/delay.h>           // �]�t�����ƪ��Y���C
#include <linux/of.h>              // �]�t�]�ƾ������ƩM���c���Y���C

#define DEVICE_NAME "gas_detector" // �w�q�r�ų]�ƪ��W�٬� "gas_detector"�C
#define CLASS_NAME "gas"           // �w�q�]�����O�W�٬� "gas"�C
#define GAS_DETECTOR_NODE "gas_detector" // �w�q�]�ƾ�`�I���W�٬� "gas_detector"�C
#define READ_INTERVAL_MS 1000      // �w�qŪ������@�ת����j�� 1000 �@��]1 ��^�C

static struct spi_device *spi_device = NULL; // ���VSPI�]�Ƶ��c�����w�A��l�Ƭ�NULL�C
static int major_number;                     // �s�x�r�ų]�ƪ��D�]�Ƹ��C
static struct class* gas_detector_class = NULL; // ���V�]�����O���c�����w�A��l�Ƭ�NULL�C
static struct device* gas_detector_device = NULL; // ���V�]�Ƶ��c�����w�A��l�Ƭ�NULL�C
static int gas_level = 0;                  // �s�x����@�ת��ܼơA��l�Ƭ�0�C
static struct task_struct *task = NULL;   // ���V���ֽu�{���c�����w�A��l�Ƭ�NULL�C
static int should_stop = 0;               // �лx�ܼơA�Ω���ܽu�{�O�_���Ӱ���A��l�Ƭ�0�C

static int read_mcp3008(int channel) {      // �w�qŪ��MCP3008������ƾ�����ơA�ѼƬ��q�D���C
    u8 tx[] = { 1, (8 + channel) << 4, 0 }; // �n�o�e���ƾڡ]SPI��ĳ�榡�^�A��l�Ƭ�Ū�����w�q�D�C
    u8 rx[3];                              // �����ƾڪ��w�İϡC
    struct spi_transfer t = {              // �w�qSPI�ǿ鵲�c�C
        .tx_buf = tx,                      // �]�m�o�e�ƾڪ��w�İϡC
        .rx_buf = rx,                      // �]�m�����ƾڪ��w�İϡC
        .len = sizeof(tx),                // �]�m�ǿ�ƾڪ����סC
    };
    struct spi_message m;                  // �w�qSPI�������c�C
    int value;                             // �s�x�p��o�쪺�ȡC

    spi_message_init(&m);                  // ��l��SPI�������c�C
    spi_message_add_tail(&t, &m);          // �NSPI�ǿ鵲�c�K�[��SPI�������C
    spi_sync(spi_device, &m);             // ����SPI�P�B�ǿ�A���ݶǿ駹���C

    value = ((rx[1] & 3) << 8) + rx[2];   // �q�����ƾڤ������íp��̲ת��ƭȡC
    printk(KERN_DEBUG "Gas Detector: SPI transfer - tx: %02x %02x %02x, rx: %02x %02x %02x\n",
           tx[0], tx[1], tx[2], rx[0], rx[1], rx[2]); // ���LSPI�ǿ骺�ոիH���C
    printk(KERN_INFO "Gas Detector: Calculated value %d from channel %d\n", value, channel); // ���L�p��o�쪺����@�סC

    return value;                          // ��^�p��o�쪺�ƭȡC
}

static int gas_detector_thread(void *data) {  // �w�q���ֽu�{��ơA�Ω�w��Ū������@�סC
    while (!kthread_should_stop() && !should_stop) { // ��u�{�����Ӱ���B�лx���]�m�ɰ���C
        gas_level = read_mcp3008(0);  // �q�q�D0Ū������@�סC
        msleep(READ_INTERVAL_MS);     // ������w���ɶ��]1000�@��^�C
    }
    printk(KERN_INFO "Gas Detector: Thread stopped\n"); // ���L�u�{����H���C
    return 0;                              // ��^0�A��ܽu�{���`�h�X�C
}

static int gas_detector_open(struct inode *inode, struct file *file) {  // ���}�]�ƮɽեΪ���ơC
    printk(KERN_INFO "Gas Detector: Device opened\n"); // ���L�]�ƥ��}���H���C
    return 0;  // ��^0�A��ܥ��}�ާ@���\�C
}

static int gas_detector_release(struct inode *inode, struct file *file) {  // �����]�ƮɽեΪ���ơC
    printk(KERN_INFO "Gas Detector: Device closed\n"); // ���L�]���������H���C
    return 0;  // ��^0�A��������ާ@���\�C
}

static ssize_t gas_detector_read(struct file *file, char __user *buf, size_t count, loff_t *offset) {  // Ū���]�ƮɽեΪ���ơC
    char tmp[16];  // �w�q�{�ɦr�żƲըӦs�x����@�ת��r�Ŧ��ܡC
    int len = snprintf(tmp, sizeof(tmp), "%d\n", gas_level);  // �N����@�׮榡�Ƭ��r�Ŧ�æs�Jtmp���C
    
    if (count < len)  // �p�G�Τᴣ�Ѫ��w�İϤj�p�����H�e�ǼƾڡA��^���~�C
        return -EINVAL;
    
    if (copy_to_user(buf, tmp, len))  // �N�ƾڱq���֪Ŷ�������Τ�Ŷ��C
        return -EFAULT;
    
    return len;  // ��^��ګ������r�`�ơC
}

static struct file_operations fops = {  // �w�q���ާ@���c�A�Ω�r�ų]�ƪ��ާ@��ơC
    .open = gas_detector_open,      // �]�m���}�ާ@��ơC
    .release = gas_detector_release, // �]�m�����ާ@��ơC
    .read = gas_detector_read,      // �]�mŪ���ާ@��ơC
};

static int __init gas_detector_init(void) {  // �Ҷ���l�ƨ�ơC
    int ret;
    struct device_node *node;

    printk(KERN_INFO "Gas Detector: Initializing module\n"); // ���L�Ҷ���l�ƪ��H���C

    gas_detector_class = class_create(THIS_MODULE, CLASS_NAME); // �Ыئr�ų]�ƪ����O�C
    if (IS_ERR(gas_detector_class)) {  // �p�G�Ы����O���ѡC
        printk(KERN_ALERT "Gas Detector: Failed to register device class\n"); // ���L���~�H���C
        return PTR_ERR(gas_detector_class); // ��^���~�N�X�C
    }

    major_number = register_chrdev(0, DEVICE_NAME, &fops); // ���U�r�ų]�ơA���t�@�ӥD�]�Ƹ��C
    if (major_number < 0) {  // �p�G���U�r�ų]�ƥ��ѡC
        printk(KERN_ALERT "Gas Detector: Failed to register a major number\n"); // ���L���~�H���C
        class_destroy(gas_detector_class); // �P���]�����O�C
        return major_number; // ��^���~�N�X�C
    }

    gas_detector_device = device_create(gas_detector_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME); // �Ыئr�ų]�ơC
    if (IS_ERR(gas_detector_device)) {  // �p�G�Ыس]�ƥ��ѡC
        printk(KERN_ALERT "Gas Detector: Failed to create the device\n"); // ���L���~�H���C
        unregister_chrdev(major_number, DEVICE_NAME); // �`�P�r�ų]�ơC
        class_destroy(gas_detector_class); // �P���]�����O�C
        return PTR_ERR(gas_detector_device); // ��^���~�N�X�C
    }

    node = of_find_node_by_name(NULL, GAS_DETECTOR_NODE); // �d��]�ƾ𤤫��w�W�٪��`�I�C
    if (!node) {  // �p�G�䤣��`�I�C
        printk(KERN_ALERT "Gas Detector: Could not find '%s' node\n", GAS_DETECTOR_NODE); // ���L���~�H���C
        goto error_cleanup; // �������~�B�z�����C
    }

    spi_device = of_find_spi_device_by_node(node); // �q�]�ƾ�`�I���d��SPI�]�ơC
    of_node_put(node); // ����]�ƾ�`�I���ޥΡC

    if (!spi_device) {  // �p�G�䤣��SPI�]�ơC
        printk(KERN_ALERT "Gas Detector: Could not find SPI device\n"); // ���L���~�H���C
        goto error_cleanup; // �������~�B�z�����C
    }

    ret = spi_setup(spi_device); // �]�mSPI�]�ưѼơC
    if (ret) {  // �p�G�]�mSPI�]�ƥ��ѡC
        printk(KERN_ALERT "Gas Detector: Failed to setup SPI device: %d\n", ret); // ���L���~�H���C
        goto error_cleanup; // �������~�B�z�����C
    }

    task = kthread_run(gas_detector_thread, NULL, "gas_detector_thread"); // �Ыؤ��ֽu�{�A��������˴���ơC
    if (IS_ERR(task)) {  // �p�G�Ыؽu�{���ѡC
        printk(KERN_ALERT "Gas Detector: Failed to create kernel thread\n"); // ���L���~�H���C
        goto error_cleanup; // �������~�B�z�����C
    }

    printk(KERN_INFO "Gas Detector: Module loaded successfully\n"); // ���L�Ҷ��[�����\���H���C
    return 0;  // ��^0�A��ܪ�l�Ʀ��\�C

error_cleanup:  // ���~�B�z�����C
    if (spi_device) {  // �p�GSPI�]�Ʀs�b�C
        spi_unregister_device(spi_device); // �`�PSPI�]�ơC
    }
    if (gas_detector_device) {  // �p�G�r�ų]�Ʀs�b�C
        device_destroy(gas_detector_class, MKDEV(major_number, 0)); // �P���r�ų]�ơC
    }
    if (major_number >= 0) {  // �p�G�D�]�Ƹ����ġC
        unregister_chrdev(major_number, DEVICE_NAME); // �`�P�r�ų]�ơC
    }
    if (gas_detector_class) {  // �p�G�]�����O�s�b�C
        class_destroy(gas_detector_class); // �P���]�����O�C
    }
    return -ENODEV;  // ��^�]�ƿ��~�N�X�C
}

static void __exit gas_detector_exit(void) {  // �Ҷ�������ơC
    printk(KERN_INFO "Gas Detector: Unloading module\n"); // ���L�Ҷ��������H���C

    should_stop = 1;  // �]�m�лx�ܼơA���ܽu�{���Ӱ���C
    if (task) {  // �p�G���ֽu�{�s�b�C
        kthread_stop(task); // ����ֽu�{�C
        task = NULL; // �]�m�u�{���w��NULL�C
    }

    device_destroy(gas_detector_class, MKDEV(major_number, 0)); // �P���r�ų]�ơC
    unregister_chrdev(major_number, DEVICE_NAME); // �`�P�r�ų]�ơC
    class_destroy(gas_detector_class); // �P���]�����O�C

    printk(KERN_INFO "Gas Detector: Module unloaded\n"); // ���L�Ҷ��������\���H���C
}

module_init(gas_detector_init);  // ���U�Ҷ���l�ƨ�ơC
module_exit(gas_detector_exit);  // ���U�Ҷ�������ơC

MODULE_LICENSE("GPL");  // �]�m�Ҷ������v��ĳ��GPL�C
MODULE_AUTHOR("Your Name");  // �]�m�Ҷ����@�̫H���C
MODULE_DESCRIPTION("Gas Detector Driver");  // �]�m�Ҷ����y�z�H���C
