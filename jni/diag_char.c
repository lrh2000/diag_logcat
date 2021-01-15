#include <stdint.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <malloc.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "common.h"
#include "diag_interface.h"

#define BUFFER_SIZE 65536

#ifndef __packed
#define __packed __attribute__ ((packed))
#endif

struct diag_char_handle_t {
	int fd;
	int dci_client;
	uint16_t remote_dev;

	long stamp;
	int msg_id;
	union {
		int *msg_size;
		char *msg_start;
	};
	union {
		char buf[BUFFER_SIZE];
		struct {
			uint32_t msg_type;
			union {
				uint32_t msg_num;
				uint32_t msg_dev;
			};
		} __packed;
	};
};

/*
 * From https://android.googlesource.com/kernel/msm/+/refs/tags/android-10.0.0_r0.87/include/linux/diagchar.h:
 */

#define MSG_MASKS_TYPE		0x00000001
#define LOG_MASKS_TYPE		0x00000002
#define EVENT_MASKS_TYPE	0x00000004
#define PKT_TYPE		0x00000008
#define DEINIT_TYPE		0x00000010
#define USER_SPACE_DATA_TYPE	0x00000020
#define DCI_DATA_TYPE		0x00000040
#define USER_SPACE_RAW_DATA_TYPE	0x00000080
#define DCI_LOG_MASKS_TYPE	0x00000100
#define DCI_EVENT_MASKS_TYPE	0x00000200
#define DCI_PKT_TYPE		0x00000400
#define HDLC_SUPPORT_TYPE	0x00001000

#define USB_MODE			1
#define MEMORY_DEVICE_MODE		2
#define NO_LOGGING_MODE			3
#define UART_MODE			4
#define SOCKET_MODE			5
#define CALLBACK_MODE			6
#define PCIE_MODE			7

/* Different IOCTL values */
#define DIAG_IOCTL_COMMAND_REG		0
#define DIAG_IOCTL_COMMAND_DEREG	1
#define DIAG_IOCTL_SWITCH_LOGGING	7
#define DIAG_IOCTL_GET_DELAYED_RSP_ID	8
#define DIAG_IOCTL_LSM_DEINIT		9
#define DIAG_IOCTL_DCI_INIT		20
#define DIAG_IOCTL_DCI_DEINIT		21
#define DIAG_IOCTL_DCI_SUPPORT		22
#define DIAG_IOCTL_DCI_REG		23
#define DIAG_IOCTL_DCI_STREAM_INIT	24
#define DIAG_IOCTL_DCI_HEALTH_STATS	25
#define DIAG_IOCTL_DCI_LOG_STATUS	26
#define DIAG_IOCTL_DCI_EVENT_STATUS	27
#define DIAG_IOCTL_DCI_CLEAR_LOGS	28
#define DIAG_IOCTL_DCI_CLEAR_EVENTS	29
#define DIAG_IOCTL_REMOTE_DEV		32
#define DIAG_IOCTL_VOTE_REAL_TIME	33
#define DIAG_IOCTL_GET_REAL_TIME	34
#define DIAG_IOCTL_PERIPHERAL_BUF_CONFIG	35
#define DIAG_IOCTL_PERIPHERAL_BUF_DRAIN		36
#define DIAG_IOCTL_REGISTER_CALLBACK	37
#define DIAG_IOCTL_HDLC_TOGGLE	38
#define DIAG_IOCTL_QUERY_PD_LOGGING	39
#define DIAG_IOCTL_QUERY_CON_ALL	40
#define DIAG_IOCTL_QUERY_MD_PID	41

/*
 * From https://android.googlesource.com/kernel/msm/+/refs/tags/android-10.0.0_r0.87/drivers/char/diag/diagchar.h:
 */

#define DIAG_CON_APSS		(0x0001)	/* Bit mask for APSS */
#define DIAG_CON_MPSS		(0x0002)	/* Bit mask for MPSS */
#define DIAG_CON_LPASS		(0x0004)	/* Bit mask for LPASS */
#define DIAG_CON_WCNSS		(0x0008)	/* Bit mask for WCNSS */
#define DIAG_CON_SENSORS	(0x0010)	/* Bit mask for Sensors */
#define DIAG_CON_WDSP		(0x0020)	/* Bit mask for WDSP */
#define DIAG_CON_CDSP		(0x0040)	/* Bit mask for CDSP */
#define DIAG_CON_NPU		(0x0080)	/* Bit mask for NPU */

