/****************************************************************************
 *
 * Copyright (C) 2013 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 * ****************************************************************************/


#include <linux/init.h> 
#include <linux/module.h> 
#include <linux/kernel.h> 
#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/earlysuspend.h>
#include <linux/fs.h>            
#include <linux/mm.h>            
#include <linux/errno.h>         
#include <linux/types.h>         
#include <linux/fcntl.h>         
#include <linux/cdev.h>         
#include <linux/device.h>         
#include <linux/major.h>         
#include <linux/gpio.h>

#include <asm/uaccess.h>  
#include <asm/io.h>  
#include <asm/mach-types.h>

#include <mach/bsp.h>  
#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>

#if defined(CONFIG_DAUDIO)
/**
 * @author valky@cleinsoft
 * @date 2013/10/25
 **/
#define GPIO_WF_EN          TCC_GPF(9)
//#define GPIO_WF_EN          TCC_GPA(0)
#elif defined(CONFIG_BROADCOM_WIFI) && defined(CONFIG_MACH_TCC893X) && !defined(CONFIG_SUPPORT_BCM4335)
#define GPIO_WF_EN          TCC_GPEXT1(3)
#elif defined(CONFIG_BROADCOM_WIFI) && defined(CONFIG_MACH_TCC8920)
#define GPIO_WF_EN          TCC_GPEXT1(3)
#elif defined(CONFIG_BROADCOM_WIFI) && defined(CONFIG_SUPPORT_BCM4335)
#define GPIO_WF_EN            TCC_GPB(31)
#else
#error unknown defined
#endif

#define WIFI_GPIO_DEV_NAME     "wifi_pwr"  
#define WIFI_PWR_DEV_MAJOR     0      
#define DEBUG_ON               1           

//#define PMU_WIFI_SWITCH			                    
//#define wifi_dbg(fmt,arg...)     if(DEBUG_ON) printk("== wifi debug == " fmt, ##arg)

static int      wifi_major = WIFI_PWR_DEV_MAJOR;
static dev_t    dev;
static struct   cdev  wifi_cdev;
static struct   class *wifi_class;
static struct   regulator *vdd_wifi = NULL;
extern int axp192_ldo_set_wifipwd(int mode);
int wifi_stat = 0;
EXPORT_SYMBOL(wifi_stat);


static int wifi_pwr_open (struct inode *inode, struct file *filp)  
{
    return 0;  
}

static int wifi_pwr_release (struct inode *inode, struct file *filp)  
{  
    //if(system_rev == 0x2002 || system_rev == 0x2003 || system_rev == 0x2004 || system_rev == 0x2005 || system_rev == 0x2006 || system_rev == 0x2007)
    //    vdd_wifi = NULL;
    //printk("%s\n", __func__); 
    return 0;  
}  

int wifi_pwr_ioctl_hw( unsigned int cmd)
{
    printk("%s %s\n", __func__, cmd == 1 ? "WIFI_ON" : "WIFI_OFF");
    switch( cmd )  
    {  
        case 1 : // WIFI_On
            if (machine_is_m805_892x() || machine_is_m805_893x()) {
                if(system_rev == 0x2000 || system_rev == 0x2001) {
                    gpio_direction_output(GPIO_WF_EN, 1);
                }else {	// 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007, 0x2008
#ifdef CONFIG_REGULATOR
                    if (vdd_wifi) {			
                        regulator_enable(vdd_wifi);
                        printk("wifi_prw: power on\n");
                    }else
                        printk("vdd_wifi is null???\n");
#endif			
                    }
                }else {
                        gpio_request(GPIO_WF_EN,"wifi_pwr");
                        gpio_direction_output(GPIO_WF_EN, 1);
                }
                break;   
        case 0 : // WIFI_Off
            if (machine_is_m805_892x() || machine_is_m805_893x()) {
                if(system_rev == 0x2000 || system_rev == 0x2001) {
                    gpio_direction_output(GPIO_WF_EN, 0);	
                }else {	// 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007, 0x2008
#ifdef CONFIG_REGULATOR	
                if (vdd_wifi){
                    regulator_disable(vdd_wifi);
                    printk("wifi_prw: power off\n");
                }else
                    printk("vdd_wifi is null???\n");
#endif
                }
            }else {
                gpio_request(GPIO_WF_EN,"wifi_pwr");
                gpio_direction_output(GPIO_WF_EN, 0);
            }
            break;
        default :
            break;
        };
        return 0;
}

