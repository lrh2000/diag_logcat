#pragma once
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define  LOGE(...)  printf("[ERROR] " __VA_ARGS__)
#define  LOGW(...)  printf("[WARN ] " __VA_ARGS__)
#define  LOGD(...)  printf("[DEBUG] " __VA_ARGS__)
#define  LOGI(...)  printf("[INFO ] " __VA_ARGS__)

static inline uint64_t get_posix_timestamp(void)
{
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	return tp.tv_sec * 1000000000ull + tp.tv_nsec;
}
