#include <string.h>
#include <stdlib.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

int main(){
	int f;
	char buf[16];
	char string[4096];
	struct pollfd pfd;
	puts("Following files were opened by 'gedit' using openat() syscall");
	for(;;){
		memset(buf, 0, sizeof(buf));
		if ((f = open("/sys/kernel/opened_with_gedit/file", O_RDONLY)) < 0 ) {
			puts("E: Couldnt open sysfs kobject file, module isnt loaded");
		}
		if ((lseek(f, 0L, SEEK_SET)) < 0){
			puts("Failed to set pointer");
			exit(2);
		}
		if ((read(f, buf, 1) <0)){
			puts("Failed to read from file");
			exit(4);
		}
		pfd.fd = f;
		pfd.events = POLLPRI;

		poll(&pfd, 1, -1);
		close(f);
		
		FILE* sysfs_obj = fopen("/sys/kernel/opened_with_gedit/file", "r");
		fgets(string, 4096, sysfs_obj);
		printf(string);
		fclose(sysfs_obj);
	}
}
