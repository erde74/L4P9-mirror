#include <u.h>
#include <libc.h>

#define NULL 0

/*
 *  strcspn	span the complement of a string 
 *  strspn	span a string from charset
 *
 *  strpbrk	locate multiple chars in string
 *
 *  strsep	separate strings (modifying source string)
 *  strtok_r	string tokens (modifying source string)
 *
 *  memccpy	copy string until character found (including separator)
 */

char *strsep(char **stringp, const char *delim);
char *strtok_r(char *s, const char *delim, char **last);
int strdiv(char *token, int size, char **top, char *delim);

/* quote, number, name, */
/* 'abc', 123, 1.23, 1.23E2, 0x123, name, _name, name123
 * 
 * scanner   вк   parser
 *         <token>
 *
 * token вк literal(number, quoted string), symbol(name, sign)
 *
 * quote ::= ' [^']* ' 
 * number ::= {-|+}[0-9]*{.}[0-9]*{E{+|-}[0-9]*}
 * name ::= [_A-Za-z][_A-Za-z0-9]*
 * sign ::= = + - * / % || ( ) > >= < <= != <> ,
 *
 */
#define SCAN_QUOTE	1
#define SCAN_NUMBER	2
#define SCAN_NAME	3
#define SCAN_OP		4

int isdbcs(unsigned char *p);
unsigned char *nextchr(unsigned char *p);

int scan(char *tok, char **src)
{
    int paren;
    int rlen;
    int status;

    paren = 0;
    status = 0;
    rlen = 0;

    while (**src != '\0') {
	switch (status) {
	case SCAN_QUOTE:
	    break;
	case SCAN_NUMBER:
	    break;
	case SCAN_NAME:
	    break;
	case SCAN_OP:
	    break;
	}
    }

    return rlen;
}

int ctype()
{
    return 0;
}

int isdbcs(unsigned char *p)
{
    int rc;

    if (*p > 0x80)
	rc = 1;
    else
	rc = 0;

    return rc;
}

unsigned char *nextchr(unsigned char *p)
{
    if (isdbcs(p))
	p += 2;
    else
	p++;

    return p;
}

/*
 * divide token by string, lossless delimiter
 *
 * (1) one delim
 * (2) non-delim to delim
 * (3) non-delim (last token)
 */
/*
 * char *token	: o   : token
 * int size	: i   : token buffer size
 * char **top	: i/o : string top address
 * char *delim  : i   : delimiter
 * return	:     : length of token
 */
int strdiv(char *token, int size, char **top, char *delim)
{
    char *p, *q, *t;
    int i;

    if (*top == NULL)
	return NULL;
    *(t = token) = '\0';
    for (i = 0; i < (size - 1) && **top != '\0'; i++) {
	for (q = delim; *q != '\0'; q++) {
	    if (**top == *q) {
		if (t == token) {
		    i++;
		    *t = **top;
		    (*top)++;
		    t++;
		}
		*t = '\0';
		return i;
	    }
	}
	*t = **top;
	t++;
	(*top)++;
    }
    *t = '\0';

    return i;
}

/* the string functions of the standard C library */

/* same as FreeBSD 6.1 */
char *strsep(char **stringp, const char *delim)
{
    char *s;
    const char *spanp;
    int c, sc;
    char *tok;

    if ((s = *stringp) == NULL)
	return (NULL);
    for (tok = s;;) {
	c = *s++;
	spanp = delim;
	do {
	    if ((sc = *spanp++) == c) {
		if (c == 0)
		    s = NULL;
		else
		    s[-1] = 0;
		*stringp = s;
		return (tok);
	    }
	} while (sc != 0);
    }
    /* not reach */
}

/* same as FreeBSD 6.1 */
char *strtok_r(char *s, const char *delim, char **last)
{
    char *spanp, *tok;
    int c, sc;

    if (s == NULL && (s = *last) == NULL)
	return (NULL);
 cont:
    c = *s++;
    for (spanp = (char *)delim; (sc = *spanp++) != 0;) {
	if (c == sc)
	    goto cont;
    }
    if (c == 0) {		/* no non-delimiter characters */
	*last = NULL;
	return (NULL);
    }
    tok = s - 1;
    for (;;) {
	c = *s++;
	spanp = (char *)delim;
	do {
	    if ((sc = *spanp++) == c) {
		if (c == 0)
		    s = NULL;
		else
		    s[-1] = '\0';
		*last = s;
		return tok;
	    }
	} while (sc != 0);
    }
    /* not reach */
}
