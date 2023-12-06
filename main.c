#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <stdio.h>
#include <string.h>

bool can_log = false;

void log_and_print(const char *format, ...) {
    va_list args;
    char buffer[1024];

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);

    // Print to kernel log if possible
    if(can_log) {
        // Open /dev/kmsg for writing
        int fd = open("/dev/kmsg", O_WRONLY | O_APPEND);
        if (fd != -1) {
            // Write the buffer to /dev/kmsg
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

int main(void)
{
    int err = 0;

    err = mount("tmpfs", "/dev", "tmpfs", MS_NOSUID, "mode=0755");
    if(err!=0) {
        log_and_print("Failed to mount /dev, we are so done :(");
    }

    err = mknod("/dev/kmsg", S_IFCHR | 0600, makedev(1, 11));
    if(err!=0) {
        log_and_print("Failed to create /dev/kmsg, we are so done :(");
    }

    // As we now have kmsg we can log to it, so allow logging
    can_log = true;

    log_and_print("ABM-init: Welcome to ABM-init!!!");
    execl("/init1", "/init1", (char *)NULL);
    return 0;
}
