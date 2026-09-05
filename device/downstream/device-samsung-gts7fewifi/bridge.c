/* bridge.c - drive the Android camera HAL directly (no binder: the allocator service
 * rejects Qualcomm's private descriptors and hwservicemanager blocks CamX on a display
 * service) and write frames to v4l2loopback. Structures are the HAL3 ABI from camera3.h.
 * Runs as an LD_PRELOAD constructor against bionic's linker; built with -nostdlib. */

extern void *dlopen(const char *filename, int flag);
extern void *dlsym(void *handle, const char *symbol);
extern long write(int fd, const void *buf, unsigned long count);
extern int open(const char *path, int flags, ...);
extern int close(int fd);
extern void *mmap(void *addr, unsigned long length, int prot, int flags, int fd, long offset);
extern int munmap(void *addr, unsigned long length);
extern long read(int fd, void *buf, unsigned long count);
extern int ioctl(int fd, unsigned long request, ...);

extern int atoi(const char *s);
extern int setpriority(int which, int who, int prio);
extern int getpid(void);
extern void *opendir(const char *name);
extern void *readdir(void *dirp);
extern int closedir(void *dirp);
extern long readlink(const char *path, char *buf, unsigned long bufsiz);
#define PRIO_PROCESS	0

/* bionic's struct dirent, LP64. Declared rather than included because this file
 * deliberately has no headers; only d_name is used. */
struct pmos_dirent {
	unsigned long d_ino;
	long d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[256];
};

typedef void (*sighandler_t)(int);
extern sighandler_t signal(int signum, sighandler_t handler);
extern int raise(int sig);
extern unsigned int alarm(unsigned int seconds);
extern void _exit(int status);
#define SIGINT		2
#define SIGALRM		14
#define SIGTERM		15
#define SIGUSR1		10
#define SIGUSR2		12

struct timespec_ {
	long tv_sec;
	long tv_nsec;
};
extern int nanosleep(const struct timespec_ *req, struct timespec_ *rem);

#define RTLD_NOW	0x00002
#define O_RDONLY	0
#define O_WRONLY	1
#define O_CREAT		0100
#define O_TRUNC		01000
#define PROT_READ	1
#define MAP_SHARED	1

#define CORE_LIB	"libgralloccore.so"
#define GETINSTANCE	"_ZN7gralloc13BufferManager11GetInstanceEv"
#define ALLOCATEBUF	"_ZN7gralloc13BufferManager14AllocateBufferERKNS_16BufferDescriptorEPPK13native_handlejb"

/* gralloc::BufferDescriptor field offsets; see allocbuf.c for how these were
 * confirmed (the handle echoes back the requested width, height and usage). */
#define OFF_WIDTH	24
#define OFF_HEIGHT	28
#define OFF_FORMAT	32
#define OFF_LAYERS	36
#define OFF_USAGE	40
#define DESC_SIZE	64

/* Indices into the QTI private handle's integer array, read off allocbuf.c's
 * dump: magic 'gmsm', flags, aligned width, aligned height, requested width,
 * requested height, internal format, ..., usage, ..., size. */
#define INT_ALIGNED_W	2
#define INT_ALIGNED_H	3
#define INT_SIZE	13

#define FORMAT_YCBCR_420_888	0x23
#define USAGE_SW_READ_OFTEN	0x3
#define USAGE_CAMERA_OUTPUT	0x20000

/* v4l2loopback output. YUYV rather than NV12 because every consumer
 * understands it, and the conversion from the HAL's NV12 is a few lines. */
#define O_RDWR			2
#define VIDIOC_S_FMT		0xC0D05605UL
#define V4L2_BUF_TYPE_VIDEO_OUTPUT	2
#define V4L2_FIELD_NONE		1
#define V4L2_PIX_FMT_YUYV	0x56595559UL
#define V4L2_COLORSPACE_SRGB	8

#define TEMPLATE_PREVIEW	1
#define TEMPLATE_STILL_CAPTURE	2
/* HAL_PIXEL_FORMAT_BLOB. A JPEG comes back in a buffer of this format, with the
 * real byte count in a footer at the very end rather than in the buffer size:
 *
 *   struct camera3_jpeg_blob { uint16_t jpeg_blob_id; uint32_t jpeg_size; }
 *
 * with jpeg_blob_id == 0x00FF. gralloc allocates a BLOB as a one-dimensional
 * buffer, so the "width" it is asked for is a size in bytes and the height is 1;
 * the image dimensions live in the camera3_stream instead. */
#define FORMAT_BLOB		0x21
/* ANDROID_JPEG_MAX_SIZE. Tags are (section << 16) | index; JPEG is section 7 and
 * MAX_SIZE is the ninth tag in it. The value is the largest JPEG the HAL will
 * ever produce for this camera, and it is the size the snapshot pipeline expects
 * the client's BLOB buffer to be. */
#define TAG_JPEG_MAX_SIZE	0x00070008
/* ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS: SCALER is section 13 and this
 * is the eleventh tag in it. The value is a flat int32 array of quadruples,
 * (format, width, height, direction), with direction 0 for output. It is the
 * authoritative answer to "what sizes does this camera take", which otherwise
 * has to be discovered by offering a size and seeing whether configure_streams
 * refuses the entire configuration. */
#define TAG_STREAM_CONFIGS	0x000d000a
/* ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS, the tag after it: quadruples of
 * (format, width, height, nanoseconds), as int64. The duration is the shortest
 * time between frames the camera can sustain at that size, so it is the frame
 * rate ceiling, and it is the difference between a preview size that is merely
 * accepted and one that is usable. */
#define TAG_MIN_FRAME_DURATIONS	0x000d000b
/* ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES: CONTROL is section 1 and this
 * is the twenty-first tag in it. Pairs of (min, max) frames per second that
 * auto-exposure is allowed to choose between. A camera that is allowed to drop
 * to 15 will do so the moment the light is poor, whatever the sensor's own
 * ceiling says, which is the usual reason a preview runs at half the rate the
 * size list promises. */
#define TAG_AE_FPS_RANGES	0x00010014
#define STREAM_CONFIG_OUTPUT	0
#define META_TYPE_INT32		1
#define META_TYPE_INT64		3
#define JPEG_BLOB_ID		0x00ff
/* HAL_DATASPACE_V0_JFIF. A BLOB stream must declare it: with data_space left at
 * UNKNOWN the HAL cannot tell a JPEG apart from any other opaque blob, and
 * configure_streams fails the WHOLE configuration with -22, taking the preview
 * stream down with it. */
#define DATASPACE_JFIF		0x8c20000
#define STREAM_OUTPUT		0
#define NUM_BUFFERS		4
/* Give up if the pipeline has produced nothing after this long. */
#define CAPTURE_TIMEOUT_MS	12000
/* Auto-exposure and auto-white-balance converge over the first second or so,
 * so a frame grabbed immediately is dark and off-colour. */
#define FRAMES_WANTED		45

/* Set from a signal handler so the streaming loop leaves through its normal exit path,
 * which flushes the HAL and closes the camera: a session killed mid-stream makes the next
 * start oops the kernel (cdm_write_genirq). A second signal restores the default action,
 * since a wedged HAL never reaches the loop; supervisors send SIGTERM twice, then SIGKILL. */
static volatile int stop_requested;

static void on_signal(int sig)
{
	if (stop_requested) {
		signal(sig, (sighandler_t)0);		/* SIG_DFL */
		raise(sig);
		return;
	}
	stop_requested = 1;
}

/* Settings come from /proc/self/environ, not getenv(): this runs as a preload constructor
 * before libc.so has published environ, so getenv() silently returns NULL here. */
static char envbuf[8192];
static int envlen;

static const char *env_get(const char *name)
{
	int i, j;

	if (!envlen) {
		long n;
		int fd = open("/proc/self/environ", O_RDONLY);

		if (fd < 0)
			return 0;
		n = read(fd, envbuf, sizeof(envbuf) - 1);
		close(fd);
		if (n <= 0)
			return 0;
		envbuf[n] = 0;
		envlen = (int)n;
	}

	for (i = 0; i < envlen; ) {
		for (j = 0; name[j] && i + j < envlen && envbuf[i + j] == name[j]; j++)
			;
		if (!name[j] && i + j < envlen && envbuf[i + j] == '=')
			return &envbuf[i + j + 1];
		while (i < envlen && envbuf[i])
			i++;
		i++;					/* past the NUL */
	}
	return 0;
}

struct native_handle {
	int version;
	int num_fds;
	int num_ints;
	int data[];
};

struct hw_module_methods;

