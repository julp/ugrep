#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ugrep.h"

//#define _open(path, omode, x) win32_open(sp->fts_priv, path, omode)
//#define _close(fd) win32_close(sp->fts_priv, fd)
#define d_namlen d_reclen
#define lstat(p, b) 1

/*
stat : http://msdn.microsoft.com/fr-fr/library/14h5k7ff%28VS.80%29.aspx
http://blogs.msdn.com/b/csliu/archive/2009/03/20/windows-unix-and-ansi-c-api-comparison.aspx
*/

#include <errno.h>

#ifndef DIRECTORY_SEPARATOR
# define DIRECTORY_SEPARATOR '\\'
#endif /* !DIRECTORY_SEPARATOR */

#define	BCHILD 1
#define	BNAMES 2
#define	BREAD  3

#define FTS_DONTCHDIR 0x01
#define FTS_SYMFOLLOW 0x02
#define FTS_ISW       0x04

#define FTS_ROOTPARENTLEVEL -1
#define FTS_ROOTLEVEL        0

#define	ISDOT(a) \
    ('.' == a[0] && ('\0' == a[1] || ('.' == a[1] && '\0' == a[2])))

#define NAPPEND(p) \
    (p->fts_path[p->fts_pathlen - 1] == DIRECTORY_SEPARATOR ? p->fts_pathlen - 1 : p->fts_pathlen)

#define	FCHDIR(ftsp, fd) \
    (!(ftsp->fts_options & FTS_NOCHDIR) && fchdir(ftsp->fts_priv, fd))

struct _fts {
    FTSENT *fts_cur;
    FTSENT *fts_child;
    FTSENT **fts_array;
    char *fts_path;
    size_t fts_path_allocated;
    size_t fts_nitems;
    fts_cmp_func fts_compar;
    int fts_options;
    int fts_rfd;
    void *fts_clientptr;
    dev_t fts_dev;
    void *fts_priv;
};

static void *reallocf(void *ptr, size_t size)
{
	void *nptr;

	nptr = realloc(ptr, size);
	if (NULL == nptr && NULL != ptr && 0 != size) {
		free(ptr);
    }

	return (nptr);
}

static int fts_palloc(FTS *ftsp, size_t len)
{
    ftsp->fts_path_allocated += len;
    ftsp->fts_path = reallocf(ftsp->fts_path, ftsp->fts_path_allocated);

    return (NULL != ftsp->fts_path);
}

static FTSENT *fts_alloc(FTS *ftsp, char *name, size_t name_length)
{
    FTSENT *p;
    size_t len;

    struct ftsent_withstat {
        FTSENT ent;
        struct stat statbuf;
    };

    if (ftsp->fts_options & FTS_NOSTAT) {
        len = sizeof(*p) + name_length + 1;
    } else {
        len = sizeof(struct ftsent_withstat) + name_length + 1;
    }

    if (NULL == (p = malloc(len))) {
        return NULL;
    }

    if (ftsp->fts_options & FTS_NOSTAT) {
        p->fts_name = (char *)(p + 1);
        p->fts_statp = NULL;
    } else {
        p->fts_name = (char *)((struct ftsent_withstat *)p + 1);
        p->fts_statp = &((struct ftsent_withstat *)p)->statbuf;
    }

    memcpy(p->fts_name, name, name_length);
    p->fts_name[name_length] = '\0';
    p->fts_namelen = name_length;
    p->fts_path = ftsp->fts_path;
    p->fts_errno = 0;
    p->fts_flags = 0;
    p->fts_instr = FTS_NOINSTR;
    p->fts_number = 0;
    p->fts_pointer = NULL;
    p->fts_fts = ftsp;

    return p;
}

static void fts_lfree(FTSENT *head)
{
    FTSENT *p;

    while (NULL != (p = head)) {
        head = head->fts_link;
        free(p);
    }
}

