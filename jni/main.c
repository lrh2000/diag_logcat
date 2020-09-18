/*
 * diag_logcat
 *
 * It is modified from MobileInsight's diag_revealer.
 * Thanks for MobileInsight team's work!
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include "common.h"
#include "diag_interface.h"

struct buffer_t {
	size_t len;
	char *buf;
};

static const struct diag_interface_t *diag_available_interfaces[] = {
	&diag_char_interface,
	&diag_serial_interface,
	NULL,
};

static const struct diag_interface_t *diag_interface;
static diag_handle_t diag_handle;

/*
 * Read the file content into a buffer.
 * If it fails, an empty buffer will be returned.
 */
static struct buffer_t read_file(const char *filename)
{
	struct buffer_t ret;
	FILE *fp;
	ssize_t sz;

	fp = fopen(filename, "rb");
	if (fp == NULL) {
		LOGE("Failed to open the file %s (%s)\n", filename, strerror(errno));
		goto fail;
	}
	fseek(fp, 0L, SEEK_END);
	sz = ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	if (sz <= 0) {
		LOGE("Failed to get the file size of %s (%s)\n", filename, strerror(errno));
		goto fail;
	}

	ret.len = sz;
	ret.buf = (char *) malloc(sz);
	if (!ret.buf) {
		LOGE("Failed to allocate memory for the file buffer\n");
		goto fail;
	}
	sz = fread(ret.buf, sizeof(char), ret.len, fp);
	if (sz != ret.len) {
		LOGE("Failed to read data from the file %s (%s)\n",
		     filename, sz >= 0 ? "Read incompletely" : strerror(errno));
		goto fail;
	}

	fclose(fp);
	return ret;
fail:
	ret.buf = NULL;
	ret.len = 0;
	fclose(fp);
	return ret;
}

static int write_commands(const struct buffer_t *cmd_buffer)
{
	char *now = cmd_buffer->buf;
	char *end = now + cmd_buffer->len;

	while (now < end) {
		size_t len = 0;
		ssize_t wlen;
		while (now + len < end && now[len] != 0x7e)
			++len;
		if (now + len >= end)
			break;
		++len;

		if (len >= 3) {
			wlen = (*diag_interface->write)(diag_handle, now, len);
			if (wlen != len)
				return -1;
		}
		now += len;
	}

	return 0;
}

static int retrieve_logs(FILE *data_log, FILE *stamp_log)
{
	const void *buf;
	ssize_t len, wlen;
	long stamp;
	struct {
		uint64_t offset;
		uint64_t stamp;
	} slog;

	slog.offset = 0;
	for (;;) {
		len = (*diag_interface->read)(diag_handle, &buf, &stamp);
		if (len <= 0)
			return -1;
		wlen = fwrite(buf, 1, len, data_log);
		if (wlen != len)
			return -2;
		slog.offset += wlen;
		if (stamp >= 0) {
			slog.stamp = stamp;
			wlen = fwrite(&slog, sizeof(slog), 1, stamp_log);
			if (wlen != 1)
				return -3;
		}
	}
}

void on_sigint(int dummy)
{
	exit(0);
}

int main(int argc, char **argv)
{
	struct buffer_t cmd_buffer;
	FILE *data_log, *stamp_log;
	int i, ret;

	signal(SIGINT, &on_sigint);
	if (argc != 4)
		return -8000;

	data_log = fopen(argv[2], "wb");
	if (!data_log) {
		LOGE("Cannot open the file %s for writing data logs (%s)\n",
		     argv[2], strerror(errno));
		return -8001;
	}
	stamp_log = fopen(argv[3], "wb");
	if (!stamp_log) {
		LOGE("Cannot open the file %s for writing stamp logs (%s)\n",
		     argv[3], strerror(errno));
		return -8002;
	}

	// Read the config file
	cmd_buffer = read_file(argv[1]);
	if (cmd_buffer.buf == NULL || cmd_buffer.len == 0)
		return -8003;

	i = 0;
	while (diag_available_interfaces[i]) {
		diag_interface = diag_available_interfaces[i];
		diag_handle = (*diag_interface->open)();
		if (diag_handle)
			break;
		++i;
	}
	if (!diag_handle)
		return -8004;

	ret = write_commands(&cmd_buffer);
	free(cmd_buffer.buf);
	if (ret != 0)
		return -8005;

	return retrieve_logs(data_log, stamp_log);
}
