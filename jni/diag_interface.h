#pragma once
#include <stdint.h>

typedef uintptr_t diag_handle_t;

struct diag_interface_t {
	diag_handle_t (*open)(void);
	ssize_t (*write)(diag_handle_t handle, const void *buf, size_t len);
	ssize_t (*read)(diag_handle_t handle, const void **buf, long *stamp);
	void (*close)(diag_handle_t handle);
};

extern const struct diag_interface_t diag_char_interface;
extern const struct diag_interface_t diag_serial_interface;
