#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <u8g2.h>
#include "u8g2_zephyr_port.h"

#include "gui_app.h"

#define GUI_STACK_SIZE 2048
K_THREAD_STACK_DEFINE(gui_stack, GUI_STACK_SIZE);
struct k_thread gui_thread;


int main(void)
{

    printk("Air Purifier Application Start\n");
    k_tid_t gui_tid = k_thread_create(&gui_thread, gui_stack,
                                      K_THREAD_STACK_SIZEOF(gui_stack),
                                      gui_thread_func,
                                      NULL, NULL, NULL,
                                      5, 0, K_NO_WAIT);
    printk("GUI thread created with TID: %p\n", gui_tid);

    while (1)
    {
        k_msleep(1000);
    }

    return 0;
}