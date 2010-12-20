#ifndef WIN32_UNISTD_H

# define WIN32_UNISTD_H

# define STDOUT_FILENO _fileno(stdout)
# define STDIN_FILENO _fileno(stdin)

# ifndef S_ISREG
#  define S_ISREG(mode)  (((mode) & S_IFMT) == S_IFREG)
# endif /* !S_ISREG */

#endif /* !WIN32_UNISTD_H */
