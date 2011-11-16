#ifndef WIN32_UNISTD_H

# define WIN32_UNISTD_H

# include <io.h>

# define STDOUT_FILENO _fileno(stdout)
# define STDIN_FILENO _fileno(stdin)

# ifndef S_ISREG
#  define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
# endif /* !S_ISREG */

# ifndef S_ISDIR
#  define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
# endif /* !S_ISDIR */

# ifndef S_ISLNK
#  define S_ISLNK(mode) 0
# endif /* !S_ISLNK */

#endif /* !WIN32_UNISTD_H */
