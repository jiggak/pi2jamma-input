#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/gpio.h>
// TODO figure out how to use new API, <linux/gpio.h> is depricated
// #include <linux/gpio/consumer.h>
#include <linux/kthread.h>
#include <linux/delay.h>

// #define _74x165_CLK 23 // Pin 16
// #define _74x165_PL  24 // Pin 18
// #define _74x165_DIN 22 // Pin 15
// use `cat /sys/kernel/debug/gpio` to find gpio number, e.g. gpio-535 (GPIO23)
#define _74x165_CLK 535 // Pin 16
#define _74x165_PL  536 // Pin 18
#define _74x165_DIN 534 // Pin 15
#define _74x165_BITS 24

static struct input_dev *pi2jamma_dev;
static struct task_struct *pi2jamma_thread;

/* keymap is in order of bits read from pi2jamma */
static int keymap[_74x165_BITS] = {
    KEY_SPACE,     // P1B3
    KEY_LEFTSHIFT, // P1B4
    KEY_Z,         // P1B5
    KEY_X,         // P1B6
    KEY_I,         // P2B5
    KEY_K,         // P2B6
    KEY_W,         // P2B4
    KEY_Q,         // P2B3
    KEY_LEFT,      // P1Left
    KEY_RIGHT,     // P1Right
    KEY_LEFTCTRL,  // P1B1
    KEY_LEFTALT,   // P1B2
    KEY_S,         // P2B2
    KEY_A,         // P2B1
    KEY_G,         // P2Right
    KEY_D,         // P2Left
    KEY_5,         // P1Coin
    KEY_1,         // P1Start
    KEY_UP,        // P1Up
    KEY_DOWN,      // P1Down
    KEY_F,         // P2Down
    KEY_R,         // P2Up
    KEY_2,         // P2Start
    KEY_6          // P2Coin
};

uint32_t read_controller_bits(void) {
    uint32_t result = 0;

    gpio_set_value(_74x165_CLK, 1);
    udelay(2);
    gpio_set_value(_74x165_PL, 0);
    udelay(2);
    gpio_set_value(_74x165_PL, 1);

    for (int i=0; i<_74x165_BITS; i++) {
        result = result << 1;

        udelay(2);
        uint8_t value = gpio_get_value(_74x165_DIN);

        // key state: 1 = off, 0 = on
        result = result | (!value);

        gpio_set_value(_74x165_CLK, 0);
        udelay(2);
        gpio_set_value(_74x165_CLK, 1);
    }

    return result;
}

int pi2jamma_polling_thread(void *data) {
    uint32_t bits;

    while (!kthread_should_stop()) {
        bits = read_controller_bits();

        // if (bits) {
        //     printk("Non zero bits %x\n", bits);
        // }

        for (int i=0; i<_74x165_BITS; i++) {
            input_report_key(pi2jamma_dev, keymap[i], bits & 1);
            bits = bits >> 1;
        }

        input_sync(pi2jamma_dev);

        // failed attempt at limiting heavy CPU usage
        // udelay(10000);
    }

    return 0;
}

static int __init pi2jamma_init(void) {
    int error;
    int gpio_init[3] = { 0, 0, 0 };

    printk("pi2jamma_input init\n");

    pi2jamma_dev = input_allocate_device();
    if (!pi2jamma_dev) {
        printk(KERN_ERR "pi2jamma_input: Not enough memory\n");
        error = -ENOMEM;
        goto err_return;
    }

    pi2jamma_dev->evbit[0] = BIT_MASK(EV_KEY);
    for (int i=0; i<_74x165_BITS; i++) {
        set_bit(keymap[i], pi2jamma_dev->keybit);
    }

    error = input_register_device(pi2jamma_dev);
    if (error) {
        printk(KERN_ERR "pi2jamma_input: Failed to register device\n");
        goto err_free_dev;
    }

    // bcm2835_gpio_fsel(CLK, BCM2835_GPIO_FSEL_OUTP);
    // bcm2835_gpio_fsel(PL, BCM2835_GPIO_FSEL_OUTP);
    // bcm2835_gpio_fsel(DIN, BCM2835_GPIO_FSEL_INPT);

    error = gpio_request(_74x165_CLK, "74x165-clk");
    if (error) {
        printk(KERN_ERR "pi2jamma_input: Failed to request gpio for CLK\n");
        goto err_free_gpio;
    }
    gpio_init[0] = _74x165_CLK;
    error = gpio_direction_output(_74x165_CLK, 0);
    if (error) {
        printk(KERN_ERR "pi2jamma_input: Failed to set gpio direction for CLK\n");
        goto err_free_gpio;
    }

    error = gpio_request(_74x165_PL, "74x165-pl");
    if (error) {
        printk(KERN_ERR "pi2jamma_input: Failed to request gpio for PL\n");
        goto err_free_gpio;
    }
    gpio_init[1] = _74x165_PL;
    error = gpio_direction_output(_74x165_PL, 0);
    if (error) {
        printk(KERN_ERR "pi2jamma_input: Failed to set gpio direction for PL\n");
        goto err_free_gpio;
    }

    error = gpio_request(_74x165_DIN, "74x165-din");
    if (error) {
        printk(KERN_ERR "pi2jamma_input: Failed to request gpio for DIN\n");
        goto err_free_gpio;
    }
    gpio_init[2] = _74x165_DIN;
    error = gpio_direction_input(_74x165_DIN);
    if (error) {
        printk(KERN_ERR "pi2jamma_input: Failed to set gpio direction for DIN\n");
        goto err_free_gpio;
    }

    pi2jamma_thread = kthread_run(pi2jamma_polling_thread, NULL, "pi2jamma_thread");
    if (pi2jamma_thread == NULL) {
        printk(KERN_ERR "pi2jamma_input: Failed to start polling thread\n");
        error = -1;
        goto err_free_gpio;
    }

    return 0;

err_free_gpio:
    for (int i = 0; i<3; i++) {
        if (gpio_init[i]) {
            gpio_free(gpio_init[i]);
        }
    }
err_free_dev:
    input_free_device(pi2jamma_dev);
err_return:
    return error;
}

static void __exit pi2jamma_exit(void) {
    input_unregister_device(pi2jamma_dev);
    gpio_free(_74x165_CLK);
    gpio_free(_74x165_PL);
    gpio_free(_74x165_DIN);
    kthread_stop(pi2jamma_thread);
}

module_init(pi2jamma_init);
module_exit(pi2jamma_exit);

MODULE_LICENSE("GPL");