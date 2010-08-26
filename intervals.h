#ifndef INTERVALS_H
# define INTERVALS_H

typedef struct {
    int32_t lower_limit;
    int32_t upper_limit;
} interval_t;

UBool interval_add(slist_t *, int32_t, int32_t, int32_t);
slist_t *intervals_new();

#endif /* INTERVALS_H */
