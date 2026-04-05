/*
 * Coverity modelling file
 *
 */

typedef unsigned short wchar_t;
typedef void mbstate_t;

/* Inhibit use of built-in models for functions where Coverity's
 * assumptions about the modelled function are incorrect for wimboot.
 */
int getchar ( void ) {
}
size_t wcrtomb ( char *buf, wchar_t wc, mbstate_t *ps ) {
}