#define ADJUST(p) \
    do {                                                                          \
        if ((p)->fts_accpath != (p)->fts_name) {                                  \
            (p)->fts_accpath = (char *)addr + ((p)->fts_accpath - (p)->fts_path); \
        }                                                                         \
        (p)->fts_path = addr;                                                     \
    } while (0)

static void fts_padjust(FTS *ftsp, FTSENT *head)
{
    FTSENT *p;
    char *addr = ftsp->fts_path;

    for (p = ftsp->fts_child; p; p = p->fts_link) {
        ADJUST(p);
    }

    for (p = head; p->fts_level >= FTS_ROOTLEVEL; ) {
        ADJUST(p);
        p = p->fts_link ? p->fts_link : p->fts_parent;
    }
}

static int fts_stat(FTS *ftsp, FTSENT *p, int follow)
{
    FTSENT *t;
    dev_t dev;
    ino_t ino;
    struct stat *sbp, sb;

    sbp = ftsp->fts_options & FTS_NOSTAT ? &sb : p->fts_statp;

#ifdef FTS_WHITEOUT
    if (p->fts_flags & FTS_ISW) {
        if (sbp != &sb) {
            memset(sbp, '\0', sizeof(*sbp));
            sbp->st_mode = S_IFWHT;
        }
        return FTS_W;
    }
#endif

    if ((ftsp->fts_options & FTS_LOGICAL) || follow) {
        if (stat(p->fts_accpath, sbp)) {
            p->fts_errno = errno;
            memset(sbp, 0, sizeof(*sbp));
            return FTS_NS;
        }
    }

    if (S_ISDIR(sbp->st_mode)) {
        dev = p->fts_dev = sbp->st_dev;
        ino = p->fts_ino = sbp->st_ino;
        p->fts_nlink = sbp->st_nlink;

        if (ISDOT(p->fts_name)) {
            return FTS_DOT;
        }

        /*for (t = p->fts_parent; t->fts_level >= FTS_ROOTLEVEL; t = t->fts_parent) {
            if (ino == t->fts_ino && dev == t->fts_dev) {
                p->fts_cycle = t;
                return FTS_DC;
            }
        }*/
        return FTS_D;
    }
    if (S_ISLNK(sbp->st_mode)) {
        return FTS_SL;
    }
    if (S_ISREG(sbp->st_mode)) {
        return FTS_F;
    }
    return FTS_DEFAULT;
}

static FTSENT *fts_sort(FTS *ftsp, FTSENT *head, size_t nitems)
{
    FTSENT **ap, *p;

    if (nitems > ftsp->fts_nitems) {
        ftsp->fts_nitems = nitems + 40;
        if ((ftsp->fts_array = reallocf(ftsp->fts_array, ftsp->fts_nitems * sizeof(FTSENT *))) == NULL) {
            ftsp->fts_nitems = 0;
            return head;
        }
    }
    for (ap = ftsp->fts_array, p = head; p; p = p->fts_link) {
        *ap++ = p;
    }
    qsort(ftsp->fts_array, nitems, sizeof(FTSENT *), ftsp->fts_compar);
    for (head = *(ap = ftsp->fts_array); --nitems; ++ap) {
        ap[0]->fts_link = ap[1];
    }
    ap[0]->fts_link = NULL;
    return head;
}

static int fts_safe_changedir(FTS *ftsp, FTSENT *p, int fd, char *path)
{
	int ret, oerrno, newfd;
	struct stat sb;

	newfd = fd;
	if (ftsp->fts_options & FTS_NOCHDIR) {
		return 0;
    }
	if (fd < 0 && (newfd = win32_open_ex(ftsp->fts_priv, path, O_RDONLY, &sb)) == -1) {
		return -1;
    }
	if (win32_fstat(ftsp->fts_priv, newfd, &sb)) {
		ret = -1;
		goto bail;
	}
	if (p->fts_dev != sb.st_dev || p->fts_ino != sb.st_ino) {
		errno = ENOENT;
		ret = -1;
		goto bail;
	}
	ret = fchdir(ftsp->fts_priv, newfd);
bail:
	oerrno = errno;
	if (fd < 0) {
		win32_close(ftsp->fts_priv, newfd);
    }
	errno = oerrno;
	return ret;
}