#define DIAG_CON_UPD_WLAN		(0x1000) /*Bit mask for WLAN PD*/
#define DIAG_CON_UPD_AUDIO		(0x2000) /*Bit mask for AUDIO PD*/
#define DIAG_CON_UPD_SENSORS	(0x4000) /*Bit mask for SENSORS PD*/

#define DIAG_CON_NONE		(0x0000)	/* Bit mask for No SS*/
#define DIAG_CON_ALL		(DIAG_CON_APSS | DIAG_CON_MPSS \
				| DIAG_CON_LPASS | DIAG_CON_WCNSS \
				| DIAG_CON_SENSORS | DIAG_CON_WDSP \
				| DIAG_CON_CDSP | DIAG_CON_NPU)
#define DIAG_CON_UPD_ALL	(DIAG_CON_UPD_WLAN \
				| DIAG_CON_UPD_AUDIO \
				| DIAG_CON_UPD_SENSORS)

#define DIAG_BUFFERING_MODE_STREAMING	0
#define DIAG_BUFFERING_MODE_THRESHOLD	1
#define DIAG_BUFFERING_MODE_CIRCULAR	2

#define DEFAULT_LOW_WM_VAL	15
#define DEFAULT_HIGH_WM_VAL	85

#define PERIPHERAL_MODEM	0
#define PERIPHERAL_LPASS	1
#define PERIPHERAL_WCNSS	2
#define PERIPHERAL_SENSORS	3
#define PERIPHERAL_WDSP		4
#define PERIPHERAL_CDSP		5
#define PERIPHERAL_NPU		6
#define NUM_PERIPHERALS		7
#define APPS_DATA		(NUM_PERIPHERALS)

/* List of remote processor supported */
enum remote_procs {
	MDM = 1,
	MDM2 = 2,
	QSC = 5,
};
#define DIAG_MD_LOCAL		0
#define DIAG_MD_LOCAL_LAST	1
#define DIAG_MD_BRIDGE_BASE	DIAG_MD_LOCAL_LAST
#define DIAG_MD_MDM		(DIAG_MD_BRIDGE_BASE)
#define DIAG_MD_MDM2		(DIAG_MD_BRIDGE_BASE + 1)
#define DIAG_MD_BRIDGE_LAST	(DIAG_MD_BRIDGE_BASE + 2)

struct diag_buffering_mode_t {
	uint8_t peripheral;
	uint8_t mode;
	uint8_t high_wm_val;
	uint8_t low_wm_val;
} __packed;

struct diag_logging_mode_param_t {
	uint32_t req_mode;
	uint32_t peripheral_mask;
	uint32_t pd_mask;
	uint8_t mode_param;
	uint8_t diag_id;
	uint8_t pd_val;
	uint8_t reserved;
	int peripheral;
	int device_mask;
} __packed;

struct diag_con_all_param_t {
	uint32_t diag_con_all;
	uint32_t num_peripherals;
	uint32_t upd_map_supported;
};

/*
 * From https://android.googlesource.com/kernel/msm/+/refs/tags/android-10.0.0_r0.87/drivers/char/diag/diag_dci.h:
 */

#define DCI_LOCAL_PROC		0
#define DCI_REMOTE_BASE		1
#define DCI_MDM_PROC		DCI_REMOTE_BASE
#define DCI_REMOTE_LAST		(DCI_REMOTE_BASE + 1)

struct diag_dci_reg_tbl_t {
	int client_id;
	uint16_t notification_list;
	int signal_type;
	int token;
} __packed;

/*
 * Android 9.0: diag_logging_mode_param_t structure
 * Reference: https://android.googlesource.com/kernel/msm.git/+/android-9.0.0_r0.31/drivers/char/diag/diagchar.h
 */
struct diag_logging_mode_param_v9 {
	uint32_t req_mode;
	uint32_t peripheral_mask;
	uint32_t pd_mask;
	uint8_t mode_param;
	uint8_t diag_id;
	uint8_t pd_val;
	uint8_t reserved;
	int peripheral;
} __packed;

/*
 * Android 7.0: diag_logging_mode_param_t structure
 * Reference: https://android.googlesource.com/kernel/msm.git/+/android-7.1.0_r0.3/drivers/char/diag/diagchar.h
 */
struct diag_logging_mode_param_v7 {
	uint32_t req_mode;
	uint32_t peripheral_mask;
	uint8_t mode_param;
} __packed;

