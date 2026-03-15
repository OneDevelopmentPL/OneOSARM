/* OneOS-ARM String Utilities */

#ifndef STRING_H
#define STRING_H

/* Define basic types for bare-metal */
typedef unsigned int size_t;

/* String comparison */
int strcmp(const char *s1, const char *s2);

/* String length */
size_t strlen(const char *s);

/* Memory copy */
void *memcpy(void *dest, const void *src, size_t n);

/* Memory set */
void *memset(void *s, int c, size_t n);

#endif
