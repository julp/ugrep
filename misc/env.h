#ifndef ENV_H

# define ENV_H

extern UFILE *ustdout;
extern UFILE *ustderr;

void env_apply(void);
const char *env_get_inputs_encoding(void);
const char *env_get_stdin_encoding(void);
void env_set_inputs_encoding(const char *);
void env_set_outputs_encoding(const char *);
void env_set_stdin_encoding(const char *);
void env_set_system_encoding(const char *);
void env_init(void);

#endif /* ENV_H */
