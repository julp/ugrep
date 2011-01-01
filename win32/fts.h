#ifndef WIN32_FTS_H

# define WIN32_FTS_H

# include <sys/types.h>
# include <sys/stat.h>

# define FTS_D       1
# define FTS_DC      2
# define FTS_DEFAULT 3
# define FTS_DNR     4
# define FTS_DOT     5
# define FTS_DP      6
# define FTS_ERR     7
# define FTS_F       8
# define FTS_INIT    9
# define FTS_NS     10
# define FTS_NSOK   11
# define FTS_SL     12
# define FTS_SLNONE 13
# define FTS_W      14

# define FTS_COMFOLLOW  0x001
# define FTS_LOGICAL    0x002
# define FTS_NOCHDIR    0x004
# define FTS_NOSTAT     0x008
# define FTS_PHYSICAL   0x010
# define FTS_SEEDOT     0x020
# define FTS_XDEV       0x040
# ifdef S_IFWHT
#  define FTS_WHITEOUT   0x080
#  define FTS_OPTIONMASK 0x0ff
# else
#  define FTS_OPTIONMASK 0x05f
# endif
# define FTS_NAMEONLY   0x100
# define FTS_STOP       0x200

# define FTS_AGAIN   1
# define FTS_FOLLOW  2
# define FTS_NOINSTR 3
# define FTS_SKIP    4

typedef short nlink_t;

typedef struct _fts FTS;

typedef struct _ftsent {
    struct _ftsent *fts_cycle;
    struct _ftsent *fts_parent;
    struct _ftsent *fts_link;
    long long fts_number;
    void *fts_pointer;
    char *fts_accpath;
    char *fts_path;
    int fts_errno;
    int fts_symfd;
    size_t fts_pathlen;
    size_t fts_namelen;
    ino_t fts_ino;
    dev_t fts_dev;
    nlink_t fts_nlink;
    long fts_level;
    int fts_info;
    unsigned fts_flags;
    int fts_instr;
    struct stat *fts_statp;
    char *fts_name;
    void *fts_priv;
    FTS *fts_fts;
} FTSENT;

typedef int (*fts_cmp_func)(const FTSENT * const *, const FTSENT * const *);

FTS *fts_open(char * const *path_argv, int options, fts_cmp_func);
FTSENT *fts_read(FTS *);
//FTSENT *fts_children(FTS *, int);
int fts_set(FTS *, FTSENT *, int);
void fts_set_clientptr(FTS *, void *);
void *fts_get_clientptr(FTS *);
//FTS *fts_get_stream(FTSENT *);
int fts_close(FTS *);

#endif /* WIN32_FTS_H */
