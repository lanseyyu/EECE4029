/*
*  battcheck.c - ACPI Battery Kernel Module
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or (at
*  your option) any later version.
*
*  This program is distributed in the hope that it will be useful, but
*  WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*  General Public License for more details.
*
*  You should have received a copy of the GNU General Public License along
*  with this program; if not, write to the Free Software Foundation, Inc.,
*  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
*
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <acpi/acpi.h>
#include <linux/wait.h>                                                                                           
#include <linux/delay.h>                                                                                           
#include <linux/kthread.h>

#define KERNEL_AUTH     "Samir Silbak"
#define KERNEL_DESC     "kernel module to monitor battery <silbak04@gmail.com>"

MODULE_LICENSE("GPL");
MODULE_AUTHOR(KERNEL_AUTH);
MODULE_DESCRIPTION(KERNEL_DESC);

#define BUFFER_SIZE     256
#define SHRT_SLEEP_TIME 1000
#define LONG_SLEEP_TIME 30000

#define BATT_FULL       0x0
#define BATT_CHARGING   0x1
#define BATT_DISCHARGE  0x2

static char error_buffer[BUFFER_SIZE];

struct task_struct *battcheck;

// function prototypes
static void acpi_packages(const char *method);

typedef struct 
{
    int last_full_charge_cap;
    int design_cap_warn;
    int design_cap_low;
}BIF_PACKAGE_t;

typedef struct 
{
    int battery_state; 
    int batt_remn_cap; 
    int rem_batt_perc;
}BST_PACKAGE_t;

BIF_PACKAGE_t bif_package;
BST_PACKAGE_t bst_package;

int method_count = 0;
int count = 0;

static int battcheck_kthread(void *data) 
{
    while (!kthread_should_stop())
    {
        int sleep_time = SHRT_SLEEP_TIME;

        // we only need to retrieve the bif package once
        if (method_count == 1)
            acpi_packages("\\_SB_.BAT0._BST");
        else
        {
            acpi_packages("\\_SB_.BAT0._BIF");
            acpi_packages("\\_SB_.BAT0._BST");

            method_count = 1;
        }

        if (bst_package.battery_state == BATT_FULL)
            printk(KERN_INFO "[battcheck]: battery is fully charged\n");

        else if (bst_package.battery_state == BATT_DISCHARGE)
            printk(KERN_INFO "[battcheck]: battery is discharging\n");

        else if (bst_package.battery_state == BATT_CHARGING)
            printk(KERN_INFO "[battcheck]: battery is charging\n");
        
        if (bst_package.batt_remn_cap == bif_package.design_cap_warn)
        {
            if (count == 0)
                printk(KERN_ALERT "[battcheck]: battery is low\n");

            count = 1;
        }
            
        else if (bst_package.batt_remn_cap == bif_package.design_cap_low)
        {
            printk(KERN_ALERT "[battcheck]: battery is critically low\n");
            sleep_time = LONG_SLEEP_TIME;
        }

        printk(KERN_INFO "[battcheck]: battery percent remaining: %d.%02d%%\n", 
                          (bst_package.rem_batt_perc / 100), bst_package.rem_batt_perc % 100);
        //printk(KERN_INFO "[battcheck]: battery remaining cap: %d\n", bst_package.batt_remn_cap);
        //printk(KERN_INFO "[battcheck]: last full charge: %d\n", bif_package.last_full_charge_cap);
        //printk(KERN_INFO "[battcheck]: design cap warn: %d\n", bif_package.design_cap_warn);
        //printk(KERN_INFO "[battcheck]: design cap critical: %d\n", bif_package.design_cap_low);

        msleep_interruptible(sleep_time);
    }

    return 0;
}
        
static void acpi_packages(const char *method)
{
    // battery info and status buffer
    struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };

    union acpi_object *result; 
    union acpi_object *element; 

    acpi_status status;
    acpi_handle handle;

    status = acpi_get_handle(NULL, (acpi_string) method, &handle);

    if (ACPI_FAILURE(status))
    {
        snprintf(error_buffer, BUFFER_SIZE, "Error: %s", acpi_format_exception(status));
        printk(KERN_ERR "[battcheck]: Cannot get handle: %s\n", error_buffer);

        return;
    }

    status = acpi_evaluate_object(handle, NULL, NULL, &buffer);

    if (ACPI_FAILURE(status))
    {
        snprintf(error_buffer, BUFFER_SIZE, "Error: %s", acpi_format_exception(status));
        printk(KERN_ERR "[battcheck]: Method call failed: %s\n", error_buffer);

        return;
    }

    *error_buffer = '\0';
    result = buffer.pointer;

    if (method == "\\_SB_.BAT0._BIF") 
    {
        element = &result->package.elements[2];
        bif_package.last_full_charge_cap = (int)element->integer.value;

        element = &result->package.elements[5];
        bif_package.design_cap_warn = (int)element->integer.value;

        element = &result->package.elements[6];
        bif_package.design_cap_low = (int)element->integer.value;
    }
    if (method == "\\_SB_.BAT0._BST")
    {
        element = &result->package.elements[0];
        bst_package.battery_state = (int)element->integer.value;

        element = &result->package.elements[2];
        bst_package.batt_remn_cap = (int)element->integer.value;

        bst_package.rem_batt_perc = (bst_package.batt_remn_cap * 10000 / bif_package.last_full_charge_cap);
    }

    kfree(buffer.pointer);
}

static int __init init_acpi_battcheck(void)
{
    printk(KERN_INFO "[battcheck]: Module loaded successfully\n");
    battcheck = kthread_run(battcheck_kthread, NULL, "battcheck kernel thread");

    return 0;
}

static void __exit unld_acpi_battcheck(void)
{
    kthread_stop(battcheck);
    printk(KERN_INFO "[battcheck]: Module unloaded successfully\n");
}

module_init(init_acpi_battcheck);
module_exit(unld_acpi_battcheck);
