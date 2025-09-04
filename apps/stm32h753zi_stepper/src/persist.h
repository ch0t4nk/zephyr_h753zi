#pragma once

#include <stddef.h>

int persistence_init(void);
int persistence_write_value(const char *path, const void *data, size_t len);
int persistence_read_value(const char *path, void *data, size_t len, size_t *out_len);
void persistence_status_print(void);
