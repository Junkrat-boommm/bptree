//
// Created by ganquan on 2020/4/17.
//
#include "murmurhash3.h"

int getHash(const char *key, int len){
    const int p = 16777619;
    int hash = (int)2166136261L;
    for (int i = 0; i < len; i++)
        hash = (hash ^ key[i]) * p;
    hash += hash << 13;
    hash ^= hash >> 7;
    hash += hash << 3;
    hash ^= hash >> 17;
    hash += hash << 5;
    if (hash < 0)
        hash = -hash;
    return hash;
}

int murmur3_32(const char *key, int len)
{
    static const int c1 = 0xcc9e2d51;
    static const int c2 = 0x1b873593;
    static const int r1 = 15;
    static const int r2 = 13;
    static const int m = 5;
    static const int n = 0xe6546b64;

    int hash = 17;

    const int nblocks = len / 4;
    const int *blocks = (const int *)key;
    int i;
    for (i = 0; i < nblocks; i++)
    {
        int k = blocks[i];
        k *= c1;
        k = (k << r1) | (k >> (32 - r1));
        k *= c2;


        hash ^= k;
        hash = ((hash << r2) | (hash >> (32 - r2))) * m + n;
    }

    const int *tail = (const int *)(key + nblocks * 4);
    int k1 = 0;

    switch (len & 3)
    {
        case 3:
            k1 ^= tail[2] << 16;
        case 2:
            k1 ^= tail[1] << 8;
        case 1:
            k1 ^= tail[0];


            k1 *= c1;
            k1 = (k1 << r1) | (k1 >> (32 - r1));
            k1 *= c2;
            hash ^= k1;
    }

    hash ^= len;
    hash ^= (hash >> 16);
    hash *= 0x85ebca6b;
    hash ^= (hash >> 13);
    hash *= 0xc2b2ae35;
    hash ^= (hash >> 16);

    return hash;
}