#ifndef OS2_KMOD_H
#define OS2_KMOD_H

#define KMOD_NAME "OS2"
#define DEBUGFS_FILE_NAME "driver"
#define DEBUGFS_FILE_PATH "/sys/kernel/debug/" KMOD_NAME "/" DEBUGFS_FILE_NAME

enum print_flags {
    PRINT_PAGE_INFO = 0b01,
    PRINT_DENTRY_INFO = 0b10
};

#endif