#ifndef KERNEL_STRING_H
#define KERNEL_STRING_H

#include <stddef.h>

int strlen(const char *str);
char *strcpy(char *dest, const char *src);
int strncmp(const char *a, const char *b, unsigned int n);
int strcmp(const char *a, const char *b);

#endif
