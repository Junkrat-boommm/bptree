//
// Created by ganquan on 2020/4/16.
//

#ifndef BPTREE_UTIL_H
#define BPTREE_UTIL_H

#include "bplustree.h"
int keyCompare(uint8_t *c1, uint8_t *c2, int l1, int l2);
int keyCompareWithMove(uint8_t **c1, uint8_t *c2, int l1, int l2);
int keyContain(uint8_t *c1, uint8_t *c2, int l1, int l2);
#endif //BPTREE_UTIL_H
