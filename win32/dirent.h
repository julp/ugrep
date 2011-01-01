#ifndef DIRENT_H

# define DIRENT_H

# include <sys/types.h>
# include <sys/stat.h>
# include "ugrep.h"

# define DT_UNKNOWN 0
# define DT_FIFO    1
# define DT_CHR     2
# define DT_DIR     4
# define DT_BLK     6
# define DT_REG     8
# define DT_LNK    10
# define DT_SOCK   12
//# define DT_WHT    14

struct dirent {
    long d_ino;
    off_t d_off;
    unsigned short int d_reclen;
    unsigned char d_type;
    char d_name[_MAX_FNAME + 1];
};

typedef struct _dir DIR;

int chdir(const char *);
int closedir(/*void *, */DIR *);
void dir_deinit(void *);
void *dir_init(void);
int dirfd(DIR *);
int fchdir(void *, int);
DIR *opendir(void *, const char *);
struct dirent *readdir(DIR *);
int win32_close(void *, int);
int win32_fstat(void *, int, struct stat *);
int win32_open(void *, const char *, int);
int win32_open_ex(void *, const char *, int, struct stat *);

#endif /* !DIRENT_H */
