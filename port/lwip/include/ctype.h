/* FlintRTOS lwIP port - freestanding <ctype.h> shim. */
#ifndef FLINT_LWIP_CTYPE_H
#define FLINT_LWIP_CTYPE_H
static inline int isdigit(int c){ return (c>='0')&&(c<='9'); }
static inline int isxdigit(int c){ return ((c>='0')&&(c<='9'))||((c>='a')&&(c<='f'))||((c>='A')&&(c<='F')); }
static inline int isspace(int c){ return (c==' ')||((c>=9)&&(c<=13)); }
static inline int isprint(int c){ return (c>=32)&&(c<127); }
static inline int islower(int c){ return (c>='a')&&(c<='z'); }
static inline int isupper(int c){ return (c>='A')&&(c<='Z'); }
static inline int isalpha(int c){ return islower(c)||isupper(c); }
static inline int isalnum(int c){ return isalpha(c)||isdigit(c); }
static inline int tolower(int c){ return isupper(c)?(c+32):c; }
static inline int toupper(int c){ return islower(c)?(c-32):c; }
#endif
