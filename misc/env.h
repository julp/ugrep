#ifndef ENV_H

# define ENV_H

extern UFILE *ustdout;
extern UFILE *ustderr;

UChar *_(const char *format, ...);
void env_apply(void);
void env_deinit(void);
const char *env_get_inputs_encoding(void);
UNormalizationMode env_get_normalization(void);
const char *env_get_stdin_encoding(void);
void env_init(void);
void env_set_inputs_encoding(const char *);
void env_set_normalization(UNormalizationMode);
void env_set_outputs_encoding(const char *);
void env_set_stdin_encoding(const char *);
void env_set_system_encoding(const char *);

#endif /* ENV_H */
