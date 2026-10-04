// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SOPWITH_SERVICES_H
#define SOPWITH_SERVICES_H
#include <stddef.h>
#include <stdint.h>

void *Port_Realloc(void *ptr, size_t size);
void Port_Free(void *ptr);
_Noreturn void Port_Fatal(const char *message);
uint32_t Port_Milliseconds(void);
void Port_IntText(char *out, size_t size, int value);
#endif
