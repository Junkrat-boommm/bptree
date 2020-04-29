//
// Created by ganquan on 2020/4/17.
//

#ifndef BPTREE_MURMURHASH3_H
#define BPTREE_MURMURHASH3_H
#define bp_fp(key, len) getHash(key, len)
//int murmur3_32(const char *key, int len);
int getHash(const char *key, int len);
#endif //BPTREE_MURMURHASH3_H
