#ifndef USERPROG_FD_H
#define USERPROG_FD_H

struct file;

enum fd_type {
    FD_STDIN,
    FD_STDOUT,
    FD_FILE,
};

struct fd_entry {
    enum fd_type type;
    struct file* file;
};

#endif /* USERPROG_FD_H */
