# Linux-System-Call-Hook
Linux Kernel module that interceps the ‘openat’ syscall, reports the name of the file that a particular process opens to the user.
## COMPILING
* run 'make' to build both kernel module and user-space client
* run 'make module' to build kernel module only
* run 'make client' to build user-space client only

## RUNNING
1. **Insert built kernel module into your kernel**
    ```bash
    sudo insmod ./alternate_openat.ko
    ```
    or
    ```bash
    sudo insmod ./alternate_openat.ko editor="<your editor>
    ```
    NOTE: If 'editor' argument isnt supplied it defaults to 'gedit'

2. **Run client**
    ```bash
    ./client
    ```
3. **Run your editor**
    ```bash
    gedit
    ```
    or
    ```bash
    <editor you supplied to module>
    ```
4. **Observe the list of file were opened by editor**
