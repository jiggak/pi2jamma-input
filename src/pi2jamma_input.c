#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/property.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>

// 3x shirft registers, 8 bits each
#define _74x165_BITS 24

static struct input_dev *input_dev;
static struct gpio_desc *clk_gpio, *pl_gpio, *din_gpio;

// keymap is in order of bits read from pi2jamma
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

static uint32_t pi2jamma_read_buttons(void) {
    uint32_t result = 0;

    gpiod_set_value(clk_gpio, 1);
    udelay(2);
    gpiod_set_value(pl_gpio, 0);
    udelay(2);
    gpiod_set_value(pl_gpio, 1);

    for (int i=0; i<_74x165_BITS; i++) {
        result = result << 1;

        udelay(2);
        uint8_t value = gpiod_get_value(din_gpio);

        // key state: 1 = off, 0 = on
        result = result | (!value);

        gpiod_set_value(clk_gpio, 0);
        udelay(2);
        gpiod_set_value(clk_gpio, 1);
    }

    return result;
}

static void pi2jamma_poller(struct input_dev *input) {
    uint32_t bits;

    bits = pi2jamma_read_buttons();

    for (int i=0; i<_74x165_BITS; i++) {
        input_report_key(input_dev, keymap[i], bits & 1);
        bits = bits >> 1;
    }

    input_sync(input_dev);
}

static const struct of_device_id pi2jamma_input_of_match[] = {
    { .compatible = "pi2jamma-input", },
    { },
};
MODULE_DEVICE_TABLE(of, pi2jamma_input_of_match);

static int pi2jamma_probe(struct platform_device *pdev) {
    struct device *dev = &pdev->dev;
    int poll_interval, error;

    if (device_property_read_u32(dev, "poll-interval", &poll_interval)) {
        printk(KERN_ERR "pi2jamma_input: Failed to read 'poll-interval' property\n");
        return -ENOMEM;
    }

    clk_gpio = gpiod_get(dev, "clk", GPIOD_OUT_HIGH);
    pl_gpio = gpiod_get(dev, "pl", GPIOD_OUT_HIGH);
    din_gpio = gpiod_get(dev, "din", GPIOD_IN);

    if (IS_ERR(clk_gpio)) {
        error = PTR_ERR(clk_gpio);
        printk(KERN_ERR "pi2jamma_input: Failed to setup clk gpio\n");
        goto err_free_gpio;
    }

    if (IS_ERR(pl_gpio)) {
        error = PTR_ERR(pl_gpio);
        printk(KERN_ERR "pi2jamma_input: Failed to setup pl gpio\n");
        goto err_free_gpio;
    }

    if (IS_ERR(din_gpio)) {
        error = PTR_ERR(din_gpio);
        printk(KERN_ERR "pi2jamma_input: Failed to setup din gpio\n");
        goto err_free_gpio;
    }

    // devm_input_allocate_device() creates a managed instance that does not
    // need to be explicitly free'd or unregistered
    // using input_allocate_device() here causes input_register_device() to block
    input_dev = devm_input_allocate_device(dev);
    if (!input_dev) {
        printk(KERN_ERR "pi2jamma_input: Not enough memory\n");
        error = -ENOMEM;
        goto err_free_gpio;
    }

    // input_dev->evbit[0] = BIT_MASK(EV_KEY);
    set_bit(EV_KEY, input_dev->evbit);
    // set_bit(EV_REP, input_dev->evbit);
    for (int i=0; i<_74x165_BITS; i++) {
        set_bit(keymap[i], input_dev->keybit);
    }

    // using input subsystem polling is much nicer on CPU usage vs creating a
    // thread and doing our own polling
    error = input_setup_polling(input_dev, pi2jamma_poller);
    if (error) {
        printk(KERN_ERR "pi2jamma_input: Failed to setup input polling\n");
        goto err_free_gpio;
    }

    // poll interval in millis
    input_set_poll_interval(input_dev, poll_interval);

    error = input_register_device(input_dev);
    if (error) {
        printk(KERN_ERR "pi2jamma_input: Failed to register input device\n");
        goto err_free_gpio;
    }

    return 0;

err_free_gpio:
    if (!IS_ERR(clk_gpio)) gpiod_put(clk_gpio);
    if (!IS_ERR(pl_gpio)) gpiod_put(pl_gpio);
    if (!IS_ERR(din_gpio)) gpiod_put(din_gpio);

    return error;
}

static void pi2jamma_shutdown(struct platform_device *pdev) {
    gpiod_put(clk_gpio);
    gpiod_put(pl_gpio);
    gpiod_put(din_gpio);
}

static struct platform_driver pi2jamma_input_device_driver = {
    .probe = pi2jamma_probe,
    .remove = pi2jamma_shutdown,
    .driver = {
        .name = "pi2jamma",
        .of_match_table = pi2jamma_input_of_match
    }
};

static int __init pi2jamma_init(void) {
    return platform_driver_register(&pi2jamma_input_device_driver);
}

static void __exit pi2jamma_exit(void) {
    platform_driver_unregister(&pi2jamma_input_device_driver);
}

module_init(pi2jamma_init);
module_exit(pi2jamma_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Josh Kropf");