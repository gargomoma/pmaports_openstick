/* props.c: serve Android system properties without Android's init. Nothing creates
 * /dev/__properties__ here, so HALs read empty properties (libhardware then dlopens the wrong
 * module and property waits spin). Creating the real area needs init's property_info trie and
 * SELinux xattrs, so this LD_PRELOAD answers the property API from a key=value file instead.
 * Built with -nostdlib: libc symbols bind to bionic at load time. */

/* A local definition so no libc header is needed; the layout is fixed ABI. */
struct pmos_timespec {
	long tv_sec;
	long tv_nsec;
};

extern int open(const char *path, int flags, ...);
extern long read(int fd, void *buf, unsigned long count);
extern int close(int fd);
extern long write(int fd, const void *buf, unsigned long count);
extern int nanosleep(const struct pmos_timespec *req, struct pmos_timespec *rem);

#define PROPS_FILE  "/vendor/etc/pmos-props.conf"
#define O_RDONLY    0

/* bionic's limits. A name is PROP_NAME_MAX and a value PROP_VALUE_MAX, both
 * including the terminator; nothing here may hand back more than that. */
#define PROP_NAME_MAX   32
#define PROP_VALUE_MAX  92

/* Names longer than PROP_NAME_MAX are legal for the newer read_callback API
 * even though the old __system_property_get API cannot express them, so the
 * store is sized generously rather than at PROP_NAME_MAX. */
#define NAME_CAP    128
#define MAX_PROPS   1024

struct prop_entry {
	char name[NAME_CAP];
	char value[PROP_VALUE_MAX];
	unsigned int serial;		/* even means "settled", as in bionic */
	int used;
};

/* .bss, so none of this is in the file on disk. */
static struct prop_entry table[MAX_PROPS];
static char filebuf[512 * 1024];
static int n_props;
static unsigned int area_serial = 2;

static unsigned long str_len(const char *s)
{
	unsigned long n = 0;

	while (s[n])
		n++;
	return n;
}

static int str_eq(const char *a, const char *b)
{
	unsigned long i = 0;

	while (a[i] && a[i] == b[i])
		i++;
	return a[i] == b[i];
}

/* Bounded copy that always terminates. Returns the length written. */
static unsigned int str_copy(char *dst, const char *src, unsigned int cap)
{
	unsigned int i = 0;

	if (!cap)
		return 0;
	while (src[i] && i < cap - 1) {
		dst[i] = src[i];
		i++;
	}
	dst[i] = '\0';
	return i;
}

static void say(const char *s)
{
	write(2, s, str_len(s));
}

static struct prop_entry *find_entry(const char *name)
{
	int i;

	for (i = 0; i < n_props; i++)
		if (table[i].used && str_eq(table[i].name, name))
			return &table[i];
	return 0;
}

static struct prop_entry *add_entry(const char *name, const char *value)
{
	struct prop_entry *e;

	if (n_props >= MAX_PROPS)
		return 0;
	e = &table[n_props++];
	str_copy(e->name, name, NAME_CAP);
	str_copy(e->value, value, PROP_VALUE_MAX);
	e->serial = 2;
	e->used = 1;
	return e;
}

__attribute__((constructor))
static void pmos_props_init(void)
{
	long total = 0, n;
	unsigned long i = 0;
	int fd;

	fd = open(PROPS_FILE, O_RDONLY);
	if (fd < 0) {
		say("pmos-props: cannot open " PROPS_FILE ", properties will be empty\n");
		return;
	}
	while ((n = read(fd, filebuf + total,
			 sizeof(filebuf) - 1 - (unsigned long)total)) > 0)
		total += n;
	close(fd);
	if (total <= 0)
		return;
	filebuf[total] = '\0';

	while (i < (unsigned long)total && n_props < MAX_PROPS) {
		unsigned long start = i, eq = 0, end;

		while (i < (unsigned long)total && filebuf[i] != '\n') {
			if (filebuf[i] == '=' && !eq)
				eq = i;
			i++;
		}
		end = i;
		i++;					/* step past the newline */

		if (end > start && filebuf[end - 1] == '\r')
			end--;
		if (!eq || eq <= start || filebuf[start] == '#')
			continue;		/* blank, comment, or no '=' */

		filebuf[eq] = '\0';
		filebuf[end] = '\0';
		add_entry(&filebuf[start], &filebuf[eq + 1]);
	}
}

