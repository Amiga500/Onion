#ifndef REDAMP_UTF8_TEXT_H
#define REDAMP_UTF8_TEXT_H
#include <stddef.h>
#include <stdint.h>
/* Strict UTF-8: 0=end, -1=malformed (consumes one byte), 1=scalar. */
int text_next(const char **p, uint32_t *cp);
int text_count(const char *s);
void text_erase(char *s);
int text_append(char *s,size_t cap,int max_chars,const char *key);
int text_casecmp(const char *a,const char *b);
int text_contains(const char *text,const char *query);
#endif
