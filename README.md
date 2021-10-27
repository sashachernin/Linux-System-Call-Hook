# Linux-System-Call-Hook
This module will intercept ‘openat’ syscall in such a way that when ‘openat’ is called by the gedit process (or any other editor that will be specified), it reports the name of the file that the particular process opens to a kobject, which can be viewed though a specific user-space application in real time