struct hw_module {
	unsigned int tag;
	unsigned short module_api_version;
	unsigned short hal_api_version;
	const char *id;
	const char *name;
	const char *author;
	struct hw_module_methods *methods;
	void *dso;
	unsigned long reserved[32 - 7];
};

struct hw_device {
	unsigned int tag;
	unsigned int version;
	struct hw_module *module;
	unsigned long reserved[12];
	int (*close)(struct hw_device *device);
};

struct hw_module_methods {
	int (*open)(const struct hw_module *module, const char *id,
		    struct hw_device **device);
};

/* camera_info as libhardware defines it for module API 2.4, and the header of a
 * camera_metadata buffer. Transcribed rather than included, like the rest of the
 * ABI in this file. Entries are a flat array at entries_start; a value of four
 * bytes or fewer sits inline in the entry, anything larger lives in the data
 * area at data_start. */
struct camera_info_t {
	int facing;
	int orientation;
	unsigned int device_version;
	unsigned int pad;
	const void *static_characteristics;
	int resource_cost;
	unsigned int pad2;
	void *conflicting_devices;
	unsigned long conflicting_devices_length;
};

struct cam_meta {
	unsigned int size;
	unsigned int version;
	unsigned int flags;
	unsigned int entry_count;
	unsigned int entry_capacity;
	unsigned int entries_start;
	unsigned int data_count;
	unsigned int data_capacity;
	unsigned int data_start;
	unsigned int padding;
	unsigned long vendor_id;
};

struct cam_meta_entry {
	unsigned int tag;
	unsigned int count;
	union {
		unsigned int offset;
		unsigned char value[4];
	} data;
	unsigned char type;
	unsigned char reserved[3];
};

struct camera_module {
	struct hw_module common;
	int (*get_number_of_cameras)(void);
	int (*get_camera_info)(int camera_id, void *info);
	int (*set_callbacks)(const void *callbacks);
	void (*get_vendor_tag_ops)(void *ops);
	int (*open_legacy)(const struct hw_module *module, const char *id,
			   unsigned int halVersion, struct hw_device **device);
	int (*set_torch_mode)(const char *camera_id, int enabled);
	int (*init)(void);
	void *reserved[5];
};

/* The trailing padding is deliberate: the HAL writes past camera3.h's reserved[6], which
 * clobbered a stack local next to it. Instances live in static storage because the HAL keeps
 * the stream pointers and writes to them later. */
struct camera3_stream {
	int stream_type;
	unsigned int width;
	unsigned int height;
	int format;
	unsigned int usage;
	unsigned int max_buffers;
	void *priv;
	int data_space;
	int rotation;
	const char *physical_camera_id;
	void *reserved[6];
	void *slack[32];
};

struct camera3_stream_configuration {
	unsigned int num_streams;
	struct camera3_stream **streams;
	unsigned int operation_mode;
	const void *session_parameters;
};

struct camera3_stream_buffer {
	struct camera3_stream *stream;
	const struct native_handle **buffer;
	int status;
	int acquire_fence;
	int release_fence;
};

struct camera3_capture_request {
	unsigned int frame_number;
	const void *settings;
	struct camera3_stream_buffer *input_buffer;
	unsigned int num_output_buffers;
	const struct camera3_stream_buffer *output_buffers;
	unsigned int num_physcam_settings;
	const char **physcam_id;
	const void **physcam_settings;
};

struct camera3_capture_result {
	unsigned int frame_number;
	const void *result;
	unsigned int num_output_buffers;
	const struct camera3_stream_buffer *output_buffers;
	const struct camera3_stream_buffer *input_buffer;
	unsigned int partial_result;
	unsigned int num_physcam_metadata;
	const char **physcam_ids;
	const void **physcam_metadata;
};

struct camera3_callback_ops {
	void (*process_capture_result)(const struct camera3_callback_ops *,
				       const struct camera3_capture_result *);
	void (*notify)(const struct camera3_callback_ops *, const void *msg);
	int (*request_stream_buffers)(const struct camera3_callback_ops *,
				      unsigned int num, const void *bufs, void *out);
	void (*return_stream_buffers)(const struct camera3_callback_ops *,
				      unsigned int num, const void *bufs);
};

struct camera3_device_ops {
	int (*initialize)(const void *dev, const struct camera3_callback_ops *cb);
	int (*configure_streams)(const void *dev, struct camera3_stream_configuration *cfg);
	int (*register_stream_buffers)(const void *dev, const void *buffer_set);
	const void *(*construct_default_request_settings)(const void *dev, int type);
	int (*process_capture_request)(const void *dev, struct camera3_capture_request *req);
	void (*get_metadata_vendor_tag_ops)(const void *dev, void *ops);
	void (*dump)(const void *dev, int fd);
	int (*flush)(const void *dev);
	void *reserved[8];
};

struct camera3_device {
	struct hw_device common;
	struct camera3_device_ops *ops;
	void *priv;
};

/* ---- small output helpers; no libc string functions are linked ---- */

static unsigned long str_len(const char *s)
{
	unsigned long n = 0;

	while (s && s[n])
		n++;
	return n;
}

static void say(const char *s)
{
	if (s)
		write(2, s, str_len(s));
}

static void say_num(long v)
{
	char tmp[24];
	int i = 0;

	if (v < 0) {
		say("-");
		v = -v;
	}
	if (!v) {
		say("0");
		return;
	}
	while (v && i < 23) {
		tmp[i++] = (char)('0' + (v % 10));
		v /= 10;
	}
	while (i--)
		write(2, &tmp[i], 1);
}

static void sleep_ms(long ms)
{
	struct timespec_ t;

	t.tv_sec = ms / 1000;
	t.tv_nsec = (ms % 1000) * 1000000L;
	nanosleep(&t, 0);
}

static void str_copy(char *dst, const char *src, int max)
{
	int i = 0;

	while (src[i] && i < max - 1) {
		dst[i] = src[i];
		i++;
	}
	dst[i] = 0;
}

static int str_eq(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return *a == *b;
}

static int append_num_str(char *dst, int at, const char *s)
{
	int i = 0;

	while (s[i]) {
		dst[at + i] = s[i];
		i++;
	}
	return at + i;
}

/* ---- v4l2loopback output ---- */

struct v4l2_pix_format {
	unsigned int width;
	unsigned int height;
	unsigned int pixelformat;
	unsigned int field;
	unsigned int bytesperline;
	unsigned int sizeimage;
	unsigned int colorspace;
	unsigned int priv;
	unsigned int flags;
	unsigned int enc;
	unsigned int quantization;
	unsigned int xfer_func;
};

struct v4l2_format {
	unsigned int type;
	unsigned int pad;
	struct v4l2_pix_format pix;
	unsigned char reserved[200 - sizeof(struct v4l2_pix_format)];
};

/* One of these per sensor: the bridge drives both cameras from one process, since CamX keeps
 * process-global state and a second process opening the other sensor dies. Two loopback
 * nodes give applications a camera to switch to. cbops must stay the first member: the HAL
 * hands that pointer back to every callback. */
struct cam {
	struct camera3_callback_ops cbops;

	const char *idstr;
	const char *outpath;
	int width, height;

	struct camera3_device *dev;
	struct camera3_stream stream;
	struct camera3_stream *streams[1];
	struct camera3_stream_configuration cfg;
	struct camera3_stream_buffer outbuf;
	struct camera3_stream_buffer reqbufs[2];
	struct camera3_capture_request req;
	const void *settings;

	const struct native_handle *bufs[NUM_BUFFERS];
	unsigned char *bufbase[NUM_BUFFERS];
	unsigned int buf_stride, buf_aligned_h;

	int out_fd;

	/* Full-resolution stills. The preview stream is what applications see
	 * through the loopback, and photographing that gives a 720p picture from
	 * a 12 megapixel sensor. A second stream of format BLOB, configured at
	 * the sensor's own size, is how camera3 produces a real photograph; it
	 * costs nothing while no request asks for it. */
	int still_w, still_h;
	struct camera3_stream still_stream;
	struct camera3_stream *stream_list[2];
	const struct native_handle *still_buf;
	unsigned char *still_base;
	unsigned int still_size;
	const void *still_settings;
	volatile int want_still;
	int still_seq;

	int got_frame;				/* this camera has produced one */
	long rate_start_cs;			/* clock when this session began */
	int rate_done;				/* reported once per session */
	int idle;				/* camera closed, node still held */
	int reopening;				/* keep the node's fd and format */
	int no_reader_ms;
	unsigned char *yuyv;
	unsigned int frame_number;
	int last_done;
	int running;

