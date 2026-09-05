/* qpnp-volume-relay: re-emit KEY_VOLUMEDOWN from qpnp_pon on a uinput device, so the real
 * device (which also carries KEY_POWER) can be hidden from libinput. No EVIOCGRAB: evdev
 * fans events out to every reader and a grab would starve powerbutton-daemon. */
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static volatile sig_atomic_t stop;

static void on_sig(int sig)
{
	(void)sig;
	stop = 1;
}

int main(int argc, char **argv)
{
	const char *src = argc > 1 ? argv[1] : "/dev/input/event0";
	struct sigaction sa;

	/* No SA_RESTART: a signal must interrupt the blocking read below. */
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_sig;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);

	int ufd = open("/dev/uinput", O_WRONLY);
	if (ufd < 0) {
		perror("qpnp-volume-relay: open /dev/uinput");
		return 1;
	}
	if (ioctl(ufd, UI_SET_EVBIT, EV_KEY) < 0 ||
	    ioctl(ufd, UI_SET_KEYBIT, KEY_VOLUMEDOWN) < 0) {
		perror("qpnp-volume-relay: uinput capability setup");
		close(ufd);
		return 1;
	}

	struct uinput_user_dev ud;
	memset(&ud, 0, sizeof ud);
	snprintf(ud.name, sizeof ud.name, "gts7fewifi-volume-relay");
	ud.id.bustype = BUS_VIRTUAL;
	if (write(ufd, &ud, sizeof ud) != (ssize_t)sizeof ud) {
		perror("qpnp-volume-relay: write uinput_user_dev");
		close(ufd);
		return 1;
	}
	if (ioctl(ufd, UI_DEV_CREATE) < 0) {
		perror("qpnp-volume-relay: UI_DEV_CREATE");
		close(ufd);
		return 1;
	}

	while (!stop) {
		/* The source node may not exist yet at boot, and can go away
		 * on a driver reset; retry forever, supervise-daemon only
		 * has to keep THIS process alive. */
		int sfd = open(src, O_RDONLY);
		if (sfd < 0) {
			sleep(1);
			continue;
		}
		for (;;) {
			struct input_event ev;
			ssize_t n = read(sfd, &ev, sizeof ev);
			if (n != (ssize_t)sizeof ev)
				break; /* EOF, error, or EINTR from a signal */
			if (ev.type == EV_KEY && ev.code == KEY_VOLUMEDOWN) {
				struct input_event out[2];
				out[0] = ev;
				memset(&out[1], 0, sizeof out[1]);
				out[1].type = EV_SYN;
				out[1].code = SYN_REPORT;
				/* input core restamps injected events; a failed
				 * write loses one volume event, nothing to do */
				if (write(ufd, out, sizeof out) < 0 && errno == EINTR)
					break;
			}
			if (stop)
				break;
		}
		close(sfd);
		if (!stop)
			sleep(1);
	}

	ioctl(ufd, UI_DEV_DESTROY);
	close(ufd);
	return 0;
}
