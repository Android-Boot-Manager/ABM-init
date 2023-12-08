#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <stdbool.h>

#ifndef CONFIG_H
#define CONFIG_H

struct boot_entry {
	char *title;
	char *super;
	bool error;
};

int parse_boot_entry_file(struct boot_entry *entry, char *file);

#endif
