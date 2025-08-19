#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include "../../src/led_blink.h"

ZTEST(app_led_blink, test_led_init_returns_success)
{
    printk("Led init test\n");
    int ret = init_led();
    zassert_equal(ret, 0, "LED init should succeed on hardware");
}

ZTEST(app_led_blink, test_led_blink_pattern)
{
    printk("blinking LED twice with 200 ms...\n");
    init_led();
    blink_led(2, 200); //2 quick blinks

    // cant zassert led blink since there is nor return
    k_msleep(1000); //give the system time to finish blinking
}

ZTEST(app_led_blink, test_blink_zero_times)
{
    printk("🧪 Testing zero blinks...\n");
    init_led();
    blink_led(0, 200); // Expect: no LED activity
    k_msleep(1000);
}

ZTEST(app_led_blink, test_blink_negative_delay)
{
    printk("🧪 Testing with negative delay (should be ignored by k_msleep)...\n");
    init_led();
    blink_led(2, -100); // Undefined behavior, but should not crash
    k_msleep(1000);
}

ZTEST(app_led_blink, test_blink_unusually_long)
{
    printk("Blinking once with long delay (1.5s)...\n");
    init_led();
    blink_led(1, 1500); // Useful for visual feedback
    k_msleep(1000);
}

ZTEST(app_led_blink, test_double_blink_signal)
{
    printk("Testing double blink for GPS fix...\n");
    init_led();
    blink_led(2, 150);
    k_msleep(1000);
}

ZTEST(app_led_blink, test_wifi_disconnected_burst)
{
    printk("Wi-Fi disconnect signal (5 rapid blinks)...\n");
    init_led();
    blink_led(5, 100);
    k_msleep(1000);
}


ZTEST_SUITE(app_led_blink, NULL, NULL, NULL, NULL, NULL);
