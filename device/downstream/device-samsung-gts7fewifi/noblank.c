/*
 * LD_PRELOAD shim: swallow FBIOBLANK. The vendor SDE stack powers the panel down on it and
 * never brings it back; Xorg's fbdevhw issues it during ordinary screen setup.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ioctl.h>

#ifndef FBIOBLANK
#define FBIOBLANK 0x4611
#endif

int ioctl(int fd, int request, ...)
{
	static int (*real_ioctl)(int, int, ...) = NULL;
	va_list ap;
	void *arg;

	if (!real_ioctl)
		real_ioctl = dlsym(RTLD_NEXT, "ioctl");

	va_start(ap, request);
	arg = va_arg(ap, void *);
	va_end(ap);

	if (request == FBIOBLANK) {
		fprintf(stderr, "[noblank] swallowed FBIOBLANK on fd %d\n", fd);
		return 0;   /* pretend success, change nothing */
	}
	return real_ioctl(fd, request, arg);
}