EXPORT_SYMBOL(wifi_pwr_ioctl_hw);

static long wifi_pwr_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    printk("%s, %d\n", __func__, cmd);
    switch( cmd )  
    {  
        case 1 : // WIFI_On
                wifi_stat = 1;
                wifi_pwr_ioctl_hw(cmd);
                break;   
			
        case 0 : // WIFI_Off
                wifi_stat = 0;
                wifi_pwr_ioctl_hw(cmd);
            break;

        default :
            break;
    };

    return 0;  
}  

static struct file_operations wifi_pwr_fops =  
{  
    .owner    = THIS_MODULE,  
    .unlocked_ioctl    = wifi_pwr_ioctl,  
    .open     = wifi_pwr_open,       
    .release  = wifi_pwr_release,    
};  

static int __init wifi_pwr_init(void)  
{  
    int result;  
    printk("%s\n", __func__); 

    if (0 == wifi_major)
    {
            /* auto select a major */
            result = alloc_chrdev_region(&dev, 0, 1, WIFI_GPIO_DEV_NAME);
            wifi_major = MAJOR(dev);
    } else {
            /* use load time defined major number */
            dev = MKDEV(wifi_major, 0);
            result = register_chrdev_region(dev, 1, WIFI_GPIO_DEV_NAME);
    }

    memset(&wifi_cdev, 0, sizeof(wifi_cdev));

    /* initialize our char dev data */
    cdev_init(&wifi_cdev, &wifi_pwr_fops);

    /* register char dev with the kernel */
    result = cdev_add(&wifi_cdev, dev, 1);

    if (0 != result) {
            unregister_chrdev_region(dev, 1);
            printk("Error registrating mali device object with the kernel__wifi\n");
    }

    wifi_class = class_create(THIS_MODULE, WIFI_GPIO_DEV_NAME);
    device_create(wifi_class, NULL, MKDEV(wifi_major, MINOR(dev)), NULL, WIFI_GPIO_DEV_NAME);

    if (machine_is_m805_892x()) {
        if(system_rev == 0x2000|| system_rev == 0x2001 ) {
            tcc_gpio_config(GPIO_WF_EN, GPIO_FN(0) );
            gpio_request(GPIO_WF_EN,"wifi_pwr");
            gpio_direction_output(GPIO_WF_EN, 0);//default-openit.	
        }else {	// 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007, 0x2008 
#if defined(CONFIG_REGULATOR)
            //vdd_wifi =  regulator_get(NULL, "vdd_wifi30");
            vdd_wifi =  regulator_get(NULL, "vdd_wifi");
            if( IS_ERR(vdd_wifi)) {
                vdd_wifi = NULL;
                printk("vdd_wifi--get ERROR!!!\n");
                return -1;
            }
            regulator_enable(vdd_wifi);    // default is on
            regulator_disable(vdd_wifi);    // default is off
#endif
        }
    } else {
        tcc_gpio_config(GPIO_WF_EN, GPIO_FN(0) );
        gpio_request(GPIO_WF_EN,"wifi_pwr");
        gpio_direction_output(GPIO_WF_EN, 0);
    }

    if (result < 0)
        return result;  

    printk("wifi_pwd_ctl driver loaded\n");

    return 0;  
}  

static void __exit wifi_pwr_exit(void)  
{  
    printk("%s\n", __func__); 

    device_destroy(wifi_class, MKDEV(wifi_major, 0));
    class_destroy(wifi_class);

    cdev_del(&wifi_cdev);
    unregister_chrdev_region(dev, 1);

    if (machine_is_m805_892x() || machine_is_m805_893x()) {
        if(system_rev == 0x2000 || system_rev == 0x2001) {
            gpio_direction_output(GPIO_WF_EN, 1);
        }else {	// 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007, 0x2008
#ifdef CONFIG_REGULATOR
            if (vdd_wifi) {
                regulator_disable(vdd_wifi);
                //regulator_put(vdd_wifi);
            }
            vdd_wifi = NULL;
#endif
        }
    } else {
        gpio_direction_output(GPIO_WF_EN, 0);
    }

    printk("wifi_pwr_ctl driver unloaded");
}  

module_init(wifi_pwr_init);  
module_exit(wifi_pwr_exit);  

MODULE_LICENSE("GPL");  
