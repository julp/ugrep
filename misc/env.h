#ifndef ENV_H

# define ENV_H

extern UFILE *ustdout;
extern UFILE *ustderr;

enum {
    UNIT_CODEPOINT,
    UNIT_GRAPHEME
};

void env_apply(void);
void env_close(void);
const char *env_get_inputs_encoding(void);
UNormalizationMode env_get_normalization(void);
const char *env_get_stdin_encoding(void);
int env_get_unit(void);
void env_init(void);
# ifdef DEBUG
#  define env_register_resource(ptr, dtor) \
    _env_register_resource((ptr), (dtor), ubasename(__FILE__), __LINE__)
void _env_register_resource(void *, func_dtor_t, const char *, int) NONNULL();
# else
void env_register_resource(void *, func_dtor_t) NONNULL();
# endif /* DEBUG */
void env_set_inputs_encoding(const char *);
void env_set_normalization(UNormalizationMode);
void env_set_outputs_encoding(const char *);
void env_set_stdin_encoding(const char *);
void env_set_system_encoding(const char *);
void env_set_unit(int);

#endif /* ENV_H */
