//
// Created by ganquan on 2020/4/16.
//
#ifndef BPTREE_BPLUSTREE_H
#define BPTREE_BPLUSTREE_H
#include <stdint.h>
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
#define LEAF_MIN_SIZE 10
#define INTER_MIN_SIZE 10
#define BITMAP_LEN 8
#define LEAF_NODE_HEADER_LEN 1 + sizeof(size_t) * 3 + sizeof(void *) * 3 + BITMAP_LEN
#define INTER_NODE_HEADER_LEN 1 + sizeof(size_t) * 2 + sizeof(off_t) * 3 + sizeof(void *)
#define PREFIX_LEN 128
#define MAX_LEAF_NODE_SIZE 32
#define MAX_INTER_NODE_SIZE 32
#define MIN_INTER_NODE_SIZE 16
#define MIN_LEAF_NODE_SIZE 16
#define MAX_INTER_NODE_ITEM_BYTES KEY_MAX_SIZE_IN_IN + 1 + sizeof(off_t) + sizeof(void *)
#define LEAF_NODE_ITEM_SIZE 128

typedef struct bpInterNode {
    bpInterNodeMeta
    off_t keys;
    off_t children;
    void *parent;
    // void *next;
    // void *prev;
    off_t brothers;
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
    off_t brothers;
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
uint64_t bpFind(bpTree *t, unsigned char *key);
int bpInsert(bpTree *t, unsigned char *key, uint64_t data);
int bpRemove(bpTree *bpTree, unsigned char *key);
void *bpInterNodeFind(bpTree *t, bpInterNode *interNode, unsigned char *key);
uint64_t bpLeafNodeFind(bpTree *t, bpLeafNode *leafNode, unsigned char *key);
int OffBinarySearch(bpTree *t, bpInterNode *interNode, unsigned char *key);
void fpBinarySearch(bpTree *t, bpLeafNode *leafNode, unsigned char *key, int *success);
int _bpLeafNodeInsert(bpTree *t, bpLeafNode *leafNode, unsigned char *key, uint64_t data, int fpPos);
int bpLeafNodeInsert(bpTree *t, bpLeafNode *leafNode, unsigned char *key, uint64_t data);
int _bpLeafNodeRemove(bpTree *t, bpLeafNode *leafNode, unsigned char *key);
int leafCompareWithKey(bpTree *t, bpLeafNode *leafNode, unsigned char *key, KV *kv);
int _bpInterNodeInsert(bpTree *t, bpInterNode *interNode, void *child1, void *child2, unsigned char *key);
void bpInterNodeInsert(bpTree *t, bpInterNode *interNode, void *child1, void *child2, unsigned char *key, int LeftOrRight);
int _bpLeafNodeRemoveWithPos(bpTree *t, bpLeafNode *leafNode, int pos);
void bpInterNodeModifyOffset(bpInterNode *interNode, int pos, int off);
int bpInterNodeTryMerge(bpTree *t, bpInterNode *interNode);
void bpInterNodeMoveForNewKeyWithLeftChild(bpTree *t, bpInterNode *interNode, int pos, int len);
void bpInterNodeMoveForNewKeyWithRightChild(bpTree *t, bpInterNode *interNode, int pos, int len);
int _bpInterNodeInsertWithLeftChild(bpTree *t, bpInterNode *interNode, void *child, unsigned char *key);
int _bpInterNodeInsertWithRightChild(bpTree *t, bpInterNode *interNode, void *child, unsigned char *key);
void bpInterNodeShiftFromRight(bpTree *t, bpInterNode *interNode);
void bpInterNodeShiftFromLeft(bpTree *t, bpInterNode *interNode);
int bpInterNodeRemoveLeftChild(bpTree *t, bpInterNode *interNode, int pos);
int bpInterNodeRemoveRightChild(bpTree *t, bpInterNode *interNode, int pos);
void bpInterNodeSetMeta(bpTree *t, bpInterNode *interNode, int keyLen);
void bpInterNodeSetChildAddr(bpInterNode *interNode, int pos, void *child);
int bpLeafNodeSplit(bpTree *t, bpLeafNode *leafNode, unsigned char *key, uint64_t data);
void _bpInterNodeMergeWithRight(bpTree *t, bpInterNode *interNode);
void _bpInterNodeMergeWithLeft(bpTree *t, bpInterNode *interNode);
bpInterNode *prevBrother(bpTree *t, bpInterNode *interNode);
bpInterNode *nextBrother(bpTree *t, bpInterNode *interNode);
void bpInterNodeFree(bpInterNode *interNode);
void setChildrenParent(bpInterNode *interNode, bpInterNode *parent);
void bpInterNodeSetChildrenBrothers(bpInterNode *interNode, int pos, int begin);

#endif //BPTREE_BPLUSTREE_H
