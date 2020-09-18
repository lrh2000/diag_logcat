#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include "common.h"
#include "diag_interface.h"

#define BUFFER_SIZE 65536

struct diag_serial_handle_t {
	int fd;
	int drop_first;
	char buf[BUFFER_SIZE];
};

static diag_handle_t diag_serial_open(void)
{
	struct diag_serial_handle_t *handle;
	struct termios tio;
	int ret;

	handle = malloc(sizeof(struct diag_serial_handle_t));
	if (!handle) {
		LOGE("Cannot allocate memory for diag_serial_handle_t\n");
		return 0;
	}
	handle->fd = open("/dev/ttyUSB0", O_RDWR | O_SYNC);
	if (handle->fd < 0) {
		LOGE("Cannot open /dev/ttyUSB0 (%s)\n", strerror(errno));
		goto out;
	}

	ret = tcgetattr(handle->fd, &tio);
	if (ret < 0) {
		LOGE("Failed to get serial settings (%s)\n", strerror(errno));
		goto out;
	}
	cfmakeraw(&tio);
	tio.c_cflag |= CREAD | CLOCAL;
	cfsetospeed(&tio, B115200);
	cfsetispeed(&tio, B115200);
	tcflush(handle->fd, TCIOFLUSH);
	ret = tcsetattr(handle->fd, TCSANOW, &tio);
	if (ret < 0) {
		LOGE("Failed to set serial settings (%s)\n", strerror(errno));
		goto out;
	}

	handle->drop_first = 3;
	return (diag_handle_t) handle;
out:
	if (handle->fd >= 0)
		close(handle->fd);
	free(handle);
	return 0;
}

static ssize_t diag_serial_read(diag_handle_t handle_, const void **buf, long *stamp)
{
	struct diag_serial_handle_t *handle = (struct diag_serial_handle_t *) handle_;

	for (;;) {
		ssize_t len = read(handle->fd, handle->buf, BUFFER_SIZE);
		long current_stamp = get_posix_timestamp();

		if (handle->drop_first) {
			char *msg = NULL;
			ssize_t i;
			for (i = 0; i < len && !msg; ++i)
				if (handle->buf[i] == 0x7e)
					msg = &handle->buf[i + 1];
			if (msg)
				--handle->drop_first;
			if (handle->drop_first)
				continue;
			if (msg == handle->buf + len)
				continue;
			*buf = msg;
			if (stamp)
				*stamp = current_stamp;
			return handle->buf + len - msg;
		}
		*buf = handle->buf;
		if (stamp)
			*stamp = current_stamp;
		return len;
	}
}

static ssize_t diag_serial_write(diag_handle_t handle_, const void *buf, size_t len)
{
	struct diag_serial_handle_t *handle = (struct diag_serial_handle_t *) handle_;
	int ret;

	ret = write(handle->fd, buf, len);
	if (ret != len) {
		LOGE("Failed to write into /dev/ttyUSB0 (%s)\n",
		     ret >= 0 ? "Write incompletely" : strerror(errno));
		goto out;
	}

	ret = read(handle->fd, handle->buf, BUFFER_SIZE);
	if (ret <= 0) {
		LOGE("Failed to receive responses from /dev/ttyUSB0 (%s)\n",
		     len == 0 ? "Empty response" : strerror(errno));
		goto out;
	}
	ret = len;

out:
	handle->drop_first = 3;
	return ret;
}

static void diag_serial_close(diag_handle_t handle_)
{
	struct diag_serial_handle_t *handle = (struct diag_serial_handle_t *) handle_;

	close(handle->fd);
	free(handle);
}

const struct diag_interface_t diag_serial_interface = {
	.open = &diag_serial_open,
	.read = &diag_serial_read,
	.write = &diag_serial_write,
	.close = &diag_serial_close,
};
