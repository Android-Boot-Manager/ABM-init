#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <stdio.h>
#include <string.h>

#define DEBUG 1

bool can_log = false;

void log_and_print(const char *format, ...) {
    va_list args;
    char buffer[1024];

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);

    // Print to kernel log if possible
    if(can_log) {
        // Open /dev/kmsg for writing
        int fd = open("/dev_abm/kmsg", O_WRONLY | O_APPEND);
        if (fd != -1) {
            // Write the buffer to /dev_abm/kmsg
            write(fd, buffer, strlen(buffer));

            // Close the file descriptor
            close(fd);
        }
    }
    // Log to a file
    FILE *log_file = fopen("/abm_init.log", "a");
    if (log_file != NULL) {
        fprintf(log_file, "%s", buffer);
        fclose(log_file);
    }
    va_end(args);
}

void create_device_node(const char *devpath, int major, int minor) {
    if (mknod(devpath, S_IFBLK | 0666, makedev(major, minor)) < 0) {
        log_and_print("mknod error");
    } else {
#ifdef DEBUG
        log_and_print("Created device node: %s\n", devpath);
#endif
    }
}

void process_device(const char *devname) {
    char sys_path[276], devpath[276], part_path[256];
    int major, minor;
    FILE *fp;

    // Process main block device
    sprintf(sys_path, "/sys_abm/block/%s/dev", devname);
    sprintf(devpath, "/dev_abm/%s", devname);

    fp = fopen(sys_path, "r");
    if (!fp) {
        log_and_print("Error opening device file");
        return;
    }

    if (fscanf(fp, "%d:%d", &major, &minor) == 2) {
        create_device_node(devpath, major, minor);
    } else {
        log_and_print("Error reading major and minor numbers, %d:%d", major, minor);
    }
    fclose(fp);

    // Process partitions
    sprintf(part_path, "/sys_abm/block/%s", devname);
    DIR *dir = opendir(part_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strncmp(entry->d_name, devname, strlen(devname)) == 0) {
            sprintf(sys_path, "/sys_abm/block/%s/%s/dev", devname, entry->d_name);
            sprintf(devpath, "/dev_abm/%s", entry->d_name);

            fp = fopen(sys_path, "r");
            if (!fp) continue;

            if (fscanf(fp, "%d:%d", &major, &minor) == 2) {
                create_device_node(devpath, major, minor);
            }
            fclose(fp);
        }
    }
    closedir(dir);
}
int setup_block_device_nodes()
{
    struct dirent *dp;
    DIR *dfd;

    if ((dfd = opendir("/sys_abm/block")) == NULL) {
        log_and_print("Can't open /sys_abm/block");
        return 1;
    }

    while ((dp = readdir(dfd)) != NULL) {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
            continue;

        process_device(dp->d_name);
    }

    closedir(dfd);
    return 0;
}

int main(void)
{
    int err = 0;

    err = mount("tmpfs", "/dev_abm", "tmpfs", MS_NOSUID, "mode=0755");
    if(err!=0) {
        log_and_print("Failed to mount /dev_abm, we are so done :(");
        goto end;
    }

    err = mknod("/dev_abm/kmsg", S_IFCHR | 0600, makedev(1, 11));
    if(err!=0) {
        log_and_print("Failed to create /dev_abm/kmsg, we are so done :(");
        goto end;
    }

    // As we now have kmsg we can log to it, so allow logging
    can_log = true;

    log_and_print("ABM-init: Welcome to ABM-init!!!");

    err = mount("sysfs", "/sys_abm", "sysfs", MS_NOSUID, "mode=0755");
    if(err!=0) {
        log_and_print("Failed to mount /sys_abm, we are so done");
        goto end;
    }

    err = setup_block_device_nodes();
    if(err!=0) {
        log_and_print("Failed to create block device nodes");
    }

end:
    execl("/init1", "/init1", (char *)NULL);
    return 0;
}
