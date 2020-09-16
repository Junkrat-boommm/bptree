//
// Created by ganquan on 2020/4/16.
//
#ifndef BPTREE_BPLUSTREE_H
#define BPTREE_BPLUSTREE_H
#include <stdint.h>
#include <stdio.h>
#include "string.h"
#define bpInterNodeMeta \
    uint8_t type;       \
    size_t bytes;       \
    size_t size;

#define bpLeafNodeNewMeta   \
    uint8_t type;           \
    size_t bytes;           \
    size_t size;

#define off_t uint16_t
#define addr_t uint32_t
#define fp_t uint8_t
#define LEAF_NODE 1
#define INTER_NODE 0

#define FP_LEN 4 // bytes
#define KEY_MAX 1024
#define KEY_MAX_SIZE_IN_IN 256
#define KEY_MAX_SIZE_IN_LN 256
#define VALUE_SIZE 16
#define KEY_META_SIZE sizeof(fp_t) + sizeof(off_t) + FLAGS_LEN + sizeof(uint32_t)
#define LEAF_NODE_BYTE 4096
#define MAX_INTER_NODE_BYTES 4096
#define FP_OFFSET_LEN sizeof(fp_t)+sizeof(off_t)
#define FLAGS_LEN 1
#define LEAF_MIN_SIZE 3
#define INTER_MIN_SIZE 10
#define BITMAP_LEN 8
#define LEAF_NODE_HEADER_LEN (1 + sizeof(size_t) * 3 + sizeof(void *) * 3 + BITMAP_LEN)
// #define INTER_NODE_HEADER_LEN (1 + sizeof(size_t) * 2 + sizeof(off_t) * 2 + sizeof(void *)) // 29
#define INTER_NODE_HEADER_LEN (sizeof(struct bpInterNode)) // 29
#define PREFIX_LEN 4
#define MAX_LEAF_NODE_SIZE 10
#define MIN_LEAF_NODE_SIZE 3
#define MAX_INTER_NODE_SIZE 4
#define MIN_INTER_NODE_SIZE 3
#define MAX_INTER_NODE_ITEM_BYTES KEY_MAX_SIZE_IN_IN + 1 + sizeof(off_t) + sizeof(void *)
#define LEAF_NODE_ITEM_SIZE 128


#define interKeyLen(n, i) ((i == n->size - 1) ? (n->children - n->keys) : *(off_t *)(n->data + (i+1) * sizeof(off_t))) - *(off_t *)(n->data + (i) * sizeof(off_t))
#define leafKeyLen(addr) *(uint32_t *)((uint8_t *)addr+FLAGS_LEN)
#define interKeyOffset(n, i) (*(off_t *)(n->data + (i) * sizeof(off_t)))
#define interKeyAddr(n, i) (n->data + n->keys + interKeyOffset(n, i))
#define interChildAddr(n, i) (n->data + n->children + sizeof(void *) * (i))
#define interOffsetAddr(n, i) (n->data + sizeof(off_t) * (i))
#define interKeysAddr(n) (n->data + n->keys)
#define interChildrenAddr(n) (n->data + n->children)
#define leafFpsAddr(n) (n->data + PREFIX_LEN)
#define keyFpAddr(n, i) (leafFpsAddr(n) + (i) * (FP_OFFSET_LEN))
#define leafKeysAddr(n) (n->data + PREFIX_LEN + LEAF_NODE_SIZE * (FP_OFFSET_LEN))
#define leafKeyOffset(n, i) (*((off_t *) (keyFpAddr(leafNode, i) + sizeof(fp_t))))
#define leafKeyAddr(n, i) leafKeysAddr(n) + leafKeyOffset(n, i)
#define prefixFlag(addr) (*(uint8_t *)addr) >> 7
#define longFlag(addr) (*((uint8_t *)addr)) << 1 >> 7
#define prefixFlagFromFlags(flags) (flags >> 7)
#define longFlagFromFlags(flags) (flags << 1 >> 7)
#define valueAddr(addr) ((uint8_t *)addr + FLAGS_LEN + sizeof(uint32_t) + leafKeyLenInNode(addr) + sizeof(addr_t))

typedef struct bpInterNode {
    bpInterNodeMeta
    off_t keys;
    off_t children;
    void *parent;
    uint8_t data[];
} bpInterNode;

typedef struct KV {
    uint8_t flags;
    uint32_t len;
    unsigned char key[KEY_MAX_SIZE_IN_LN];
    void *ptr;
    uint64_t value;
} KV;

typedef struct fpAndOffset {
    fp_t fingerprint;
    off_t pos;
} fpAndOffset;

typedef struct bpLeafNode {
    bpLeafNodeNewMeta
    size_t prefixLen;
    uint8_t bitmap[8];
    void *parent;
    void *next;
    void *prev;
    off_t min, max;
    unsigned char prefix[PREFIX_LEN];
    fpAndOffset fp[MAX_LEAF_NODE_SIZE];
    KV kv[MAX_LEAF_NODE_SIZE];
} bpLeafNode;

