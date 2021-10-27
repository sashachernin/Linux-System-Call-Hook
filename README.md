# Linux-System-Call-Hook
COMPILING:
	run 'make' to build both kernel module and user-space client
	run 'make module' to build kernel module only
	run 'make client' to build user-space client only
RUNNING:
	1. Insert built kernel module into your kernel:
		'sudo insmod ./alternate_openat.ko'
			or
		'sudo insmod ./alternate_openat.ko editor="<your editor>"
	NOTE: If 'editor' argument isnt supplied it defaults to 'gedit'
	2. Run client
		'./client'
	3. Run your editor
		'gedit'
			or
		'<editor you supplied to module>'
	4. Observe the list of file were opened by editor
FUNNY FACT:
	gedit never uses openat call to open files for editing, but leafpad does
