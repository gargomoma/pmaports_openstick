/* Minimal faccessat compatibility for postmarketOS on Huawei Linux 4.4. */

#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_EACCESS 0x200
#define F_OK 0
#define EINVAL 22
#define ENOSYS 38
#define SYS_FACCESSAT 48
#define SYS_NEWFSTATAT 79

extern int *__errno_location(void);

static long syscall3(long number, long arg0, long arg1, long arg2)
{
    register long x0 __asm__("x0") = arg0;
    register long x1 __asm__("x1") = arg1;
    register long x2 __asm__("x2") = arg2;
    register long x8 __asm__("x8") = number;

    __asm__ volatile("svc 0"
                     : "+r"(x0)
                     : "r"(x1), "r"(x2), "r"(x8)
                     : "memory", "cc");
    return x0;
}

static long syscall4(long number, long arg0, long arg1, long arg2, long arg3)
{
    register long x0 __asm__("x0") = arg0;
    register long x1 __asm__("x1") = arg1;
    register long x2 __asm__("x2") = arg2;
    register long x3 __asm__("x3") = arg3;
    register long x8 __asm__("x8") = number;

    __asm__ volatile("svc 0"
                     : "+r"(x0)
                     : "r"(x1), "r"(x2), "r"(x3), "r"(x8)
                     : "memory", "cc");
    return x0;
}

static int syscall_result(long result)
{
    if (result < 0 && result >= -4095) {
        *__errno_location() = (int)-result;
        return -1;
    }
    return (int)result;
}

int faccessat(int dirfd, const char *path, int mode, int flags)
{
    long result;

    if ((flags & ~(AT_SYMLINK_NOFOLLOW | AT_EACCESS)) != 0) {
        *__errno_location() = EINVAL;
        return -1;
    }

    /* GLib uses this existence check for launchers and icons. */
    if ((flags & AT_SYMLINK_NOFOLLOW) != 0) {
        unsigned long stat_buffer[32] __attribute__((aligned(16)));

        if (mode != F_OK) {
            *__errno_location() = EINVAL;
            return -1;
        }
        result = syscall4(SYS_NEWFSTATAT, dirfd, (long)path,
                          (long)stat_buffer, AT_SYMLINK_NOFOLLOW);
        return syscall_result(result);
    }

    /* The affected session has matching real and effective IDs. */
    result = syscall3(SYS_FACCESSAT, dirfd, (long)path, mode);
    if (result == -ENOSYS) {
        *__errno_location() = ENOSYS;
        return -1;
    }
    return syscall_result(result);
}