	volatile int inflight[NUM_BUFFERS];
	volatile int frames_done;
	volatile int frames_errored;
	volatile int frames_written;
	volatile int shutters;
	volatile int first_error_code;
	volatile int first_error_frame;
	const struct native_handle *done_handle;
};

#define MAX_CAMS	2

static struct cam cams[MAX_CAMS];
static int ncams;
/* One conversion buffer per camera, sized for the largest stream the bridge
 * will configure. Static rather than allocated: there is no malloc in this
 * translation unit and the sizes are known. */
static unsigned char yuyv_store[MAX_CAMS][1920 * 1080 * 2];

/* A session that never produces a frame must not sit there forever: CamX can block in its
 * own initialization in an untimed futex. An alarm still fires inside a library call; the
 * handler exits so the supervisor starts a fresh process. Armed before open and around a
 * wake, cancelled by the first frame. */
static volatile int watchdog_armed;

static void arm_watchdog(unsigned int seconds)
{
	watchdog_armed = 1;
	alarm(seconds);
}

static void cancel_watchdog(void)
{
	if (watchdog_armed) {
		watchdog_armed = 0;
		alarm(0);
	}
}

static void on_watchdog(int sig)
{
	(void)sig;
	say("bridge: no frames arrived; exiting so the supervisor can retry\n");
	_exit(3);
}

/* SIGUSR1 photographs the first camera, SIGUSR2 the second. A signal rather
 * than a socket or a file because the bridge has no event loop to spare and
 * this has to be safe to call from a shell script; the handler only sets a
 * flag, and the streaming loop attaches the still buffer to its next request. */
static void on_still_signal(int sig)
{
	int n = (sig == SIGUSR2) ? 1 : 0;

	if (n < ncams)
		cams[n].want_still = 1;
}

/* Tell the loopback what the producer is about to write. Re-issued when a paused camera
 * wakes: after a gap in writes readers get the same frame forever until the format is set again. */
static int set_loopback_format(int fd, int w, int h)
{
	struct v4l2_format fmt;
	unsigned int i;

	for (i = 0; i < sizeof(fmt); i++)
		((char *)&fmt)[i] = 0;
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	fmt.pix.width = (unsigned int)w;
	fmt.pix.height = (unsigned int)h;
	fmt.pix.pixelformat = (unsigned int)V4L2_PIX_FMT_YUYV;
	fmt.pix.field = V4L2_FIELD_NONE;
	fmt.pix.bytesperline = (unsigned int)w * 2;
	fmt.pix.sizeimage = (unsigned int)(w * h * 2);
	fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
	return ioctl(fd, VIDIOC_S_FMT, &fmt);
}

/* Returns the fd, or -1. Each camera keeps its own, because there is one
 * loopback node per sensor. */
static int open_loopback(const char *path, int w, int h)
{
	struct v4l2_format fmt;
	unsigned int i;
	int fd;

	fd = open(path, O_RDWR);
	if (fd < 0)
		return -1;
	for (i = 0; i < sizeof(fmt); i++)
		((char *)&fmt)[i] = 0;
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	fmt.pix.width = (unsigned int)w;
	fmt.pix.height = (unsigned int)h;
	fmt.pix.pixelformat = (unsigned int)V4L2_PIX_FMT_YUYV;
	fmt.pix.field = V4L2_FIELD_NONE;
	fmt.pix.bytesperline = (unsigned int)w * 2;
	fmt.pix.sizeimage = (unsigned int)(w * h * 2);
	fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
	if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

/* Semi-planar YUV 4:2:0 to packed YUYV (Y0 Cb Y1 Cr). The HAL hands back NV21 (Cr first);
 * reading it as NV12 swaps red and blue. PMOS_CAM_NV12=1 reads the other order. */
static int chroma_cb_first;			/* 0 = NV21 (Cr first) */

/* Spread the four bytes of a u32 into the even byte positions of a u64, which
 * is the shape both halves of a YUYV group need. Little-endian: byte 0 is the
 * low one, so 0xD3D2D1D0 becomes 0x00D300D200D100D0. */
static unsigned long spread_bytes(unsigned int v)
{
	unsigned long t = v;

	t = (t | (t << 16)) & 0x0000ffff0000ffffUL;
	t = (t | (t << 8)) & 0x00ff00ff00ff00ffUL;
	return t;
}

/* Semi-planar YUV 4:2:0 to packed YUYV, four pixels at a time with word operations; the
 * byte-by-byte version cost 1.5 cores for two cameras. The swap is the NV21 chroma order. */
static void nv12_to_yuyv(unsigned char *dst, const unsigned char *base,
			 unsigned int stride, unsigned int aligned_h, int w, int h)
{
	const unsigned char *uv = base + (unsigned long)stride * aligned_h;
	int cb = chroma_cb_first ? 0 : 1;	/* offset of Cb within the pair */
	int cr = chroma_cb_first ? 1 : 0;
	int x, y;

	for (y = 0; y < h; y++) {
		const unsigned char *yrow = base + (unsigned long)y * stride;
		const unsigned char *uvrow = uv + (unsigned long)(y / 2) * stride;
		unsigned char *o = dst + (unsigned long)y * (unsigned long)w * 2;

		for (x = 0; x + 3 < w; x += 4) {
			unsigned int y4 = *(const unsigned int *)(yrow + x);
			unsigned int c4 = *(const unsigned int *)(uvrow + x);
			unsigned long out;

			if (!chroma_cb_first)		/* NV21: Cr before Cb */
				c4 = ((c4 & 0x00ff00ffu) << 8) |
				     ((c4 >> 8) & 0x00ff00ffu);
			out = spread_bytes(y4) | (spread_bytes(c4) << 8);
			*(unsigned long *)(o + x * 2) = out;
		}
		for (; x < w; x += 2) {		/* whatever the word loop left */
			o[x * 2 + 0] = yrow[x];
			o[x * 2 + 1] = uvrow[x + cb];
			o[x * 2 + 2] = yrow[x + 1];
			o[x * 2 + 3] = uvrow[x + cr];
		}
	}
}

/* Look up a single int32 in a camera_metadata buffer. Returns 0 on success. */
static int meta_find_int32(const void *meta, unsigned int tag, int *out)
{
	const struct cam_meta *h = meta;
	const struct cam_meta_entry *e;
	unsigned int i;

	if (!h || !h->entry_count)
		return -1;
	e = (const struct cam_meta_entry *)((const char *)h + h->entries_start);
	for (i = 0; i < h->entry_count; i++) {
		if (e[i].tag != tag)
			continue;
		if (e[i].type != META_TYPE_INT32 || e[i].count < 1)
			return -1;
		if (e[i].count * 4 <= 4)
			*out = *(const int *)e[i].data.value;
		else
			*out = *(const int *)((const char *)h + h->data_start +
					      e[i].data.offset);
		return 0;
	}
	return -1;
}

/* Point at an int32 array in the metadata. Returns 0 on success. */
static int meta_find_int32_array(const void *meta, unsigned int tag,
				 const int **out, unsigned int *count)
{
	const struct cam_meta *h = meta;
	const struct cam_meta_entry *e;
	unsigned int i;

	if (!h || !h->entry_count)
		return -1;
	e = (const struct cam_meta_entry *)((const char *)h + h->entries_start);
	for (i = 0; i < h->entry_count; i++) {
		if (e[i].tag != tag)
			continue;
		if (e[i].type != META_TYPE_INT32 || !e[i].count)
			return -1;
		*count = e[i].count;
		if (e[i].count * 4 <= 4)
			*out = (const int *)e[i].data.value;
		else
			*out = (const int *)((const char *)h + h->data_start +
					     e[i].data.offset);
		return 0;
	}
	return -1;
}

/* Hundredths of a second since boot, from /proc/uptime: the loop cannot count time by its
 * iterations, and this process has no libc time functions. */
static long now_cs(void)
{
	char buf[32];
	long whole = 0;
	long frac = 0;
	int fd, i;
	long n;

	fd = open("/proc/uptime", O_RDONLY);
	if (fd < 0)
		return 0;
	n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0)
		return 0;
	buf[n] = 0;
	for (i = 0; buf[i] >= '0' && buf[i] <= '9'; i++)
		whole = whole * 10 + (buf[i] - '0');
	if (buf[i] == '.') {
		i++;
		if (buf[i] >= '0' && buf[i] <= '9')
			frac = (buf[i] - '0') * 10;
		if (buf[i + 1] >= '0' && buf[i + 1] <= '9')
			frac += buf[i + 1] - '0';
	}
	return whole * 100 + frac;
}

