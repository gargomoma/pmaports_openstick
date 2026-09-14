/* selinux_shim.c: this kernel has no SELinux, and Android's native services abort when
 * getcon() fails (hwservicemanager: LOG_ALWAYS_FATAL). LD_PRELOADed so these definitions win
 * over libselinux; only the process-context and access-check calls are replaced, and every
 * check returns allowed, since there is no policy to enforce. Built with -nostdlib: no
 * DT_NEEDED, libc symbols bind to bionic at load time. */

/* Resolved from bionic at load time; not linked here. */
extern void *malloc(unsigned long size);
extern void free(void *p);

#define SHIM_CONTEXT "u:r:init:s0"

/* Return a heap copy so callers can freecon() it exactly as they would a real
 * context. Hand-rolled copy: memcpy would have to be resolved too, and
 * -fno-builtin keeps the compiler from turning this loop back into one. */
static char *dup_context(void)
{
	static const char src[] = SHIM_CONTEXT;
	char *dst = (char *)malloc(sizeof(src));
	unsigned long i;

	if (!dst)
		return 0;
	for (i = 0; i < sizeof(src); i++)
		dst[i] = src[i];
	return dst;
}

/* ---- process contexts: the calls that fail without a kernel LSM ---- */

int getcon(char **con)
{
	*con = dup_context();
	return *con ? 0 : -1;
}

int getcon_raw(char **con)
{
	return getcon(con);
}

int getpidcon(int pid, char **con)
{
	(void)pid;
	return getcon(con);
}

int getpidcon_raw(int pid, char **con)
{
	(void)pid;
	return getcon(con);
}

int getpeercon(int fd, char **con)
{
	(void)fd;
	return getcon(con);
}

int getpeercon_raw(int fd, char **con)
{
	(void)fd;
	return getcon(con);
}

void freecon(char *con)
{
	if (con)
		free(con);
}

int setcon(const char *con)
{
	(void)con;
	return 0;
}

int setcon_raw(const char *con)
{
	(void)con;
	return 0;
}

/* ---- enforcement state: nothing to enforce ---- */

int is_selinux_enabled(void)
{
	return 0;
}

int is_selinux_mls_enabled(void)
{
	return 0;
}

int security_getenforce(void)
{
	return 0;
}

int security_setenforce(int value)
{
	(void)value;
	return 0;
}

/* ---- access checks: allow, since no policy exists to consult ---- */

int selinux_check_access(const char *scon, const char *tcon, const char *class,
			 const char *perm, void *auditdata)
{
	(void)scon; (void)tcon; (void)class; (void)perm; (void)auditdata;
	return 0;
}

int security_check_context(const char *con)
{
	(void)con;
	return 0;
}

int security_check_context_raw(const char *con)
{
	(void)con;
	return 0;
}

/* ---- status page: backed by a kernel file that is absent here ---- */

int selinux_status_open(int fallback)
{
	(void)fallback;
	return 0;
}

int selinux_status_updated(void)
{
	return 0;
}

void selinux_status_close(void)
{
}
