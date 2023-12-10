#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
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
#include <config.h>

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

void create_symlink(const char *partition, const char *name) {
    char linkpath[256];
    sprintf(linkpath, "/dev_abm/by-name/%s", name);

    if (symlink(partition, linkpath) < 0) {
        log_and_print("Failed to create symlink: %s -> %s\n", linkpath, partition);
    } else {
#ifdef DEBUG
        log_and_print("Created symlink: %s -> %s\n", linkpath, partition);
#endif
    }
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

void process_partition(const char *devname, const char *partition) {
    char sys_path[276], devpath[276], uevent_path[256], line[256], partname[239];
    FILE *fp;
    int major, minor;

    sprintf(sys_path, "/sys_abm/block/%s/%s/dev", devname, partition);
    sprintf(devpath, "/dev_abm/%s", partition);
    sprintf(uevent_path, "/sys_abm/block/%s/%s/uevent", devname, partition);

    // Create device node
    fp = fopen(sys_path, "r");
    if (!fp) {
        log_and_print("Error opening device file");
        return;
    }

    if (fscanf(fp, "%d:%d", &major, &minor) == 2) {
        create_device_node(devpath, major, minor);
    }
    fclose(fp);

    // Check for partition name and create symlink
    fp = fopen(uevent_path, "r");
    if (!fp) {
        log_and_print("Error opening device file");
        return;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "PARTNAME=%s", partname) == 1) {
            create_symlink(devpath, partname);
            break;
        }
    }
    fclose(fp);
}

void process_device(const char *devname) {
    char part_path[256];
    sprintf(part_path, "/sys_abm/block/%s", devname);
    DIR *dir = opendir(part_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strncmp(entry->d_name, devname, strlen(devname)) == 0) {
            process_partition(devname, entry->d_name);
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

    if (mkdir("/dev_abm/by-name", 0755) < 0) {
        log_and_print(" Failed to mkdir /dev_abm/by-name");
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
    struct boot_entry *entry = malloc(sizeof(struct boot_entry));
    FILE *fp;
    char cmdline[1024];
    char *token;
    char *abm_config_name = NULL;
    int abm_config_name_len;
    char *tmp;

    err = mount("tmpfs", "/dev_abm", "tmpfs", MS_NOSUID, "mode=0755");
    if(err!=0) {
        log_and_print("Failed to mount /dev_abm, we are so done :( err: %d", err);
        goto end;
    }

    err = mknod("/dev_abm/kmsg", S_IFCHR | 0600, makedev(1, 11));
    if(err!=0) {
        log_and_print("Failed to create /dev_abm/kmsg, we are so done :( err: %d", err);
        goto end;
    }

    // As we now have kmsg we can log to it, so allow logging
    can_log = true;

    log_and_print("ABM-init: Welcome to ABM-init!!!");

    err = mount("sysfs", "/sys_abm", "sysfs", MS_NOSUID, "mode=0755");
    if(err!=0) {
        log_and_print("Failed to mount /sys_abm, we are so done err: %d", err);
        goto end;
    }

    err = setup_block_device_nodes();
    if(err!=0) {
        log_and_print("Failed to create block device nodes err: %d", err);
    }

    err = mount("proc", "/proc_abm", "proc", 0, "gid=3009,hidepid=2");
    if(err!=0) {
        log_and_print("Failed to mount /proc_abm, we are so done err: %d", err);
        goto end;
    }

    fp = fopen("/proc_abm/cmdline", "r");
    if (fp == NULL) {
        log_and_print("Error opening /proc_abm/cmdline");
        goto end;
    }

    if (fgets(cmdline, sizeof(cmdline), fp) == NULL) {
        log_and_print("Error reading /proc/cmdline");
        fclose(fp);
        goto end;
    }
    fclose(fp);

    // tmp would point to begining of actual cong name as strstr will provide pointer to letter A of ABM, we add len of key
    tmp = strstr(cmdline, "ABM.config=\"") + strlen("ABM.config=\"");
    if(tmp == NULL) {
        log_and_print("No ABM.config found");
        goto end;
    }

    // Find end of config name by searching for "
    abm_config_name_len = strlen(tmp) - strlen(strstr(tmp, "\""));
    abm_config_name = malloc(abm_config_name_len + 1);
    abm_config_name[abm_config_name_len+1] = "\0";
    memcpy(abm_config_name, tmp, abm_config_name_len);
    log_and_print("ABM config name: %s\n", abm_config_name);

    // Unmount dev sys and proc, else android wount boot
    umount("/dev_abm");
    umount("/sys_abm");
    umount("/proc_abm");

end:
    execl("/init1", "/init1", (char *)NULL);
    return 0;
}