/* Point at an int64 array in the metadata. Returns 0 on success. */
static int meta_find_int64_array(const void *meta, unsigned int tag,
				 const long **out, unsigned int *count)
{
	const struct cam_meta *h = meta;
	const struct cam_meta_entry *e;
	unsigned int i;

	if (!h || !h->entry_count)
		return -1;
	e = (const struct cam_meta_entry *)((const char *)h + h->entries_start);
	for (i = 0; i < h->entry_count; i++) {
		if (e[i].tag != tag)
			continue;
		if (e[i].type != META_TYPE_INT64 || !e[i].count)
			return -1;
		*count = e[i].count;
		/* Eight bytes never fits the four inline, so this is always in
		 * the data area. */
		*out = (const long *)((const char *)h + h->data_start +
				      e[i].data.offset);
		return 0;
	}
	return -1;
}

/* Frames per second this size can sustain, or 0 if the camera does not say. */
static int max_fps(const void *meta, int format, int w, int h)
{
	const long *dur;
	unsigned int n, i;

	if (meta_find_int64_array(meta, TAG_MIN_FRAME_DURATIONS, &dur, &n))
		return 0;
	for (i = 0; i + 3 < n; i += 4) {
		if ((int)dur[i] != format || (int)dur[i + 1] != w ||
		    (int)dur[i + 2] != h)
			continue;
		if (dur[i + 3] <= 0)
			return 0;
		/* Round to nearest rather than down, so 33,333,333 ns reads as
		 * 30 and not 29. */
		return (int)((1000000000L + dur[i + 3] / 2) / dur[i + 3]);
	}
	return 0;
}

/* Is this size offered as an output for this format? If not, report the largest offered size
 * no bigger than the request in either dimension (or the smallest offered size), bounded by
 * `cap` pixels (0 for none). Returns 1 if offered, 0 with a replacement in best_w/best_h, or
 * -1 if the camera publishes no list. */
static int size_supported(const void *meta, int format, int w, int h,
			  unsigned long cap, int *best_w, int *best_h)
{
	const int *cfg;
	unsigned int n, i;
	int found = 0;
	int small_w = 0, small_h = 0;

	*best_w = 0;
	*best_h = 0;
	if (meta_find_int32_array(meta, TAG_STREAM_CONFIGS, &cfg, &n))
		return -1;			/* no list: cannot say */
	for (i = 0; i + 3 < n; i += 4) {
		int cw = cfg[i + 1];
		int ch = cfg[i + 2];
		unsigned long px = (unsigned long)cw * (unsigned long)ch;

		if (cfg[i] != format || cfg[i + 3] != STREAM_CONFIG_OUTPUT)
			continue;
		if (cap && px * 2 > cap)
			continue;
		if (cw == w && ch == h)
			found = 1;
		if (cw <= w && ch <= h &&
		    px > (unsigned long)*best_w * (unsigned long)*best_h) {
			*best_w = cw;
			*best_h = ch;
		}
		if (!small_w || px < (unsigned long)small_w * (unsigned long)small_h) {
			small_w = cw;
			small_h = ch;
		}
	}
	if (!found && !*best_w) {
		*best_w = small_w;
		*best_h = small_h;
	}
	return found;
}

/* Print the sizes this camera offers for one format, largest first is not worth
 * sorting for: the list is short and the order is the HAL's own. */
static void say_sizes(const void *meta, int format, const char *label)
{
	const int *cfg;
	unsigned int n, i;
	int shown = 0;
	int fps;

	if (meta_find_int32_array(meta, TAG_STREAM_CONFIGS, &cfg, &n))
		return;
	say("  ");
	say(label);
	say(":");
	for (i = 0; i + 3 < n; i += 4) {
		if (cfg[i] != format || cfg[i + 3] != STREAM_CONFIG_OUTPUT)
			continue;
		if (shown++ == 16) {
			say(" ...");
			break;
		}
		say(" ");
		say_num(cfg[i + 1]);
		say("x");
		say_num(cfg[i + 2]);
		fps = max_fps(meta, format, cfg[i + 1], cfg[i + 2]);
		if (fps) {
			say("@");
			say_num(fps);
		}
	}
	say("\n");
}

/* Turn a number into decimal text, returning the new end offset. */
static int append_int(char *dst, int at, int v)
{
	char tmp[16];
	int i = 0;

	if (!v)
		tmp[i++] = '0';
	while (v > 0) {
		tmp[i++] = '0' + (v % 10);
		v /= 10;
	}
	while (i > 0)
		dst[at++] = tmp[--i];
	return at;
}

/* Pull the JPEG out of a returned BLOB buffer and write it to Pictures.
 *
 * The buffer is as large as the raw frame; the real length is in a footer at
 * the very end, which is why this cannot simply write the whole thing. */
static void save_still(struct cam *c)
{
	const int *bi;
	unsigned int cap;
	unsigned int size;
	char path[128];
	int at, fd;

	if (!c->still_base || !c->still_buf)
		return;
	bi = &c->still_buf->data[c->still_buf->num_fds];
	cap = (unsigned int)bi[INT_SIZE];
	if (cap < 16)
		return;

	/* A JPEG starts with SOI. If that is not there, nothing was written and
	 * hunting for a footer is pointless. */
	if (c->still_base[0] != 0xff || c->still_base[1] != 0xd8) {
		say("still: buffer has no JPEG in it\n");
		return;
	}

	/* The length lives in a camera3_jpeg_blob {uint16 id; uint32 size}, padded to eight bytes,
	 * at the end of the buffer. The end is ambiguous (gralloc rounds up, a HAL may use the
	 * configured size), so both are tried and sanity-checked. */
	size = 0;
	{
		unsigned int cands[2];
		int n;

		cands[0] = cap;
		cands[1] = c->still_size;
		for (n = 0; n < 2; n++) {
			const unsigned char *p;
			unsigned int v;

			if (cands[n] < 16 || cands[n] > cap)
				continue;
			p = c->still_base + cands[n] - 8;
			if (*(const unsigned short *)p != JPEG_BLOB_ID)
				continue;
			v = *(const unsigned int *)(p + 4);
			if (v > 3 && v < cands[n]) {
				size = v;
				break;
			}
		}
	}
	if (!size) {
		say("still: no JPEG footer, dropping the frame\n");
		return;
	}

	/* One fixed name per camera, dotted so a half-written file is not picked up by a gallery;
	 * camera-photo renames it afterwards, since this process has no clock to ask. */
	at = 0;
	at = append_num_str(path, at, "/home/user/Pictures/.camera-");
	at = append_num_str(path, at, c->idstr);
	at = append_num_str(path, at, "-new.jpg");
	path[at] = 0;
	c->still_seq++;

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		say("still: cannot write ");
		say(path);
		say("\n");
		return;
	}
	write(fd, c->still_base, size);
	close(fd);
	say("still: wrote ");
	say(path);
	say(" (");
	say_num((long)size);
	say(" bytes)\n");
}

/* ---- capture callbacks, run on the HAL's own threads ---- */

static void on_result(const struct camera3_callback_ops *cb,
		      const struct camera3_capture_result *result)
{
	struct cam *c = (struct cam *)cb;		/* cbops is first */
	unsigned int i;
	int k;

	if (!c || !result)
		return;
	for (i = 0; i < result->num_output_buffers; i++) {
		const struct camera3_stream_buffer *b = &result->output_buffers[i];

		if (!b->buffer)
			continue;
		/* Match by handle so the buffer can be reused; the HAL returns
		 * them in whatever order its pipeline finishes. */
		for (k = 0; k < NUM_BUFFERS; k++)
			if (c->bufs[k] == *b->buffer)
				c->inflight[k] = 0;

		if (c->still_buf && *b->buffer == c->still_buf) {
			if (b->status == 0)
				save_still(c);
			else
				say("still: the HAL returned an error buffer\n");
			continue;
		}

		if (b->status == 0) {
			/* Cancel the watchdog only when EVERY camera has
			 * produced something. Cancelling on the first frame
			 * from any of them would leave a half-working stack
			 * alive: one camera streaming, the other wedged, and
			 * nothing left to notice the second one. */
			c->got_frame = 1;
			{
				int k, all = 1;

				for (k = 0; k < ncams; k++)
					if (!cams[k].got_frame)
						all = 0;
				if (all)
					cancel_watchdog();
			}
			/* Keep the newest good frame: auto-exposure needs a
			 * few dozen frames to settle, so the last one is worth
			 * far more than the first. */
			c->done_handle = *b->buffer;
			c->frames_done++;
			if (c->out_fd >= 0) {
				for (k = 0; k < NUM_BUFFERS; k++) {
					if (c->bufs[k] != *b->buffer || !c->bufbase[k])
						continue;
					nv12_to_yuyv(c->yuyv, c->bufbase[k],
						     c->buf_stride, c->buf_aligned_h,
						     c->width, c->height);
					write(c->out_fd, c->yuyv,
					      (unsigned long)c->width * c->height * 2);
					c->frames_written++;
				}
			}
		} else {
			c->frames_errored++;
		}
	}
}