typedef struct bpTree {
    void *header;
    size_t bytes;
    size_t size;
    uint64_t level;
} bpTree;

bpTree *bpTreeNew(void);
bpInterNode *bpInterNodeNew(void);
bpLeafNode *bpLeafNodeNew(void);
uint64_t bpFind(bpTree *t, unsigned char *key, int len);
int bpInsert(bpTree *t, unsigned char *key, uint64_t data, int len);
int bpRemove(bpTree *bpTree, unsigned char *key, int len);
void *bpInterNodeFind(bpTree *t, bpInterNode *interNode, unsigned char *key, int len);
uint64_t bpLeafNodeFind(bpTree *t, bpLeafNode *leafNode, unsigned char *key, int len);
int OffBinarySearch(bpTree *t, bpInterNode *interNode, unsigned char *key, int len);
void fpBinarySearch(bpTree *t, bpLeafNode *leafNode, unsigned char *key, int *success, int len);
void bpLeafNodeSplit(bpTree *t, bpLeafNode *leafNode, unsigned char *key, uint64_t data, int len);
int _bpLeafNodeInsert(bpTree *t, bpLeafNode *leafNode, unsigned char *key, uint64_t data, int fpPos, int len);
int bpLeafNodeInsert(bpTree *t, bpLeafNode *leafNode, unsigned char *key, uint64_t data, int len);
int _bpLeafNodeRemove(bpTree *t, bpLeafNode *leafNode, unsigned char *key, int len);
void bpLeafNodeSplit(bpTree *t, bpLeafNode *leafNode, unsigned char *key, uint64_t data, int len);
void bpLeafNodeShiftFromLeft(bpTree *t, bpLeafNode *leafNode);
int leafCompareWithKey(bpTree *t, bpLeafNode *leafNode, unsigned char *key, KV *kv, int len);
// int _bpInterNodeInsert(bpTree *t, bpInterNode *interNode, void *child1, void *child2, unsigned char *key);
void bpInterNodeInsert(bpTree *t, bpInterNode *interNode, void *child1, void *child2, unsigned char *key, int LeftOrRight, int len);
void bpInterNodeModifyOffset(bpInterNode *interNode, int pos, int off);
int bpInterNodeTryMerge(bpTree *t, bpInterNode *interNode);
void bpInterNodeMoveForNewKeyWithLeftChild(bpTree *t, bpInterNode *interNode, int pos, int len);
void bpInterNodeMoveForNewKeyWithRightChild(bpTree *t, bpInterNode *interNode, int pos, int len);
int _bpInterNodeInsertWithLeftChild(bpTree *t, bpInterNode *interNode, void *child, unsigned char *key, int len);
int _bpInterNodeInsertWithRightChild(bpTree *t, bpInterNode *interNode, void *child, unsigned char *key, int len);
void bpInterNodeShiftFromRight(bpTree *t, bpInterNode *interNode, int keyPos);
void bpInterNodeShiftFromLeft(bpTree *t, bpInterNode *interNode, int keyPos);
int bpInterNodeRemoveLeftChild(bpTree *t, bpInterNode *interNode, int pos);
int bpInterNodeRemoveRightChild(bpTree *t, bpInterNode *interNode, int pos);
void bpInterNodeSetMeta(bpTree *t, bpInterNode *interNode, int keyLen);
void bpInterNodeSetChildAddr(bpInterNode *interNode, int pos, void *child);
int _bpLeafNodeRemoveWithPos(bpTree *t, bpLeafNode *leafNode, int pos);
void _bpInterNodeMergeWithRight(bpTree *t, bpInterNode *interNode, int keyPos);
void _bpInterNodeMergeWithLeft(bpTree *t, bpInterNode *interNode, int keyPos);
bpInterNode *prevBrother(bpTree *t, bpInterNode *interNode, int pos);
bpInterNode *nextBrother(bpTree *t, bpInterNode *interNode, int pos);
void bpInterNodeFree(bpInterNode *interNode);
void setChildrenParent(bpInterNode *interNode, bpInterNode *parent);
void bpInterNodeSetChildrenBrothers(bpInterNode *interNode, int pos, int begin);
int OffBinarySearch(bpTree *t, bpInterNode *interNode, unsigned char *key, int len);
int bpLeafNodeTryMerge(bpTree *t, bpLeafNode *leafNode);
int setBitmap(uint8_t bitmap[], int pos, int v);
void printInterNode(bpInterNode *t);
void bpLeafNodeFree(bpLeafNode *leafNode);
int bpLeafFindMaxExceptMax(bpLeafNode *leafNode, int pos);
int bpLeafFindMinExceptMin(bpLeafNode *leafNode, int pos);
void bpLeafNodeShiftFromRight(bpTree *t, bpLeafNode *leafNode);
void bpLeafNodeShiftFromLeft(bpTree *t, bpLeafNode *leafNode);
void setParent(void *node, bpInterNode *interNode);


#endif //BPTREE_BPLUSTREE_H
