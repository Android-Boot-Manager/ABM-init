// SPDX-License-Identifier: GPL-2.0+
// © 2019 Mis012
// © 2020-2021, 2023 Luka Panio

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include "config.h"
#include "main.h"
#include <dirent.h>

#define ENTRIES_DIR "/abm_userdata/db/entries"

static int config_parse_option(char **_dest, const char *option, const char *buffer) {
	char *temp = strstr(buffer, option);
	if(!temp)
		return -1;

	temp += strlen(option);
	while (*temp == ' ')
		temp++;
	char *newline = strchr(temp, '\n');
	if(newline)
		*newline = '\0';
	char *dest = malloc(strlen(temp) + 1);
	if(!dest)
		return 1;
	strcpy(dest, temp);
	*_dest = dest;

	//restore the buffer
	*newline = '\n';

	return 0;
}

int parse_boot_entry_file(struct boot_entry *entry, char *file) {
	int ret;
    FILE *fp;
	unsigned char *buf;
	char *path = malloc(strlen(file) + strlen(ENTRIES_DIR) + strlen("/") + 1);
	strcpy(path, ENTRIES_DIR);
	strcat(path, "/");
	strcat(path, file);

	fp = fopen (path, "r");
	if(fp==NULL) {
		return 1;
	}
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);  /* same as rewind(f); */

    buf = malloc(fsize + 1);

    fread(buf, 1, fsize, fp);

	fclose(fp);

	buf[fsize] = '\0';

	ret = config_parse_option(&entry->title, "title", (const char *)buf);
	if(ret < 0) {
		log_and_print("SYNTAX ERROR: entry \"%s\" - no option 'title'\n", path);
		free(buf);
		return ret;
	}

	ret = config_parse_option(&entry->super, "super", (const char *)buf);
	if(ret < 0) {
		log_and_print("SYNTAX ERROR: entry \"%s\" - no option 'super'\n", path);
		free(buf);
		return ret;
	}

	free(buf);

	entry->error = false;

	return 0;
}
