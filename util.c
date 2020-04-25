//
// Created by ganquan on 2020/4/16.
//
#include "util.h"
int keyCompare(uint8_t *c1, uint8_t *c2, int l1, int l2) {
    uint8_t *s1 = c1;
    uint8_t *s2 = c2;
    for(; l1>0 && l2>0 ; s1++, s2++, l1--, l2--) {
        if(*s1 != *s2) {
            return (*s1 > *s2) ? 1 : -1;
        }
    }
    if (l1 == l2) return 0;
    if (l1 > 0) return 1;
    return -1;
}

int keyCompareWithMove(uint8_t **c1, uint8_t *c2, int l1, int l2) {
    for(; l1>0 && l2>0 ; (*c1)++, c2++, l1--, l2--) {
        if(**c1 != *c2) {
            return (**c1 > *c2) ? 1 : -1;
        }
    }
    if (l2 > 0 && 0 == l1) return -1;
    // else if (l2 > 0) return -1;
    return 0;
}


