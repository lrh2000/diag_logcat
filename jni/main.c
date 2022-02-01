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
	if (sz < 0) {
		LOGE("Failed to get the file size of %s (%s)\n", filename, strerror(errno));
		goto fail;
	}
	if (sz == 0) {
		LOGE("Empty file %s\n", filename);
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
	if (fp != NULL)
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

static int retrieve_logs(const char *data_log_prefix,
			 const char *stamp_log_prefix)
{
	static char data_log_name[FILENAME_MAX];
	static char stamp_log_name[FILENAME_MAX];

	size_t dlog_plen = strlen(data_log_prefix);
	size_t tlog_plen = strlen(stamp_log_prefix);

	if (dlog_plen > FILENAME_MAX - 11) {
		LOGE("Invalid argument: data log prefix is too long\n");
		return -4;
	}
	if (tlog_plen > FILENAME_MAX - 11) {
		LOGE("Invalid argument: stamp log prefix is too long\n");
		return -5;
	}

	strcpy(data_log_name, data_log_prefix);
	strcpy(stamp_log_name, stamp_log_prefix);

	strcpy(data_log_name + dlog_plen, ".0000.dlog");
	strcpy(stamp_log_name + tlog_plen, ".0000.tlog");

	const void *buf;
	ssize_t len, wlen;
	long stamp, last_stamp;
	struct {
		uint64_t offset;
		uint64_t stamp;
	} slog;
	FILE *data_log, *stamp_log;

	last_stamp = 0;
	slog.offset = 0;
	for (;;) {
		if (!data_log)
			data_log = fopen(data_log_name, "wb");
		if (!data_log) {
			LOGE("Failed to open data log at %s\n", data_log_name);
			return -5;
		}
		if (!stamp_log)
			stamp_log = fopen(stamp_log_name, "wb");
		if (!stamp_log) {
			LOGE("Failed to open stamp log at %s\n", stamp_log_name);
			return -6;
		}

		len = (*diag_interface->read)(diag_handle, &buf, &stamp);
		if (len <= 0)
			return -1;

		wlen = fwrite(buf, 1, len, data_log);
		if (wlen != len) {
			LOGE("Failed to write to data log at %s\n", data_log_name);
			return -2;
		}

		slog.offset += wlen;
		if (stamp < 0)
			continue;
		slog.stamp = stamp;

		wlen = fwrite(&slog, sizeof(slog), 1, stamp_log);
		if (wlen != 1) {
			LOGE("Failed to write to stamp log at %s\n", stamp_log_name);
			return -3;
		}

		if (stamp < last_stamp + 1000000000)
			continue;
		last_stamp = stamp;
		fclose(data_log);
		fclose(stamp_log);
		data_log = NULL;
		stamp_log = NULL;

		if ((data_log_name[dlog_plen + 4]++, stamp_log_name[tlog_plen + 4]++) != '9')
			continue;
		data_log_name[dlog_plen + 4] = stamp_log_name[tlog_plen + 4] = '0';
		if ((data_log_name[dlog_plen + 3]++, stamp_log_name[tlog_plen + 3]++) != '9')
			continue;
		data_log_name[dlog_plen + 3] = stamp_log_name[tlog_plen + 3] = '0';
		if ((data_log_name[dlog_plen + 2]++, stamp_log_name[tlog_plen + 2]++) != '9')
			continue;
		data_log_name[dlog_plen + 2] = stamp_log_name[tlog_plen + 2] = '0';
		if ((data_log_name[dlog_plen + 1]++, stamp_log_name[tlog_plen + 1]++) != '9')
			continue;
		data_log_name[dlog_plen + 1] = stamp_log_name[tlog_plen + 1] = '0';
		LOGE("The number of the log files has overflowed\n");
		return -7;
	}
}

static void on_sigint(int dummy)
{
	exit(0);
}

int main(int argc, char **argv)
{
	struct buffer_t cmd_buffer;
	int i, ret;

	signal(SIGINT, &on_sigint);
	if (argc != 4) {
		printf("Usage: %s [DIAG CFG] [DLOG PREFIX] [TLOG PREFIX]\n", argv[0]);
		return -8000;
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

	return retrieve_logs(argv[2], argv[3]);
}
