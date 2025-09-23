#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <stdio.h>

int main() {
	int fd = open("/dev/sda3", O_RDWR);
	int sz;
	ioctl(fd, BLKGETSIZE, &sz);
	printf("%i", sz);
	return 0;
}
