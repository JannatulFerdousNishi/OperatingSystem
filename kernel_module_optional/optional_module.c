// optional_module.c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hash.h>   // GOLDEN_RATIO_PRIME
#include <linux/gcd.h>    // gcd()

static int __init optional_init(void)
{
    printk(KERN_INFO "Loading Kernel Module (Optional)\n");
    printk(KERN_INFO "GOLDEN_RATIO_PRIME = %lu\n", (unsigned long)GOLDEN_RATIO_PRIME);
    return 0; // success
}

static void __exit optional_exit(void)
{
    unsigned long result = gcd(3700, 24);
    printk(KERN_INFO "Removing Kernel Module (Optional)\n");
    printk(KERN_INFO "gcd(3700, 24) = %lu\n", result);
}

module_init(optional_init);
module_exit(optional_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Optional Kernel Module: prints GOLDEN_RATIO_PRIME on load and gcd(3700,24) on unload");
MODULE_AUTHOR("Student");