static void fts_load(FTS *ftsp, FTSENT *p)
{
    size_t len;
    char *cp;

    len = p->fts_pathlen = p->fts_namelen;
    memmove(ftsp->fts_path, p->fts_name, len + 1);
    if ((cp = strrchr(p->fts_name, DIRECTORY_SEPARATOR)) && (cp != p->fts_name || cp[1])) {
        len = strlen(++cp);
        memmove(p->fts_name, cp, len + 1);
        p->fts_namelen = len;
    }
    p->fts_accpath = p->fts_path = ftsp->fts_path;
    ftsp->fts_dev = p->fts_dev;
}

static FTSENT *fts_build(FTS *ftsp, int type)
{
    FTSENT *p, *head;
    FTSENT *cur, *tail;
    struct dirent *dp;
    DIR *dirp;
    void *oldaddr;
    char *cp;
    int cderrno, descend, saved_errno, nostat, doadjust;
    long level;
    short nlinks;
    size_t dnamlen, len, maxlen, nitems;

    cur = ftsp->fts_cur;
    if ((dirp = opendir(ftsp->fts_priv, cur->fts_accpath)) == NULL) {
        if (type == BREAD) {
            cur->fts_info = FTS_DNR;
            cur->fts_errno = errno;
        }
        return NULL;
    }

    if (type == BNAMES) {
        nlinks = 0;
        nostat = 0;
    } else if ((ftsp->fts_options & FTS_NOSTAT) && (ftsp->fts_options & FTS_PHYSICAL)) {
        nlinks = cur->fts_nlink - ((ftsp->fts_options & FTS_SEEDOT) ? 0 : 2);
        nostat = 1;
    } else {
        nlinks = -1;
        nostat = 0;
    }

    cderrno = 0;
    if (nlinks || type == BREAD) {
        if (fts_safe_changedir(ftsp, cur, dirfd(dirp), NULL)) {
            if (nlinks && type == BREAD) {
                cur->fts_errno = errno;
            }
            cur->fts_flags |= FTS_DONTCHDIR;
            descend = 0;
            cderrno = errno;
        } else {
            descend = 1;
        }
    } else {
        descend = 0;
    }

    len = NAPPEND(cur);
    if (ftsp->fts_options & FTS_NOCHDIR) {
        cp = ftsp->fts_path + len;
        *cp++ = DIRECTORY_SEPARATOR;
    } else {
        cp = NULL;
    }
    len++;
    maxlen = ftsp->fts_path_allocated - len;

    level = cur->fts_level + 1;

    doadjust = 0;
    for (head = tail = NULL, nitems = 0; dirp && (dp = readdir(dirp)); ) {
        dnamlen = dp->d_namlen;
        if (!(ftsp->fts_options & FTS_SEEDOT) && ISDOT(dp->d_name)) {
            continue;
        }

        if ((p = fts_alloc(ftsp, dp->d_name, dnamlen)) == NULL) {
            goto mem1;
        }
        if (dnamlen >= maxlen) {
            oldaddr = ftsp->fts_path;
            if (fts_palloc(ftsp, dnamlen + len + 1)) {
mem1:               saved_errno = errno;
                if (p) {
                    free(p);
                }
                fts_lfree(head);
                closedir(/*ftsp->fts_priv, */dirp);
                cur->fts_info = FTS_ERR;
                ftsp->fts_options |= FTS_STOP;
                errno = saved_errno;
                return NULL;
            }
            if (oldaddr != ftsp->fts_path) {
                doadjust = 1;
                if (ftsp->fts_options & FTS_NOCHDIR) {
                    cp = ftsp->fts_path + len;
                }
            }
            maxlen = ftsp->fts_path_allocated - len;
        }

        p->fts_level = level;
        p->fts_parent = ftsp->fts_cur;
        p->fts_pathlen = len + dnamlen;

#ifdef FTS_WHITEOUT
        if (dp->d_type == DT_WHT)
            p->fts_flags |= FTS_ISW;
#endif

        if (cderrno) {
            if (nlinks) {
                p->fts_info = FTS_NS;
                p->fts_errno = cderrno;
            } else {
                p->fts_info = FTS_NSOK;
            }
            p->fts_accpath = cur->fts_accpath;
        } else if (nlinks == 0
#ifdef DT_DIR
            || (nostat &&
            dp->d_type != DT_DIR && dp->d_type != DT_UNKNOWN)
#endif
            ) {
            p->fts_accpath = (ftsp->fts_options & FTS_NOCHDIR) ? p->fts_path : p->fts_name;
            p->fts_info = FTS_NSOK;
        } else {
            if (ftsp->fts_options & FTS_NOCHDIR) {
                p->fts_accpath = p->fts_path;
                memmove(cp, p->fts_name, p->fts_namelen + 1);
            } else {
                p->fts_accpath = p->fts_name;
            }
            p->fts_info = fts_stat(ftsp, p, 0);

            if (nlinks > 0 && (p->fts_info == FTS_D || p->fts_info == FTS_DC || p->fts_info == FTS_DOT)) {
                --nlinks;
            }
        }

        p->fts_link = NULL;
        if (head == NULL) {
            head = tail = p;
        } else {
            tail->fts_link = p;
            tail = p;
        }
        ++nitems;
    }
    closedir(/*ftsp->fts_priv, */dirp);

    if (doadjust) {
        fts_padjust(ftsp, head);
    }

    if (ftsp->fts_options & FTS_NOCHDIR) {
        ftsp->fts_path[cur->fts_pathlen] = '\0';
    }

    if (descend && (type == BCHILD || !nitems) && (cur->fts_level == FTS_ROOTLEVEL ? FCHDIR(ftsp, ftsp->fts_rfd) : fts_safe_changedir(ftsp, cur->fts_parent, -1, ".."))) {
        cur->fts_info = FTS_ERR;
        ftsp->fts_options |= FTS_STOP;
        return NULL;
    }

    if (!nitems) {
        if (type == BREAD) {
            cur->fts_info = FTS_DP;
        }
        return NULL;
    }

    if (ftsp->fts_compar && nitems > 1) {
        head = fts_sort(ftsp, head, nitems);
    }
    return head;
}

