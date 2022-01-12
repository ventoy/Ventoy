1. install ubuntu 21.10
2. apt-get install build-essential flex libncurses-dev linux-headers-generic linux-source libssl-dev ...... and so on
3. cp /lib/modules/5.13.0-23-generic/build/Module.symvers ./
4. /boot/config-5.13.0-23-generic as .config   make oldconfig
5. make menuconfig 
    1. close CONFIG_STACKPROTECTOR
    2. close CONFIG_RETPOLINE

6. modify ./scripts/mod/modpost.c
    1. skip add_srcversion (just return)
    2. force add_retpoline (#ifdef --> #ifndef)
    3. force add_intree_flag

7. make modules_prepare LOCALVERSION=-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx

8. Append padding at the end of struct module <include/linux/module.h>
struct module {
    enum module_state state;

	/* Member of list of modules */
	struct list_head list;

	/* Unique handle for this module */
	char name[MODULE_NAME_LEN];
    
    ....
    
    char padding[1024];
};

This is because struct module size is different in different kernel versions or with different CONFIG item.


9. make modules M=/home/dmpatch
10. strip --strip-debug /home/dmpatch/dm_patch.ko