/*
 * Motorola Nexus 6: special ioctl requests
 * Reference: https://github.com/MotorolaMobilityLLC/kernel-msm/blob/kitkat-4.4.4-release-victara/include/linux/diagchar.h
 */
#define DIAG_IOCTL_OPTIMIZED_LOGGING		35
#define DIAG_IOCTL_OPTIMIZED_LOGGING_FLUSH	36

/*
 * Explicitly probe the length of the argument that ioctl(fd, req, ...) takes.
 *
 * Assumptions:
 *  1. The length is fixed.
 *  2. The insufficient length is the only reason to make ioctl(fd, req, ...)
 *     fail and set errno to EFAULT.
 *  3. The argument filled with 0x3f won't cause unrecoverable errors, or
 *     interfere with what we're going to do next.
 */
static ssize_t probe_ioctl_arglen(int fd, int req, size_t maxlen)
{
	size_t pagesize = sysconf(_SC_PAGESIZE);
	char *p;
	size_t len;

	if (maxlen > pagesize) {
		LOGE("probe_ioctl_arglen: maxlen > pagesize is not implemented\n");
		return -1;
	}

	p = mmap(NULL, pagesize * 2, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
	if (p == MAP_FAILED) {
		LOGE("probe_ioctl_arglen: mmap fails (%s)\n", strerror(errno));
		return -1;
	}
	p += pagesize;
	munmap(p, pagesize);
	memset(p - maxlen, 0x3f, maxlen);

	for (len = 0; len <= maxlen; ++len) {
		if (ioctl(fd, req, p - len) >= 0)
			break;
		if (errno != EFAULT)
			break;
	}
	munmap(p - pagesize, pagesize);
	return len;
}

/*
 * Calling functions into libdiag.so will create several threads. For example,
 * in diag_switch_logging, three threads (disk_write_hdl, qsr4_db_parser_thread_hdl,
 * db_write_thread_hdl) will be created. But actually we don't need them at all.
 * Meanwhile we cannot call dlclose when these useless threads are still alive.
 * So the following fake pthread_create is used to prevent them from being created.
 *
 * Note this fake pthread_create may cause some unexpected side effects on another
 * untested version of libdiag.so. If so, futher modification is needed.
 *
 * Tested devices:
 *   Xiaomi Mi 5S            Android 7.1.2
 *   Huawei Nexus 6P         Android 8.0.0
 *   Xiaomi Redmi Note 8     Android 10.0.0
 *   Samsung Galaxy A90 5G   Android 10.0.0
 */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg)
{
	*thread = 1;
	return 0;
}

static int enable_logging_libdiag(int fd, int mode)
{
	const char *LIB_DIAG_PATH[] = {
		"/system/vendor/lib64/libdiag.so",
		"/system/vendor/lib/libdiag.so",
	};

	int ret;
	const char *err;
	void *handle;
	void (*diag_switch_logging)(int, const char *);
	int *diag_fd;
	int *logging_mode;
	unsigned int i;

	/*
	 * "Starting in Android 7.0, the system prevents apps from dynamically linking against
	 * non-NDK libraries, which may cause your app to crash."
	 * Reference: https://developer.android.com/about/versions/nougat/android-7.0-changes#ndk
	 *
	 * Loading private libraries directly is only possible with the root privileges.
	 */
	handle = NULL;
	for (i = 0; i < sizeof(LIB_DIAG_PATH) / sizeof(LIB_DIAG_PATH[0]) && !handle; ++i) {
		handle = dlopen(LIB_DIAG_PATH[i], RTLD_NOW);
		if (!handle)
			LOGE("dlopen %s failed (%s)\n", LIB_DIAG_PATH[i], dlerror());
		else
			LOGI("dlopen %s succeeded\n", LIB_DIAG_PATH[i]);
	}
	if (!handle)
		return -1;

	// Note diag_switch_logging does NOT have a return value in general.
	err = "diag_switch_logging";
	diag_switch_logging = (void (*)(int, const char *)) dlsym(handle, "diag_switch_logging");
	if (!diag_switch_logging)
		goto fail;
	err = "diag_fd/fd";
	diag_fd = (int *) dlsym(handle, "diag_fd");
	if (!diag_fd)
		diag_fd = (int *) dlsym(handle, "fd");
	if (!diag_fd)
		goto fail;
	logging_mode = (int *) dlsym(handle, "logging_mode");

	/*
	 * It seems that calling Diag_LSM_Init here is not necessary.
	 *
	 * When diag_fd is not set, Diag_LSM_Init will try to open
	 * /dev/diag, which will fail since we've already opened one
	 * (errno=EEXIST).
	 *
	 * When diag_fd is set, Diag_LSM_Init will also do nothing
	 * related to our goal.
	 */
	*diag_fd = fd;
	(*diag_switch_logging)(mode, NULL);

	if (logging_mode && *logging_mode != mode) {
		LOGE("diag_switch_logging in libdiag.so failed\n");
		ret = -1;
	} else if (!logging_mode) {
		LOGW("Missing symbol logging_mode in libdiag.so, "
		     "assume diag_switch_logging succeeded\n");
		ret = 0;
	} else {
		ret = 0;
	}

	// We have never created new threads in libdiag.so, so we can close it.
	dlclose(handle);
	return ret;
fail:
	LOGE("Missing symbol %s in libdiag.so\n", err);
	dlclose(handle);
	return -1;
}