/* camera3_notify_msg: an int type, then a union. type 1 is an error carrying
 * {frame_number, stream, error_code}; type 2 is the shutter, which is the HAL
 * saying the sensor exposed a frame for that request. Counting shutters
 * separates "the pipeline never ran" from "it ran but the buffer came back
 * bad". */
struct camera3_notify_msg {
	int type;
	unsigned int frame_number;
	union {
		struct {
			struct camera3_stream *error_stream;
			int error_code;
		} error;
		struct {
			unsigned long timestamp;
		} shutter;
	} u;
};

static void on_notify(const struct camera3_callback_ops *cb, const void *msg)
{
	struct cam *c = (struct cam *)cb;
	const struct camera3_notify_msg *m = msg;

	if (!c || !m)
		return;
	if (m->type == 2) {
		c->shutters++;
	} else if (m->type == 1) {
		if (c->first_error_frame < 0) {
			c->first_error_frame = (int)m->frame_number;
			c->first_error_code = m->u.error.error_code;
		}
	}
}

/* Allocate one gralloc buffer and return its handle, or 0. Split out of
 * alloc_buffers because the still stream needs exactly one, of a different
 * format and shape. */
static const struct native_handle *alloc_one(int w, int h, int format,
					     unsigned long usage)
{
	int (*allocate)(void *, const void *, const struct native_handle **,
			unsigned int, int);
	void *(*get_instance)(void);
	const struct native_handle *out = 0;
	void *lib, *mgr;
	char desc[DESC_SIZE];
	unsigned int i;

	lib = dlopen(CORE_LIB, RTLD_NOW);
	if (!lib)
		return 0;
	get_instance = dlsym(lib, GETINSTANCE);
	allocate = dlsym(lib, ALLOCATEBUF);
	if (!get_instance || !allocate)
		return 0;
	mgr = get_instance();
	if (!mgr)
		return 0;

	for (i = 0; i < DESC_SIZE; i++)
		desc[i] = 0;
	*(int *)(desc + OFF_WIDTH) = w;
	*(int *)(desc + OFF_HEIGHT) = h;
	*(int *)(desc + OFF_FORMAT) = format;
	*(unsigned int *)(desc + OFF_LAYERS) = 1;
	*(unsigned long *)(desc + OFF_USAGE) = usage;

	if (allocate(mgr, desc, &out, 0, 0))
		return 0;
	return out;
}

static int alloc_buffers(struct cam *c, int w, int h, unsigned long usage)
{
	int (*allocate)(void *, const void *, const struct native_handle **,
			unsigned int, int);
	void *(*get_instance)(void);
	void *lib, *mgr;
	char desc[DESC_SIZE];
	unsigned int i;
	int rc;

	lib = dlopen(CORE_LIB, RTLD_NOW);
	if (!lib) {
		say("bridge: cannot dlopen " CORE_LIB "\n");
		return -1;
	}
	get_instance = dlsym(lib, GETINSTANCE);
	allocate = dlsym(lib, ALLOCATEBUF);
	if (!get_instance || !allocate) {
		say("bridge: BufferManager symbols missing\n");
		return -1;
	}
	mgr = get_instance();
	if (!mgr)
		return -1;

	for (i = 0; i < DESC_SIZE; i++)
		desc[i] = 0;
	*(int *)(desc + OFF_WIDTH) = w;
	*(int *)(desc + OFF_HEIGHT) = h;
	*(int *)(desc + OFF_FORMAT) = FORMAT_YCBCR_420_888;
	*(unsigned int *)(desc + OFF_LAYERS) = 1;
	*(unsigned long *)(desc + OFF_USAGE) = usage;

	for (i = 0; i < NUM_BUFFERS; i++) {
		rc = allocate(mgr, desc, &c->bufs[i], 0, 0);
		if (rc || !c->bufs[i]) {
			say("bridge: buffer allocation failed at ");
			say_num(i);
			say("\n");
			return -1;
		}
	}
	return 0;
}

/* Copy the visible region out of the padded gralloc buffer into tightly
 * packed NV12 and write it to a file. The HAL writes at the buffer's aligned
 * stride, which for 640x480 is 1024x512, so a straight dump would be padded
 * and unreadable. */
static void save_frame(const struct native_handle *h, int w, int h_px, const char *path)
{
	const int *ints = &h->data[h->num_fds];
	unsigned int aligned_w = (unsigned int)ints[INT_ALIGNED_W];
	unsigned int aligned_h = (unsigned int)ints[INT_ALIGNED_H];
	unsigned int size = (unsigned int)ints[INT_SIZE];
	unsigned char *base;
	int fd, out;
	int row;

	fd = h->data[0];
	base = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
	if (base == (void *)-1 || !base) {
		say("bridge: mmap of the frame failed\n");
		return;
	}
	out = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (out < 0) {
		say("bridge: cannot open the output file\n");
		munmap(base, size);
		return;
	}
	for (row = 0; row < h_px; row++)
		write(out, base + (unsigned long)row * aligned_w, (unsigned long)w);
	for (row = 0; row < h_px / 2; row++)
		write(out, base + (unsigned long)aligned_w * aligned_h +
		      (unsigned long)row * aligned_w, (unsigned long)w);
	close(out);
	munmap(base, size);

	say("bridge: wrote ");
	say(path);
	say(" (");
	say_num(w * h_px * 3 / 2);
	say(" bytes NV12, from a ");
	say_num(aligned_w);
	say("x");
	say_num(aligned_h);
	say(" buffer)\n");
}

/* ---- is anything actually watching? ----
 * Two cameras at 720p cost about 1.5 cores and nothing reads them most of the time.
 * v4l2loopback exposes no consumer signal, so look for a process holding the node open:
 * a /proc walk, once every IDLE_CHECK_MS. */
static int node_has_reader(const char *path, int self)
{
	char pdir[320];
	char link[320];
	char target[320];
	struct pmos_dirent *pe, *fe;
	void *procd, *fdd;
	int found = 0;
	int at;

	procd = opendir("/proc");
	if (!procd)
		return 1;			/* cannot tell: assume watched */

	while (!found && (pe = readdir(procd)) != 0) {
		if (pe->d_name[0] < '1' || pe->d_name[0] > '9')
			continue;
		if (atoi(pe->d_name) == self)
			continue;

		str_copy(pdir, "/proc/", sizeof(pdir));
		at = append_num_str(pdir, 6, pe->d_name);
		at = append_num_str(pdir, at, "/fd");
		pdir[at] = 0;

		fdd = opendir(pdir);
		if (!fdd)
			continue;		/* kernel thread, or gone */
		while ((fe = readdir(fdd)) != 0) {
			long n;

			if (fe->d_name[0] < '0' || fe->d_name[0] > '9')
				continue;
			str_copy(link, pdir, sizeof(link));
			at = 0;
			while (link[at])
				at++;
			link[at++] = '/';
			at = append_num_str(link, at, fe->d_name);
			link[at] = 0;

			n = readlink(link, target, sizeof(target) - 1);
			if (n <= 0)
				continue;
			target[n] = 0;
			if (str_eq(target, path)) {
				found = 1;
				break;
			}
		}
		closedir(fdd);
	}
	closedir(procd);
	return found;
}

/* Apply a width/height pair from the environment, keeping the defaults if missing or out of
 * range; the bound is the static output buffer (up to 1080p). Each camera takes its own pair. */
/* Still sizes are bounded only by what the HAL will configure and by memory:
 * the buffer is width * height * 3/2 bytes, about 12 MB at 12 megapixels. */
static void pick_still(const char *wstr, const char *hstr, int *w, int *h)
{
	int nw, nh;

	if (!wstr || !hstr)
		return;
	nw = atoi(wstr);
	nh = atoi(hstr);
	if (nw >= 320 && nh >= 240 && nw <= 8000 && nh <= 8000) {
		*w = nw;
		*h = nh;
	} else {
		say("bridge: ignoring out-of-range still size\n");
	}
}