FTS *fts_open(char * const *argv, int options, fts_cmp_func compar)
{
    FTS *ftsp;
    FTSENT *parent, *root;
    int nitems, len;

    if (options & ~FTS_OPTIONMASK) {
        errno = EINVAL;
        return NULL;
    }

    if (NULL == (ftsp = malloc(sizeof(*ftsp)))) {
        return NULL;
    }
    memset(ftsp, 0, sizeof(*ftsp));
    ftsp->fts_options = options;
    ftsp->fts_compar = compar;
    ftsp->fts_priv = dir_init();

    if (ftsp->fts_options & FTS_LOGICAL) {
        ftsp->fts_options |= FTS_NOCHDIR;
    }

    if (!fts_palloc(ftsp, MAXPATHLEN)) {
        goto mem1;
    }

    if (NULL == (parent = fts_alloc(ftsp, "", 0))) {
        goto mem2;
    }
    parent->fts_level = FTS_ROOTPARENTLEVEL;

    for (root = NULL, nitems = 0; NULL != *argv; ++argv, ++nitems) {
        FTSENT *p, *tmp;

        if (0 == (len = strlen(*argv))) {
            errno = ENOENT;
            goto mem3;
        }

        p = fts_alloc(ftsp, *argv, len);
        p->fts_level = FTS_ROOTLEVEL;
        p->fts_parent = parent;
        p->fts_accpath = p->fts_name;
        p->fts_info = fts_stat(ftsp, p, options & FTS_COMFOLLOW);

        if (p->fts_info == FTS_DOT)
            p->fts_info = FTS_D;

        if (NULL != compar) {
            p->fts_link = root;
            root = p;
        } else {
            p->fts_link = NULL;
            if (root == NULL)
                tmp = root = p;
            else {
                tmp->fts_link = p;
                tmp = p;
            }
        }
    }
    if (NULL != compar && nitems > 1) {
        root = fts_sort(ftsp, root, nitems);
    }

    if (NULL == (ftsp->fts_cur = fts_alloc(ftsp, "", 0))) {
        goto mem3;
    }
    ftsp->fts_cur->fts_link = root;
    ftsp->fts_cur->fts_info = FTS_INIT;

    if (!(ftsp->fts_options & FTS_NOCHDIR) && (ftsp->fts_rfd = win32_open(ftsp->fts_priv, ".", O_RDONLY)) == -1) {
        ftsp->fts_options |= FTS_NOCHDIR;
    }

    return ftsp;

mem3:
    fts_lfree(root);
	free(parent);
mem2:
    free(ftsp->fts_path);
mem1:
    free(ftsp);
    return NULL;
}