static int enable_logging(struct diag_char_handle_t *handle, int mode)
{
	int ret = -1, fd = handle->fd;
	uint16_t remote_dev;
	struct diag_dci_reg_tbl_t dci_reg_tbl;
	struct diag_buffering_mode_t buffering_mode;
	ssize_t arglen;

	// Get remote_dev
	ret = ioctl(fd, DIAG_IOCTL_REMOTE_DEV, &remote_dev);
	if (ret < 0) {
		LOGW("DIAG_IOCTL_REMOTE_DEV ioctl failed (%s)\n", strerror(errno));
		remote_dev = 0;
	}
	handle->remote_dev = remote_dev;

	// Register a DCI client
	dci_reg_tbl.client_id = 0;
	dci_reg_tbl.notification_list = 0;
	dci_reg_tbl.signal_type = SIGPIPE;
	dci_reg_tbl.token = remote_dev ? DCI_MDM_PROC : DCI_LOCAL_PROC;
	ret = ioctl(fd, DIAG_IOCTL_DCI_REG, &dci_reg_tbl);
	if (ret < 0)
		LOGW("DIAG_IOCTL_DCI_REG ioctl failed (%s)\n", strerror(errno));
	handle->dci_client = ret;

	/*
	 * Nexus-6-only logging optimizations
	 * It will fail on other devices (errno=EFAULT), since DIAG_IOCTL_OPTIMIZED_LOGGING is equal to DIAG_IOCTL_PERIPHERAL_BUF_CONFIG.
	 * Reference: https://github.com/MotorolaMobilityLLC/kernel-msm/blob/kitkat-4.4.4-release-victara/drivers/char/diag/diagchar_core.c#L1189
	 */
	(void) ioctl(fd, DIAG_IOCTL_OPTIMIZED_LOGGING, (long) 1);

	// Configure the buffering mode
	buffering_mode.peripheral = PERIPHERAL_MODEM;
	buffering_mode.mode = DIAG_BUFFERING_MODE_STREAMING;
	buffering_mode.high_wm_val = DEFAULT_HIGH_WM_VAL;
	buffering_mode.low_wm_val = DEFAULT_LOW_WM_VAL;
	ret = ioctl(fd, DIAG_IOCTL_PERIPHERAL_BUF_CONFIG, &buffering_mode);
	if (ret < 0)
		LOGW("DIAG_IOCTL_PERIPHERAL_BUF_CONFIG ioctl failed (%s)\n", strerror(errno));

	/*
	 * Enable logging mode:
	 *
	 * DIAG_IOCTL_SWITCH_LOGGING has multiple versions. They require different arguments (which have
	 * different fields and whose lengths are also different). However, it seems there is no way to
	 * directly determine the version of DIAG_IOCTL_SWITCH_LOGGING. So some tricks can not be avoided
	 * here.
	 *
	 * A traditional way is to try one by one. But it can cause undefined behaviour. Specially, when
	 * a new verison of DIAG_IOCTL_SWITCH_LOGGING is introduced, it may not report an error. But some
	 * new fields will be out of bounds. Consequently, it may cause random bugs, which is confusing.
	 *
	 * So a more elegant way is to explicitly probe the length of DIAG_IOCTL_SWITCH_LOGGING's argument.
	 * And the version can be deduced from the length. It is not very precise, but it is enough at least
	 * for now.
	 */
	arglen = probe_ioctl_arglen(handle->fd, DIAG_IOCTL_SWITCH_LOGGING, sizeof(struct diag_logging_mode_param_t));
	switch (arglen) {
	case sizeof(struct diag_logging_mode_param_t): {
		/* Android 10.0 mode
		 * Reference:
		 *   https://android.googlesource.com/kernel/msm.git/+/android-10.0.0_r0.87/drivers/char/diag/diagchar_core.c
		 *   and the disassembly code of libdiag.so
		 */
		struct diag_logging_mode_param_t new_mode;
		struct diag_con_all_param_t con_all;
		con_all.diag_con_all = 0xff /* DIAG_CON_ALL */;
		ret = ioctl(fd, DIAG_IOCTL_QUERY_CON_ALL, &con_all);
		if (ret == 0)
			new_mode.peripheral_mask = con_all.diag_con_all;
		else
			new_mode.peripheral_mask = 0x7f;
		new_mode.req_mode = mode;
		new_mode.pd_mask = 0;
		new_mode.mode_param = 1;
		new_mode.diag_id = 0;
		new_mode.pd_val = 0;
		new_mode.peripheral = -22;
		new_mode.device_mask = (1 << DIAG_MD_LOCAL) | ((remote_dev & 0x3ff) << 1);
		ret = ioctl(fd, DIAG_IOCTL_SWITCH_LOGGING, &new_mode);
		break;
	}
	case sizeof(struct diag_logging_mode_param_v9): {
		/* Android 9.0 mode
		 * Reference: https://android.googlesource.com/kernel/msm.git/+/android-9.0.0_r0.31/drivers/char/diag/diagchar_core.c
		 */
		struct diag_logging_mode_param_v9 new_mode;
		new_mode.req_mode = mode;
		new_mode.mode_param = 0;
		new_mode.pd_mask = 0;
		new_mode.peripheral_mask = DIAG_CON_ALL;
		ret = ioctl(fd, DIAG_IOCTL_SWITCH_LOGGING, &new_mode);
		break;
	}
	case sizeof(struct diag_logging_mode_param_v7): {
		/* Android 7.0 mode
		 * Reference: https://android.googlesource.com/kernel/msm.git/+/android-7.1.0_r0.3/drivers/char/diag/diagchar_core.c
		 */
		struct diag_logging_mode_param_v7 new_mode;
		new_mode.req_mode = mode;
		new_mode.peripheral_mask = DIAG_CON_ALL;
		new_mode.mode_param = 0;
		ret = ioctl(fd, DIAG_IOCTL_SWITCH_LOGGING, &new_mode);
		break;
	}
	case sizeof(int):
		/* Android 6.0 mode
		 * Reference: https://android.googlesource.com/kernel/msm.git/+/android-6.0.0_r0.9/drivers/char/diag/diagchar_core.c
		 */
		ret = ioctl(fd, DIAG_IOCTL_SWITCH_LOGGING, &mode);
		if (ret >= 0)
			break;
		/*
		 * Is it really necessary? It seems that the kernel will simply ignore all the fourth and subsequent
		 * arguments of ioctl. But similar lines do exist in libdiag.so. Why?
		 * Reference: https://android.googlesource.com/kernel/msm.git/+/android-10.0.0_r0.87/fs/ioctl.c#692
		 */
		ret = ioctl(fd, DIAG_IOCTL_SWITCH_LOGGING, &mode, 12, 0, 0, 0, 0);
		break;
	case 0:
		// Yuanjie: the following works for Samsung S5
		ret = ioctl(fd, DIAG_IOCTL_SWITCH_LOGGING, (long) mode);
		if (ret >= 0)
			break;
		// Same question as above: Is it really necessary?
		// Yuanjie: the following is used for Xiaomi RedMi 4
		ret = ioctl(fd, DIAG_IOCTL_SWITCH_LOGGING, (long) mode, 12, 0, 0, 0, 0);
		break;
	default:
		LOGW("ioctl DIAG_IOCTL_SWITCH_LOGGING with arglen=%ld is not supported\n", arglen);
		ret = -8080;
		break;
	}
	if (ret < 0 && ret != -8080)
		LOGE("ioctl DIAG_IOCTL_SWITCH_LOGGING with arglen=%ld is supported, "
		     "but it failed (%s)\n", arglen, strerror(errno));
	else if (ret >= 0)
		LOGI("ioctl DIAG_IOCTL_SWITCH_LOGGING with arglen=%ld succeeded\n", arglen);
	if (ret >= 0)
		return ret;

	// Ultimate approach: use libdiag.so
	ret = enable_logging_libdiag(handle->fd, mode);
	if (ret >= 0)
		LOGI("Using libdiag.so to switch logging succeeded\n");
	return ret;
}

