#ifndef TPRINTF_H
#define TPRINTF_H

#define tprintf(args...) do { \
        struct timeval tv;     \
        gettimeofday(&tv, 0); \
        printf("%ld:\t", tv.tv_sec * 1000 + tv.tv_usec / 1000);\
        printf(args);   \
        } while (0);
#define cltprintf(args...)  {   \
        struct timeval tv;      \
        gettimeofday(&tv, 0);   \
        printf("%ld c[%s][l=%lld][t=%ld]%s:\t", tv.tv_sec * 1000 + tv.tv_usec / 1000, id.c_str(), lid, pthread_self(), __FUNCTION__);\
        printf(args);           \
        }
#define cltputs(__str) {        \
        struct timeval tv;      \
        gettimeofday(&tv, 0);   \
        printf("%ld c[%s][l=%lld][t=%ld]%s:\t", tv.tv_sec * 1000 + tv.tv_usec / 1000, id.c_str(), lid, pthread_self(), __FUNCTION__);\
        puts(__str);            \
        }
#endif