/*
 * faccessat-compat.so: faccessat2() exists only from Linux 5.8. On this 5.4 kernel musl gets
 * ENOSYS and then rejects AT_SYMLINK_NOFOLLOW with EINVAL, so every glib existence check
 * fails. Retry without the unsupported flags, only on that exact failure.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

int faccessat(int fd, const char *path, int mode, int flags)
{
	static int (*real_faccessat)(int, const char *, int, int) = NULL;
	int r, saved;

	if (!real_faccessat)
		real_faccessat = dlsym(RTLD_NEXT, "faccessat");

	r = real_faccessat(fd, path, mode, flags);
	if (r == 0 || errno != EINVAL || !(flags & ~AT_EACCESS))
		return r;

	/* Kernel has no faccessat2 and musl refused the extra flags. Retry
	 * with only the flag musl can honour without the syscall. */
	saved = errno;
	errno = 0;
	r = real_faccessat(fd, path, mode, flags & AT_EACCESS);
	if (r != 0 && errno == 0)
		errno = saved;
	return r;
}