static diag_handle_t diag_char_open(void)
{
	struct diag_char_handle_t *handle;

	handle = malloc(sizeof(struct diag_char_handle_t));
	if (!handle) {
		LOGE("Cannot allocate memory for diag_char_handle_t\n");
		return 0;
	}
	handle->fd = open("/dev/diag", O_RDWR);
	if (handle->fd < 0) {
		LOGE("Cannot open /dev/diag (%s)\n", strerror(errno));
		goto fail;
	}
	if (enable_logging(handle, MEMORY_DEVICE_MODE) < 0)
		goto fail;

	handle->msg_id = handle->msg_num = 0;
	return (diag_handle_t) handle;
fail:
	if (handle->fd >= 0)
		close(handle->fd);
	free(handle);
	return 0;
}

static ssize_t diag_char_read(diag_handle_t handle_, const void **buf, long *stamp)
{
	struct diag_char_handle_t *handle = (struct diag_char_handle_t *) handle_;
	ssize_t len;

	while (handle->msg_id >= handle->msg_num ||
	       handle->msg_type != USER_SPACE_DATA_TYPE) {
		ssize_t ret = read(handle->fd, handle->buf, BUFFER_SIZE);
		if (ret <= 4) {
			LOGE("Failed to read from /dev/diag (%s)\n",
			     ret >= 0 ? "Read incompletely" : strerror(errno));
			handle->msg_id = handle->msg_num = 0;
			return ret >= 0 ? 0 : ret;
		}
		if (handle->msg_type != USER_SPACE_DATA_TYPE)
			continue;
		handle->msg_id = 0;
		handle->stamp = get_posix_timestamp();
		handle->msg_start = handle->buf + 8;
	}

	if ((int32_t) handle->msg_size[0] >= 0) {
		*buf = handle->msg_start + 4;
		len = handle->msg_size[0];
		handle->msg_start += len + 4;
	} else {
		*buf = handle->msg_start + 8;
		len = handle->msg_size[1];
		handle->msg_start += len + 8;
	}
	++handle->msg_id;

	if (stamp && handle->msg_id == handle->msg_num)
		*stamp = handle->stamp;
	else if (stamp)
		*stamp = -1;

	return len;
}