FTSENT *fts_read(FTS *ftsp)
{
    FTSENT *p, *tmp;
    int instr;
    char *t;
    int saved_errno;

    if (ftsp->fts_cur == NULL || (ftsp->fts_options & FTS_STOP)) {
        return NULL;
    }

    p = ftsp->fts_cur;

    instr = p->fts_instr;
    p->fts_instr = FTS_NOINSTR;

    if (instr == FTS_AGAIN) {
        p->fts_info = fts_stat(ftsp, p, 0);
        return p;
    }

    if (instr == FTS_FOLLOW && (p->fts_info == FTS_SL || p->fts_info == FTS_SLNONE)) {
        p->fts_info = fts_stat(ftsp, p, 1);
        if (p->fts_info == FTS_D && !(ftsp->fts_options & FTS_NOCHDIR)) {
            if ((p->fts_symfd = win32_open(ftsp->fts_priv, ".", O_RDONLY)) == -1) {
                p->fts_errno = errno;
                p->fts_info = FTS_ERR;
            } else {
                p->fts_flags |= FTS_SYMFOLLOW;
            }
        }
        return p;
    }

    if (p->fts_info == FTS_D) {
        if (instr == FTS_SKIP || ((ftsp->fts_options & FTS_XDEV) && p->fts_dev != ftsp->fts_dev)) {
            if (p->fts_flags & FTS_SYMFOLLOW) {
                win32_close(ftsp->fts_priv, p->fts_symfd);
            }
            if (ftsp->fts_child) {
                fts_lfree(ftsp->fts_child);
                ftsp->fts_child = NULL;
            }
            p->fts_info = FTS_DP;
            return p;
        }

        if (ftsp->fts_child != NULL && (ftsp->fts_options & FTS_NAMEONLY)) {
            ftsp->fts_options &= ~FTS_NAMEONLY;
            fts_lfree(ftsp->fts_child);
            ftsp->fts_child = NULL;
        }

        if (ftsp->fts_child != NULL) {
            if (fts_safe_changedir(ftsp, p, -1, p->fts_accpath)) {
                p->fts_errno = errno;
                p->fts_flags |= FTS_DONTCHDIR;
                for (p = ftsp->fts_child; p != NULL; p = p->fts_link) {
                    p->fts_accpath = p->fts_parent->fts_accpath;
                }
            }
        } else if ((ftsp->fts_child = fts_build(ftsp, BREAD)) == NULL) {
            if (ftsp->fts_options & FTS_STOP) {
                return NULL;
            }
            return p;
        }
        p = ftsp->fts_child;
        ftsp->fts_child = NULL;
        goto name;
    }

next:
    tmp = p;
    if ((p = p->fts_link) != NULL) {
        free(tmp);

        if (p->fts_level == FTS_ROOTLEVEL) {
            if (FCHDIR(ftsp, ftsp->fts_rfd)) {
                ftsp->fts_options |= FTS_STOP;
                return NULL;
            }
            fts_load(ftsp, p);
            return (ftsp->fts_cur = p);
        }

        if (p->fts_instr == FTS_SKIP) {
            goto next;
        }
        if (p->fts_instr == FTS_FOLLOW) {
            p->fts_info = fts_stat(ftsp, p, 1);
            if (p->fts_info == FTS_D && !(ftsp->fts_options & FTS_NOCHDIR)) {
                if ((p->fts_symfd = win32_open(ftsp->fts_priv, ".", O_RDONLY)) == -1) {
                    p->fts_errno = errno;
                    p->fts_info = FTS_ERR;
                } else {
                    p->fts_flags |= FTS_SYMFOLLOW;
                }
            }
            p->fts_instr = FTS_NOINSTR;
        }

name:
        t = ftsp->fts_path + NAPPEND(p->fts_parent);
        *t++ = DIRECTORY_SEPARATOR;
        memmove(t, p->fts_name, p->fts_namelen + 1);
        return (ftsp->fts_cur = p);
    }

    p = tmp->fts_parent;
    free(tmp);

    if (p->fts_level == FTS_ROOTPARENTLEVEL) {
        free(p);
        errno = 0;
        return (ftsp->fts_cur = NULL);
    }

    ftsp->fts_path[p->fts_pathlen] = '\0';

    if (p->fts_level == FTS_ROOTLEVEL) {
        if (FCHDIR(ftsp, ftsp->fts_rfd)) {
            ftsp->fts_options |= FTS_STOP;
            return NULL;
        }
    } else if (p->fts_flags & FTS_SYMFOLLOW) {
        if (FCHDIR(ftsp, p->fts_symfd)) {
            saved_errno = errno;
            win32_close(ftsp->fts_priv, p->fts_symfd);
            errno = saved_errno;
            ftsp->fts_options |= FTS_STOP;
            return NULL;
        }
        win32_close(ftsp->fts_priv, p->fts_symfd);
    } else if (!(p->fts_flags & FTS_DONTCHDIR) && fts_safe_changedir(ftsp, p->fts_parent, -1, "..")) {
        ftsp->fts_options |= FTS_STOP;
        return NULL;
    }
    p->fts_info = p->fts_errno ? FTS_ERR : FTS_DP;
    return (ftsp->fts_cur = p);
}