static void pick_size(const char *wstr, const char *hstr, int *w, int *h)
{
	int nw, nh;

	if (!wstr || !hstr)
		return;
	nw = atoi(wstr);
	nh = atoi(hstr);
	if (nw >= 160 && nh >= 120 &&
	    (unsigned long)nw * (unsigned long)nh * 2 <= sizeof(yuyv_store[0])) {
		*w = nw;
		*h = nh;
	} else {
		say("bridge: ignoring out-of-range size\n");
	}
}

/* How many threads this process has. CamX creates a pipeline's worth per camera
 * session and destroys them again when the session closes, so this is a usable
 * proxy for "the teardown has finished". */
static int thread_count(void)
{
	struct pmos_dirent *e;
	void *d;
	int n = 0;

	d = opendir("/proc/self/task");
	if (!d)
		return -1;
	while ((e = readdir(d)) != 0)
		if (e->d_name[0] >= '0' && e->d_name[0] <= '9')
			n++;
	closedir(d);
	return n;
}

/* Wait for CamX to finish releasing a closed session: close() returns before the kernel-side
 * command buffers are freed, and a session started too early fails in InitializePool() with
 * "Out of memory". Wait for the thread count to stop falling, with a ceiling. */
static void wait_for_teardown(int max_ms)
{
	int last = thread_count();
	int stable = 0;
	int waited = 0;

	if (last < 0) {
		sleep_ms(3000);
		return;
	}
	while (waited < max_ms) {
		int now;

		sleep_ms(250);
		waited += 250;
		now = thread_count();
		if (now == last) {
			stable += 250;
			if (stable >= 1500)
				break;
		} else {
			stable = 0;
			last = now;
		}
	}
	say("teardown settled after ");
	say_num(waited);
	say(" ms, ");
	say_num(last);
	say(" threads left\n");
}

/* Bring one camera all the way up: open it, hand it its callbacks, configure a
 * single output stream, allocate and map the buffers it asked for, and open the
 * loopback node the frames go to. Returns 0 when the camera is ready to have
 * requests submitted to it. */
static int setup_cam(struct cam *c, const struct hw_module *mod,
		     const char *idstr, const char *outpath, int width, int height)
{
	unsigned int i;
	int rc;

	c->cbops.process_capture_result = on_result;
	c->cbops.notify = on_notify;
	c->cbops.request_stream_buffers = 0;
	c->cbops.return_stream_buffers = 0;
	c->idstr = idstr;
	c->outpath = outpath;
	c->width = width;
	c->height = height;
	if (!c->reopening)
		c->out_fd = -1;			/* kept across an idle pause */
	c->first_error_frame = -1;
	c->yuyv = yuyv_store[c - cams];

	say("\n=== bridge: camera ");
	say(idstr);
	say(chroma_cb_first ? " (NV12) " : " (NV21) ");
	say_num(width);
	say("x");
	say_num(height);
	say(" -> ");
	say(outpath ? outpath : "(nowhere)");
	say(" ===\n");

	/* Check the requested sizes against what the camera says it offers,
	 * before offering them. configure_streams does not refuse one stream, it
	 * refuses the whole configuration with -22, so a bad still size takes
	 * the preview down with it and the camera looks broken rather than
	 * misconfigured. Falling back to the largest supported size keeps a
	 * working camera in every case. */
	{
		const struct camera_module *cm = (const struct camera_module *)mod;
		struct camera_info_t info;
		unsigned int k;

		for (k = 0; k < sizeof(info); k++)
			((char *)&info)[k] = 0;
		if (cm->get_camera_info && !cm->get_camera_info(atoi(idstr), &info) &&
		    info.static_characteristics) {
			const void *meta = info.static_characteristics;
			int bw, bh;

			say("camera ");
			say(idstr);
			say(" offers:\n");
			say_sizes(meta, FORMAT_YCBCR_420_888, "preview (YCbCr_420_888)");
			say_sizes(meta, FORMAT_BLOB, "still (JPEG)");

			if (size_supported(meta, FORMAT_YCBCR_420_888, c->width,
					   c->height, sizeof(yuyv_store[0]),
					   &bw, &bh) == 0 && bw && bh) {
				say("  requested preview size is not offered; using ");
				say_num(bw);
				say("x");
				say_num(bh);
				say("\n");
				c->width = bw;
				c->height = bh;
				width = bw;
				height = bh;
			}
			{
				const int *r;
				unsigned int rn, ri;

				if (!meta_find_int32_array(meta, TAG_AE_FPS_RANGES,
							   &r, &rn)) {
					say("  auto-exposure may choose:");
					for (ri = 0; ri + 1 < rn && ri < 24; ri += 2) {
						say(" ");
						say_num(r[ri]);
						say("-");
						say_num(r[ri + 1]);
					}
					say(" fps\n");
				}
			}

			/* Say what the size in use can actually sustain. A size
			 * being offered says nothing about its frame rate: the
			 * largest ones on this hardware run at a few frames a
			 * second, which is fine for a photograph and useless
			 * for a video call. */
			{
				int fps = max_fps(meta, FORMAT_YCBCR_420_888,
						  c->width, c->height);

				if (fps) {
					say("  preview ");
					say_num(c->width);
					say("x");
					say_num(c->height);
					say(" sustains up to ");
					say_num(fps);
					say(" fps\n");
				}
			}

			if (c->still_w &&
			    size_supported(meta, FORMAT_BLOB, c->still_w,
					   c->still_h, 0, &bw, &bh) == 0 &&
			    bw && bh) {
				say("  requested still size is not offered; using ");
				say_num(bw);
				say("x");
				say_num(bh);
				say("\n");
				c->still_w = bw;
				c->still_h = bh;
			}
		}
	}

	rc = mod->methods->open(mod, idstr, (struct hw_device **)&c->dev);
	if (rc || !c->dev) {
		say("bridge: open failed, rc=");
		say_num(rc);
		say("\n");
		return -1;
	}
	say("opened, device version ");
	say_num(c->dev->common.version);
	say("\n");

	rc = c->dev->ops->initialize(c->dev, &c->cbops);
	say("initialize: rc=");
	say_num(rc);
	say("\n");
	if (rc)
		return -1;

	for (i = 0; i < sizeof(c->stream); i++)
		((char *)&c->stream)[i] = 0;
	c->stream.stream_type = STREAM_OUTPUT;
	c->stream.width = (unsigned int)width;
	c->stream.height = (unsigned int)height;
	c->stream.format = FORMAT_YCBCR_420_888;
	c->stream.usage = USAGE_SW_READ_OFTEN;
	c->stream.data_space = 0;
	c->stream.rotation = 0;
	c->streams[0] = &c->stream;

	c->stream_list[0] = &c->stream;
	c->cfg.num_streams = 1;
	c->cfg.streams = c->stream_list;

	if (c->still_w && c->still_h) {
		for (i = 0; i < sizeof(c->still_stream); i++)
			((char *)&c->still_stream)[i] = 0;
		c->still_stream.stream_type = STREAM_OUTPUT;
		c->still_stream.width = (unsigned int)c->still_w;
		c->still_stream.height = (unsigned int)c->still_h;
		c->still_stream.format = FORMAT_BLOB;
		c->still_stream.usage = USAGE_SW_READ_OFTEN;
		c->still_stream.data_space = DATASPACE_JFIF;
		c->still_stream.rotation = 0;
		c->stream_list[1] = &c->still_stream;
		c->cfg.num_streams = 2;
	}

	c->cfg.operation_mode = 0;
	c->cfg.session_parameters = 0;

	rc = c->dev->ops->configure_streams(c->dev, &c->cfg);
	say("configure_streams: rc=");
	say_num(rc);
	say(" -> usage=");
	say_num(c->stream.usage);
	say(" max_buffers=");
	say_num(c->stream.max_buffers);
	say("\n");
	if (rc)
		return -1;

	/* The HAL has just told us what it needs; allocate to that, plus CPU
	 * read so the frame can be copied out. */
	if (alloc_buffers(c, width, height, c->stream.usage | USAGE_SW_READ_OFTEN))
		return -1;
	say("allocated ");
	say_num(NUM_BUFFERS);
	say(" buffers\n");

	if (c->cfg.num_streams == 2) {
		/* A JPEG cannot be larger than the raw frame it came from, and
		 * in practice is a fraction of it; the HAL publishes an exact
		 * maximum in its static metadata, which this bridge does not
		 * parse, so the raw size is used as a safe upper bound. */
		/* Ask the HAL how big a JPEG it can produce, and allocate exactly
		 * that. Guessing does not work: a buffer that is merely large
		 * enough was refused by the snapshot pipeline's Import(), which
		 * wants the size the static metadata advertises. The raw frame
		 * size is only the fallback for a HAL that does not publish it. */
		{
			const struct camera_module *cm = (const struct camera_module *)mod;
			struct camera_info_t info;
			int maxjpeg = 0;
			unsigned int k;

			for (k = 0; k < sizeof(info); k++)
				((char *)&info)[k] = 0;
			c->still_size = (unsigned int)(c->still_w * c->still_h * 3 / 2);
			if (cm->get_camera_info &&
			    !cm->get_camera_info(atoi(c->idstr), &info) &&
			    !meta_find_int32(info.static_characteristics,
					     TAG_JPEG_MAX_SIZE, &maxjpeg) &&
			    maxjpeg > 0) {
				c->still_size = (unsigned int)maxjpeg;
				say("stills: HAL reports a maximum JPEG of ");
				say_num(maxjpeg);
				say(" bytes\n");
			} else {
				say("stills: no JPEG maximum in the metadata; guessing\n");
			}
		}
		c->still_buf = alloc_one((int)c->still_size, 1, FORMAT_BLOB,
					 c->still_stream.usage | USAGE_SW_READ_OFTEN);
		if (!c->still_buf) {
			say("bridge: no still buffer; photographs disabled\n");
			c->still_w = 0;
		} else {
			const int *bi = &c->still_buf->data[c->still_buf->num_fds];

			c->still_base = mmap(0, (unsigned long)(unsigned int)bi[INT_SIZE],
					     PROT_READ, MAP_SHARED,
					     c->still_buf->data[0], 0);
			if (c->still_base == (void *)-1)
				c->still_base = 0;
			c->still_settings = c->dev->ops->construct_default_request_settings(
						c->dev, TEMPLATE_STILL_CAPTURE);
			say("stills: ");
			say_num(c->still_w);
			say("x");
			say_num(c->still_h);
			say(c->still_settings ? " ready\n" : " no settings\n");
		}
	}

	/* Map each buffer once. Mapping per frame would cost a syscall pair and
	 * a TLB shootdown on every frame for no benefit: the HAL reuses this
	 * same set of buffers for the whole session. */
	{
		const int *ints = &c->bufs[0]->data[c->bufs[0]->num_fds];

		c->buf_stride = (unsigned int)ints[INT_ALIGNED_W];
		c->buf_aligned_h = (unsigned int)ints[INT_ALIGNED_H];
		for (i = 0; i < NUM_BUFFERS; i++) {
			const int *bi = &c->bufs[i]->data[c->bufs[i]->num_fds];

			c->bufbase[i] = mmap(0, (unsigned long)(unsigned int)bi[INT_SIZE],
					     PROT_READ, MAP_SHARED, c->bufs[i]->data[0], 0);
			if (c->bufbase[i] == (void *)-1)
				c->bufbase[i] = 0;
		}
	}

	if (outpath && c->out_fd < 0) {
		c->out_fd = open_loopback(outpath, width, height);
		if (c->out_fd >= 0) {
			say("streaming to ");
			say(outpath);
			say(" as ");
			say_num(width);
			say("x");
			say_num(height);
			say(" YUYV\n");
		} else {
			say("could not open ");
			say(outpath);
			say(" for output\n");
		}
	}

	c->settings = c->dev->ops->construct_default_request_settings(c->dev,
								      TEMPLATE_PREVIEW);
	say("default preview settings: ");
	say_num(c->settings ? 1 : 0);
	say("\n");
	if (!c->settings)
		return -1;

	c->running = 1;
	return 0;
}