static ssize_t diag_char_write(diag_handle_t handle_, const void *buf, size_t len)
{
	struct diag_char_handle_t *handle = (struct diag_char_handle_t *) handle_;
	ssize_t ret, offset;

	handle->msg_type = USER_SPACE_DATA_TYPE;
	if (handle->remote_dev) {
		handle->msg_dev = -MDM;
		offset = 8;
	} else {
		offset = 4;
	}
	memcpy(handle->buf + offset, buf, len);

	ret = write(handle->fd, handle->buf, len + offset);
	if (ret < 0) {
		LOGE("Failed to write into /dev/diag (%s)\n", strerror(errno));
		goto out;
	}

	/*
	 * Read responses after writting each command.
	 * NOTE: This step MUST EXIST. Without it, some phones cannot collect logs for two reasons:
	 *  (1) Ensure every config commands succeeds (otherwise read() will be blocked)
	 *  (2) Clean up the buffer, thus avoiding pollution of later real cellular logs
	 */
	ret = read(handle->fd, handle->buf, BUFFER_SIZE);
	if (ret <= 0) {
		LOGE("Failed to receive responses from /dev/diag (%s)\n",
		     len == 0 ? "Empty response" : strerror(errno));
		goto out;
	}
	ret = len;

out:
	handle->msg_id = handle->msg_num = 0;
	return ret;
}

static void diag_char_close(diag_handle_t handle_)
{
	struct diag_char_handle_t *handle = (struct diag_char_handle_t *) handle_;

	if (handle->dci_client >= 0) {
		int ret = ioctl(handle->fd, DIAG_IOCTL_DCI_DEINIT, &handle->dci_client);
		if (ret < 0)
			LOGW("DIAG_IOCTL_DCI_DEINIT ioctl failed (%s)\n", strerror(errno));
	}

	close(handle->fd);
	free(handle);
}

const struct diag_interface_t diag_char_interface = {
	.open = &diag_char_open,
	.read = &diag_char_read,
	.write = &diag_char_write,
	.close = &diag_char_close,
};