FTSENT *fts_children(FTS *ftsp, int options)
{
    return NULL;
}

int fts_set(FTS *UNUSED(ftsp), FTSENT *f, int instr)
{
    if (instr != 0 && instr != FTS_AGAIN && instr != FTS_FOLLOW && instr != FTS_NOINSTR && instr != FTS_SKIP) {
        errno = EINVAL;
        return 1;
    }
    f->fts_instr = instr;
    return 0;
}

void fts_set_clientptr(FTS *ftsp, void *data)
{
    ftsp->fts_clientptr = data;
}

void *fts_get_clientptr(FTS *ftsp)
{
    return ftsp->fts_clientptr;
}

/*FTS *fts_get_stream(FTSENT *f)
{
    //
}*/

int fts_close(FTS *ftsp)
{
    FTSENT *freep, *p;
    int saved_errno;

    if (ftsp->fts_cur) {
        for (p = ftsp->fts_cur; p->fts_level >= FTS_ROOTLEVEL;) {
            freep = p;
            p = p->fts_link != NULL ? p->fts_link : p->fts_parent;
            free(freep);
        }
        free(p);
    }

    if (ftsp->fts_child) {
        fts_lfree(ftsp->fts_child);
    }
    if (ftsp->fts_array) {
        free(ftsp->fts_array);
    }
    free(ftsp->fts_path);

    if (!(ftsp->fts_options & FTS_NOCHDIR)) {
        saved_errno = fchdir(ftsp->fts_priv, ftsp->fts_rfd) ? errno : 0;
        win32_close(ftsp->fts_priv, ftsp->fts_rfd);

        if (saved_errno != 0) {
            free(ftsp);
            errno = saved_errno;
            return -1;
        }
    }

    dir_deinit(ftsp->fts_priv);

    free(ftsp);

    return 0;
}