/* Submit a request for every free buffer this camera has. Returns non-zero if
 * the HAL rejected one, which is fatal for that camera. */
static int feed_cam(struct cam *c)
{
	unsigned int i;
	int rc;

	for (i = 0; i < NUM_BUFFERS; i++) {
		if (c->inflight[i])
			continue;

		c->outbuf.stream = &c->stream;
		c->outbuf.buffer = &c->bufs[i];
		c->outbuf.status = 0;
		c->outbuf.acquire_fence = -1;
		c->outbuf.release_fence = -1;

		c->req.frame_number = c->frame_number;
		c->req.settings = c->settings;
		c->req.input_buffer = 0;
		c->req.num_output_buffers = 1;
		c->req.output_buffers = &c->outbuf;
		c->req.num_physcam_settings = 0;
		c->req.physcam_id = 0;
		c->req.physcam_settings = 0;

		c->inflight[i] = 1;

		/* A photograph rides along with an ordinary preview request:
		 * one request carrying both buffers, with the HAL's own
		 * still-capture template so that it picks the settings it wants
		 * for a still (longer exposure, full processing) rather than
		 * the preview ones. */
		if (c->want_still && c->still_buf && c->still_settings) {
			c->reqbufs[0] = c->outbuf;
			c->reqbufs[1].stream = &c->still_stream;
			c->reqbufs[1].buffer = &c->still_buf;
			c->reqbufs[1].status = 0;
			c->reqbufs[1].acquire_fence = -1;
			c->reqbufs[1].release_fence = -1;
			c->req.settings = c->still_settings;
			c->req.num_output_buffers = 2;
			c->req.output_buffers = c->reqbufs;
			c->want_still = 0;
			say("still: requested on camera ");
			say(c->idstr);
			say("\n");
		}

		rc = c->dev->ops->process_capture_request(c->dev, &c->req);
		if (rc) {
			c->inflight[i] = 0;
			say("camera ");
			say(c->idstr);
			say(": process_capture_request(");
			say_num(c->frame_number);
			say("): rc=");
			say_num(rc);
			say("\n");
			return rc;
		}
		c->frame_number++;
	}
	return 0;
}

static void report_cam(struct cam *c)
{
	say("camera ");
	say(c->idstr);
	say(": frames returned: ");
	say_num(c->frames_done);
	say(", errored: ");
	say_num(c->frames_errored);
	say(", shutters: ");
	say_num(c->shutters);
	say(", requests: ");
	say_num(c->frame_number);
	say(", written: ");
	say_num(c->frames_written);
	say("\n");
	if (c->first_error_frame >= 0) {
		/* 1 device, 2 request, 3 result, 4 buffer */
		say("  first notify error: code ");
		say_num(c->first_error_code);
		say(" on frame ");
		say_num(c->first_error_frame);
		say("\n");
	}
}

/* flush() asks the HAL to return everything in flight; close() must not race
 * it, or the session is torn down with requests still owned by the pipeline and
 * the kernel keeps the CDM client that the next start_dev then trips over. */
static void close_cam(struct cam *c)
{
	int waited;
	unsigned int i;

	if (!c->dev)
		return;
	c->dev->ops->flush(c->dev);
	for (waited = 0; waited < 1000; waited += 10) {
		int busy = 0;

		for (i = 0; i < NUM_BUFFERS; i++)
			busy |= c->inflight[i];
		if (!busy)
			break;
		sleep_ms(10);
	}
	c->dev->common.close(&c->dev->common);
	c->dev = 0;
	c->running = 0;
	/* The loopback fd stays open on purpose: v4l2loopback forgets the format
	 * when the last producer closes, and with no format the node stops
	 * looking like a camera at all. Holding it keeps the node advertised
	 * while the sensor is off, so an application can still find it and its
	 * first read is what wakes the camera up. */
}

/* How long a camera keeps streaming with nobody reading its node, and how often
 * that is checked. The check walks /proc, so it is not free; two seconds is
 * often enough to notice an application starting, and rare enough to cost
 * nothing measurable. */
static int idle_seconds = 30;
#define IDLE_CHECK_MS	2000

static void pause_cam(struct cam *c)
{
	say("camera ");
	say(c->idstr);
	say(": nothing reading ");
	say(c->outpath);
	say(", stopping the sensor\n");
	close_cam(c);
	c->idle = 1;
	wait_for_teardown(15000);
}

static void wake_cam(struct cam *c, const struct hw_module *mod)
{
	say("camera ");
	say(c->idstr);
	say(": a reader appeared, starting the sensor\n");
	arm_watchdog(120);
	c->idle = 0;
	c->reopening = 1;
	c->frame_number = 0;
	c->last_done = 0;
	c->frames_done = 0;
	c->frames_errored = 0;
	c->frames_written = 0;
	c->got_frame = 0;
	c->rate_start_cs = 0;
	c->rate_done = 0;
	c->shutters = 0;
	c->done_handle = 0;
	for (int i = 0; i < NUM_BUFFERS; i++)
		c->inflight[i] = 0;
	if (setup_cam(c, mod, c->idstr, c->outpath, c->width, c->height)) {
		say("camera ");
		say(c->idstr);
		say(": could not restart; leaving it stopped\n");
		close_cam(c);
		c->idle = 1;
	} else if (c->out_fd >= 0) {
		set_loopback_format(c->out_fd, c->width, c->height);
	}
	c->reopening = 0;
}

