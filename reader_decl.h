#ifndef READER_DECL_H

# define READER_DECL_H

extern reader_t mm_reader;
extern reader_t stdio_reader;
# ifdef HAVE_ZLIB
extern reader_t gz_reader;
# endif /* HAVE_ZLIB */
# ifdef HAVE_BZIP2
extern reader_t bz2_reader;
# endif /* HAVE_BZIP2 */

#endif /* READER_DECL_H */