/* ---- the old, string-copying API ---- */

int __system_property_get(const char *name, char *value)
{
	struct prop_entry *e = find_entry(name);

	if (!e) {
		value[0] = '\0';
		return 0;
	}
	return (int)str_copy(value, e->value, PROP_VALUE_MAX);
}

int __system_property_set(const char *name, const char *value)
{
	struct prop_entry *e = find_entry(name);

	if (e) {
		str_copy(e->value, value, PROP_VALUE_MAX);
		e->serial += 2;
	} else if (!add_entry(name, value)) {
		return -1;
	}
	area_serial += 2;
	return 0;
}

/* ---- the handle-based API ----
 *
 * prop_info is opaque to every caller, so a pointer to our own entry is a
 * valid handle as long as the functions that consume it are ours too. They
 * all are: find, read, read_callback, serial and wait are defined here. */

const void *__system_property_find(const char *name)
{
	return (const void *)find_entry(name);
}

void __system_property_read_callback(const void *pi,
				     void (*callback)(void *cookie, const char *name,
						      const char *value, unsigned int serial),
				     void *cookie)
{
	const struct prop_entry *e = (const struct prop_entry *)pi;

	if (e && callback)
		callback(cookie, e->name, e->value, e->serial);
}

int __system_property_read(const void *pi, char *name, char *value)
{
	const struct prop_entry *e = (const struct prop_entry *)pi;

	if (!e)
		return 0;
	if (name)
		str_copy(name, e->name, PROP_NAME_MAX);
	if (!value)
		return 0;
	return (int)str_copy(value, e->value, PROP_VALUE_MAX);
}

unsigned int __system_property_serial(const void *pi)
{
	const struct prop_entry *e = (const struct prop_entry *)pi;

	return e ? e->serial : 0;
}

unsigned int __system_property_area_serial(void)
{
	return area_serial;
}

int __system_property_foreach(void (*propfn)(const void *pi, void *cookie), void *cookie)
{
	int i;

	if (!propfn)
		return -1;
	for (i = 0; i < n_props; i++)
		if (table[i].used)
			propfn((const void *)&table[i], cookie);
	return 0;
}

/* Sleep out the caller's timeout, then report nothing changed: every process has its own
 * table, so a wait can only time out, and an instant return turns libhidlbase's
 * WaitForProperty loop into a hot loop. The cap bounds callers with no timeout. */
int __system_property_wait(const void *pi, unsigned int old_serial,
			   unsigned int *new_serial_ptr, const void *relative_timeout)
{
	const struct prop_entry *e = (const struct prop_entry *)pi;
	struct pmos_timespec t;

	if (relative_timeout) {
		t = *(const struct pmos_timespec *)relative_timeout;
	} else {
		t.tv_sec = 1;
		t.tv_nsec = 0;
	}
	if (t.tv_sec > 2) {
		t.tv_sec = 2;
		t.tv_nsec = 0;
	}
	if (t.tv_sec > 0 || t.tv_nsec > 0)
		nanosleep(&t, 0);

	if (new_serial_ptr)
		*new_serial_ptr = e ? e->serial : old_serial;
	return 0;					/* false: no change seen */
}

unsigned int __system_property_wait_any(unsigned int old_serial)
{
	return old_serial;
}

/* Present so that anything probing for the modern protocol gets a clear "no".
 * Returning 0 keeps callers on the plain get/set path used above. */
unsigned int __system_property_area__(void)
{
	return 0;
}