__attribute__((constructor))
static void bridge(void)
{
	int (*hw_get_module)(const char *id, const struct hw_module **module);
	const struct hw_module *mod = 0;
	const char *chromastr;
	const char *idstr;
	const char *outpath;
	void *libhw;
	int width = 640, height = 480;
	int width2, height2;
	int still_w, still_h, still_w2, still_h2;
	int waited;
	int idle_check_ms = 0;
	int heartbeat_ms = 0;
	int n;

	/* Yield to the desktop: the CamX pipeline threads run SCHED_OTHER at nice 0 like the
	 * compositor, and a late frame is invisible while a late redraw is not. setpriority()
	 * rather than a nice(1) wrapper, which would drag LD_PRELOAD into a musl helper. */
	{
		const char *nicestr = env_get("PMOS_CAM_NICE");
		int prio = nicestr ? atoi(nicestr) : 10;

		if (prio)
			setpriority(PRIO_PROCESS, 0, prio);
	}

	{
		const char *idle = env_get("PMOS_CAM_IDLE");

		if (idle)
			idle_seconds = atoi(idle);
	}

	chromastr = env_get("PMOS_CAM_NV12");
	chroma_cb_first = chromastr && chromastr[0] == '1';

	/* PMOS_CAM_W/H pick the stream size. camera3 only accepts sizes the HAL
	 * advertises, so a bad pair fails in configure_streams rather than here;
	 * the bound is the static output buffer, which holds up to 1080p. */
	pick_size(env_get("PMOS_CAM_W"), env_get("PMOS_CAM_H"), &width, &height);
	still_w = still_h = 0;
	pick_still(env_get("PMOS_CAM_STILL_W"), env_get("PMOS_CAM_STILL_H"),
		   &still_w, &still_h);
	still_w2 = still_w;
	still_h2 = still_h;
	pick_still(env_get("PMOS_CAM_STILL_W2"), env_get("PMOS_CAM_STILL_H2"),
		   &still_w2, &still_h2);
	width2 = width;
	height2 = height;
	pick_size(env_get("PMOS_CAM_W2"), env_get("PMOS_CAM_H2"), &width2, &height2);

	libhw = dlopen("libhardware.so", RTLD_NOW);
	if (!libhw) {
		say("bridge: no libhardware.so\n");
		return;
	}
	hw_get_module = dlsym(libhw, "hw_get_module");
	if (!hw_get_module || hw_get_module("camera", &mod) || !mod) {
		say("bridge: hw_get_module(camera) failed\n");
		return;
	}
	if (((struct camera_module *)mod)->init)
		((struct camera_module *)mod)->init();

	signal(SIGALRM, on_watchdog);
	arm_watchdog(180);

	idstr = env_get("PMOS_CAM_ID");
	outpath = env_get("PMOS_CAM_OUT");
	cams[0].still_w = still_w;
	cams[0].still_h = still_h;
	if (setup_cam(&cams[0], mod, idstr ? idstr : "0", outpath, width, height)) {
		/* Close whatever did open. Leaving a half-opened session behind
		 * is what makes the NEXT start oops the kernel. */
		close_cam(&cams[0]);
		return;
	}
	ncams = 1;

	/* The second camera is optional and its failure is not fatal: one
	 * working camera is better than none, and the sensors are not equally
	 * willing to share the ISP. */
	idstr = env_get("PMOS_CAM_ID2");
	outpath = env_get("PMOS_CAM_OUT2");
	if (idstr && outpath) {
		cams[1].still_w = still_w2;
		cams[1].still_h = still_h2;
		if (!setup_cam(&cams[1], mod, idstr, outpath, width2, height2)) {
			ncams = 2;
		} else {
			close_cam(&cams[1]);
			say("bridge: second camera did not start; carrying on with one\n");
		}
	}

	signal(SIGTERM, on_signal);
	signal(SIGINT, on_signal);
	signal(SIGUSR1, on_still_signal);
	signal(SIGUSR2, on_still_signal);

	/* Keep the pipelines fed: a camera3 HAL wants a request in flight for every frame, queued
	 * ahead by the pipeline delay, or the kernel skips frames ("Skip Frame: req 1 not ready").
	 * Submit whenever a buffer is free and reuse each buffer as soon as it comes back. */
	waited = 0;
	while (!stop_requested && waited < CAPTURE_TIMEOUT_MS) {
		int progress = 0;
		int live = 0;
		int paused = 0;

		/* Pause a camera nothing is reading and wake it when something opens its node.
		 * Waking takes the HAL several seconds, so the first read sees a stalled node;
		 * CAMERA_IDLE_SECONDS=0 turns this off. */
		if (idle_seconds > 0) {
			idle_check_ms += 5;
			if (idle_check_ms >= IDLE_CHECK_MS) {
				int self = getpid();

				idle_check_ms = 0;
				for (n = 0; n < ncams; n++) {
					struct cam *c = &cams[n];

					if (!c->outpath)
						continue;
					if (node_has_reader(c->outpath, self)) {
						c->no_reader_ms = 0;
						if (c->idle)
							wake_cam(c, mod);
					} else if (c->running) {
						c->no_reader_ms += IDLE_CHECK_MS;
						if (c->no_reader_ms >= idle_seconds * 1000)
							pause_cam(c);
					}
				}
			}
		}

		for (n = 0; n < ncams; n++) {
			struct cam *c = &cams[n];

			if (c->idle) {
				paused++;
				continue;
			}
			if (!c->running)
				continue;
			if (feed_cam(c)) {
				close_cam(c);
				continue;
			}
			live++;
			if (c->frames_done != c->last_done) {
				c->last_done = c->frames_done;
				progress = 1;
			}

			/* Report the delivered frame rate once per session: v4l2loopback hands a
			 * reader the current frame as fast as it asks, so it cannot be measured
			 * from outside. */
			if (!c->rate_done) {
				long elapsed;

				if (!c->rate_start_cs)
					c->rate_start_cs = now_cs();
				elapsed = now_cs() - c->rate_start_cs;
				if (elapsed >= 1000) {		/* ten seconds */
					c->rate_done = 1;
					say("camera ");
					say(c->idstr);
					say(": delivering ");
					say_num(c->frames_written * 100 / elapsed);
					say(" fps at ");
					say_num(c->width);
					say("x");
					say_num(c->height);
					say("\n");
				}
			}
		}
		if (!live && !paused)
			break;

		/* Keep a paused camera's node alive by rewriting its last frame once a second:
		 * v4l2loopback ties the node's usable state to a producer that is writing, and
		 * readers of a silent node get a stale frame or nothing. */
		if (paused) {
			heartbeat_ms += 5;
			if (heartbeat_ms >= 1000) {
				heartbeat_ms = 0;
				for (n = 0; n < ncams; n++) {
					struct cam *c = &cams[n];

					if (c->idle && c->out_fd >= 0)
						write(c->out_fd, c->yuyv,
						      (unsigned long)c->width *
						      c->height * 2);
				}
			}
		}

		if (!live) {
			/* Everything is parked: nothing can make progress, so
			 * the stall deadline must not run either. */
			sleep_ms(5);
			waited = 0;
			continue;
		}

		/* Always pace the loop. Advancing the deadline only when
		 * nothing was submitted made this spin at 100% of a core,
		 * because buffers that come straight back as errors keep the
		 * queue permanently drainable and the timeout never arrived. */
		sleep_ms(5);
		/* PROGRESS means a frame counter moved, not that it is nonzero: testing frames_done
		 * reset the deadline forever once one frame had arrived, and a dead pipeline then
		 * held the camera open. Exit so the supervisor starts a fresh session. */
		if (progress)
			waited = 0;
		else
			waited += 5;
	}

	for (n = 0; n < ncams; n++)
		report_cam(&cams[n]);

	if (cams[0].frames_done && cams[0].done_handle)
		save_frame(cams[0].done_handle, cams[0].width, cams[0].height,
			   "/tmp/frame.nv12");

	for (n = 0; n < ncams; n++)
		close_cam(&cams[n]);

	/* Let CamX finish tearing down before this process disappears; killing it mid-release
	 * poisons the next session, and unloading camera.ko panics the kernel instead. */
	wait_for_teardown(20000);
	say("=== bridge done ===\n");
}
