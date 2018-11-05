#ifndef ATMEL_TRNG_H
#define ATMEL_TRNG_H

#ifdef CONFIG_ATMEL_TRNG
void atmel_trng_init(void);
void atmel_trng_remove(void);
#else
static inline void atmel_trng_init(void) {}
static inline void atmel_trng_remove(void) {}
#endif

#endif
