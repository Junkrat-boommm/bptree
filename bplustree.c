//
// Created by ganquan on 2020/4/16.
//
#include "bplustree.h"
#include "bp_malloc.h"
#include "murmurhash3.h"
#include "util.h"



#define interKeyLen(n, i) ((i == n->size) ? (n->children - n->keys) : *(off_t *)(n->data + (i+1) * sizeof(off_t))) - *(off_t *)(n->data + i * sizeof(off_t))
#define leafKeyLen(addr) *(uint32_t *)((uint8_t *)addr+FLAGS_LEN)
#define interKeyOffset(n, i) *(off_t *)(n->data + i * sizeof(off_t))
#define interKeyAddr(n, i) n->data + n->keys + interKeyOffset(n, i)
#define interChildAddr(n, i) n->data + n->children + sizeof(void *) * i
#define interOffsetAddr(n, i) n->data + sizeof(off_t) * i
#define interKeysAddr(n) n->data + n->keys
#define interChildrenAddr(n) n->data + n->children
#define leafFpsAddr(n) n->data + PREFIX_LEN
#define keyFpAddr(n, i) leafFpsAddr(n) + i * (FP_OFFSET_LEN)
#define leafKeysAddr(n) n->data + PREFIX_LEN + LEAF_NODE_SIZE * (FP_OFFSET_LEN)
#define leafKeyOffset(n, i) *((off_t *) (keyFpAddr(leafNode, i) + sizeof(fp_t)))
#define leafKeyAddr(n, i) leafKeysAddr(n) + leafKeyOffset(n, i)
#define prefixFlag(addr) (*(uint8_t *)addr) >> 7
#define longFlag(addr) (*((uint8_t *)addr)) << 1 >> 7
#define prefixFlagFromFlags(flags) flags >> 7
#define longFlagFromFlags(flags) flags << 1 >> 7
#define valueAddr(addr) ((uint8_t *)addr + FLAGS_LEN + sizeof(uint32_t) + leafKeyLenInNode(addr) + sizeof(addr_t))

// create a new B+tree
bpTree *bpTreeNew(void) {
    bpTree *t = bp_malloc(sizeof(*t));
    if (t == NULL) return NULL;
    t->header = bpLeafNodeNew();
    t->bytes = 0;
    t->size = 0;
    t->level = 1;
    return t;
}

bpInterNode *bpInterNodeNew(void) {
    bpInterNode *node = bp_malloc(sizeof(*node));
    if (node == NULL) return NULL;
    node->type = INTER_NODE;
    node->bytes = INTER_NODE_HEADER_LEN;
    node->keys = node->children = node->bytes = node->brothers = 0;
    return node;
}

bpLeafNode *bpLeafNodeNew(void) {
    bpLeafNode *ln = bp_malloc(sizeof(*ln));
    if (ln == NULL) return NULL;
    ln->type = LEAF_NODE;
    ln->bytes = LEAF_NODE_HEADER_LEN;
    return ln;
}

uint64_t bpFind(bpTree *t, unsigned char *key) {
    if (key == NULL || 0 == strlen(key) || t == NULL) return -1;
    void *h = t->header;
    int l = t->level;
    if (l == 0 || h == NULL) return -1;
    if (l == 1) {
        return bpLeafNodeFind(t, h, key);
    }
    else {
        bpLeafNode *ln = bpInterNodeFind(t, h, key);
        return bpLeafNodeFind(t, ln, key);
    }
}

// find the leafNode
void *bpInterNodeFind(bpTree *t, bpInterNode *interNode, unsigned char *key) {
    if (interNode == NULL) return NULL;
    size_t size = interNode->size;
    uint8_t type = interNode->type;
    if (type != INTER_NODE || size == 0) return NULL;
    int child = OffBinarySearch(t, interNode, key);
    void **next = (void **)interChildAddr(interNode, child);
    if (*(uint8_t *)(*next) == INTER_NODE) {
        return bpInterNodeFind(t, (bpInterNode *)(*next), key);
    } else return next;
}


int OffBinarySearch(bpTree *t, bpInterNode *interNode, unsigned char *key) {
    int low = -1;
    int len = interNode->size;
    int high = len;
    // off_t offset = interNode->offset;
    while (low + 1 < high) {
        int mid = low + (high - low)/2;
        if (-1 != keyCompare(interKeyAddr(interNode, mid), (uint8_t *)key, interKeyLen(interNode, mid), strlen(key))) {
            low = mid;
        } else {
            high = mid;
        }
    }
    return high;
}

// set children's parent
void setChildrenParent(bpInterNode *interNode, bpInterNode *parent) {
    void **childAddr = (void **)interChildrenAddr(interNode);
    void *node = *childAddr;
    int i=0;
    while(i <= interNode->size) {
        if(*(uint8_t *)node == INTER_NODE) {
            ((bpInterNode *)node)->parent = parent;
        } else {
            ((bpLeafNode *)node)->parent = parent;
        }
        i ++;
        node = *(childAddr+i);
    }
}

int bpInterNodeRemoveRightChild(bpTree *t, bpInterNode *interNode, int pos) {
    uint8_t *keyAddr = interKeyAddr(interNode, pos);
    uint8_t *offsetAddr = interKeyAddr(interNode, pos);
    uint8_t *childAddr = interKeyAddr(interNode, pos + 1);
    int keyLen = interKeyLen(interNode, pos);
    uint8_t *offsetMoveBegin = offsetAddr + sizeof(off_t);
    uint8_t *offsetMoveTo = offsetAddr;
    int offsetMoveLen = keyAddr - offsetMoveBegin;
    uint8_t *keyMoveBegin = keyAddr + keyLen;
    uint8_t *keyMoveTo = keyAddr - sizeof(off_t);
    int keyMoveLen = childAddr - keyMoveBegin;
    uint8_t *childMoveBegin = childAddr + sizeof(void *);
    uint8_t *childMoveTo = childAddr - keyLen - sizeof(off_t);
    int childMoveLen = sizeof(void *) * (interNode->size - pos - 1);
    memmove(offsetMoveTo, offsetMoveBegin, offsetMoveLen);
    memmove(keyMoveTo, keyMoveBegin, keyMoveLen);
    memmove(childMoveTo, childMoveBegin, childMoveLen+1);
    bpInterNodeSetMeta(t, interNode, -keyLen);
    // TODO: modify the offset
    bpInterNodeModifyOffset(interNode, pos, -keyLen);
    bpInterNodeTryMerge(t, interNode);
}

// 删除中间节点中的第pos个key以及一个孩子节点
int bpInterNodeRemoveLeftChild(bpTree *t, bpInterNode *interNode, int pos) {
    uint8_t *keyAddr = interKeyAddr(interNode, pos);
    uint8_t *offsetAddr = interKeyAddr(interNode, pos);
    uint8_t *childAddr = interKeyAddr(interNode, pos);
    int keyLen = interKeyLen(interNode, pos);
    uint8_t *offsetMovebegin = offsetAddr + sizeof(off_t);
    uint8_t *offsetMoveTo = offsetAddr;
    int offsetMoveLen = keyAddr - offsetMovebegin;
    uint8_t *keyMoveBegin = keyAddr + keyLen;
    uint8_t *keyMoveTo = keyAddr - sizeof(off_t);
    int keyMoveLen = childAddr - keyMoveBegin;
    uint8_t *childMoveBegin = childAddr + sizeof(void *);
    uint8_t *childMoveTo = childAddr - keyLen - sizeof(off_t);
    int childMoveLen = sizeof(void *) * (interNode->size - pos);
    memmove(offsetMoveTo, offsetMovebegin, offsetMoveLen);
    memmove(keyMoveTo, keyMoveBegin, keyMoveLen);
    memmove(childMoveTo, childMoveBegin, childMoveLen);
    bpInterNodeSetMeta(t, interNode, -keyLen);
    // TODO: modify the offset
    bpInterNodeModifyOffset(interNode, pos, -keyLen);
    bpInterNodeTryMerge(t, interNode);
}


int bpInterNodeSet(bpTree *t, bpInterNode *interNode, int pos, unsigned char *key) {
    uint8_t *keyAddr = interKeyAddr(interNode, pos);
    int oralLen = interKeyLen(interNode, pos);
    uint8_t *moveBegin = keyAddr + oralLen;
    int keyLen = strlen(key);
    memmove(moveBegin + keyLen - oralLen, moveBegin, (uint8_t *)interNode + interNode->size - moveBegin + 1);
    if(keyLen > KEY_MAX_SIZE_IN_IN) keyLen = KEY_MAX_SIZE_IN_IN + sizeof(void *) + 1;
    if(keyLen == KEY_MAX_SIZE_IN_IN + sizeof(void *) + 1) {
        *keyAddr = 1;
        memcpy(keyAddr+1, key, KEY_MAX_SIZE_IN_IN);
    } else {
        *keyAddr = 0;
        memcpy(keyAddr+1, key, keyLen - 1);
    }
    return 0;
}

void bpInterNodeInsert(bpTree *t, bpInterNode *interNode, void *left, void *right, unsigned char *key, int LeftOrRight) {
    if (interNode == NULL) {    // need to malloc a new interNode
        bpInterNode *newHeader = bpInterNodeNew();
        // set offset
        *(off_t *)newHeader->data = 0;

        int keyLen = strlen(key);
        if(keyLen > KEY_MAX_SIZE_IN_IN) keyLen = KEY_MAX_SIZE_IN_IN + sizeof(void *);
        keyLen += 1; // for flag

        int totalLen = sizeof(void *) * 2 + sizeof(off_t) + keyLen;
        newHeader->size ++;
        newHeader->bytes += totalLen;
        newHeader->keys += sizeof(off_t);
        newHeader->children += sizeof(off_t) + keyLen;

        uint8_t *keyAddr = interKeysAddr(newHeader);
        // set the flag and key
        if(keyLen == KEY_MAX_SIZE_IN_IN + sizeof(void *) + 1) {
            *keyAddr = 1;
            memcpy(keyAddr+1, key, KEY_MAX_SIZE_IN_IN);
        } else {
            *keyAddr = 0;
            memcpy(keyAddr+1, key, keyLen - 1);
        }
        // set the children
        uint8_t *childAddr = interChildrenAddr(newHeader);
        void **childBegin = (void **)childAddr;
        *(childBegin) = left;
        *(childBegin + 1) = right;

        setChildrenParent(newHeader, newHeader);
        bpInterNodeSetChildrenBrothers(newHeader, 0, 0);
        return;
    } else if (-1 == LeftOrRight) {
        _bpInterNodeInsertWithLeftChild(t, interNode, left, key);
    } else if (1 == LeftOrRight) {
        _bpInterNodeInsertWithRightChild(t, interNode, left, key);
    }
}


int bpInterNodeSplit(bpTree *t, bpInterNode *interNode) {
    int split = interNode->size/2;
    unsigned char *splitKey = malloc(KEY_MAX_SIZE_IN_IN);
    if(splitKey == NULL) return -1;
    memcpy(splitKey, interKeyAddr(interNode,split), interKeyLen(interNode, split));
    bpInterNode *newNode = bpInterNodeNew();

    uint8_t *keyAddr = interKeyAddr(interNode, split+1);
    uint8_t *offsetAddr = interOffsetAddr(interNode, split);
    uint8_t *childAddr = interChildAddr(interNode, split);

    // handle newNode
    newNode->keys = newNode->children = sizeof(off_t) * split;
    memcpy(newNode->data, interNode->data, newNode->keys);
    int keyMoveLen = interKeyOffset(interNode, split);
    memcpy(interKeysAddr(newNode), interKeysAddr(interNode), keyMoveLen);
    newNode->children += keyMoveLen;
    int childrenMoveLen = sizeof(void *) * (split+1);
    memcpy(interChildrenAddr(newNode), interChildrenAddr(interNode), childrenMoveLen);
    newNode->size = split;
    newNode->bytes = newNode->children + childrenMoveLen;

    // handle the original interNode
    int leftKeyLen = interNode->children - interNode->keys -  interKeyOffset(interNode, split + 1);
    int leftChildrenLen = sizeof(void *) * (interNode->size - split);
    int deleteKeyLen = interKeyOffset(interNode, split + 1);
    int deleteOffsetLen = sizeof(off_t) * (split + 1);
    int deleteChildrenLen = sizeof(void *) * (split + 1);
    uint8_t *oldChildrenBegin = interChildAddr(interNode, split + 1);
    uint8_t *oldKeysBegin = interKeyAddr(interNode, split + 1);
    interNode->keys -= deleteOffsetLen;
    interNode->children -= (deleteOffsetLen + deleteKeyLen);

    // move the offset
    memmove(interNode->data, offsetAddr + sizeof(fp_t), (newNode->size - split - 1) * sizeof(off_t));
    // move the keys
    memmove(interKeysAddr(interNode), oldKeysBegin, leftKeyLen);
    // move the children
    memmove(interChildrenAddr(interNode), oldChildrenBegin, leftChildrenLen + 1);

    interNode->size -= (split + 1);
    interNode->bytes -= (deleteOffsetLen + deleteKeyLen + deleteChildrenLen);

    // set parent for the children of new node
    setChildrenParent(newNode, newNode);

    _bpInterNodeInsertWithLeftChild(t, interNode->parent, newNode, splitKey);
    free(splitKey);
}

// int bpInterNodeInsert(bpTree *t, bpInterNode *interNode, void *child1, void *child2, unsigned char *key) {
//    if(interNode == NULL) {
//        t->header = bpInterNodeNew();
//        t->level ++;
//        interNode = t->header;
//    }
//
//    int keyLen = strlen(key);
//    if(keyLen > MAX_INTER_KEY_LEN) keyLen = MAX_INTER_KEY_LEN + sizeof(void *);
//    keyLen += 1; // for flag
//    int totalLen = sizeof(void*) + sizeof(off_t) + keyLen;
//
//    _bpInterNodeInsert(t, interNode, child1, NULL, key);
//
//    if (interNode->size)
//
//    if (interNode->bytes + totalLen > INTER_NODE_SIZE) {
//        // TODO: need to split
//        bpInterNodeSplitAndInsert(t, interNode, child1, child2, key);
//    }
//    else {
//        _bpInterNodeInsert(t, interNode, child1, NULL, key);
//    }
// }

void setParent(void *node, bpInterNode *interNode) {
    if(*(uint8_t *)node == INTER_NODE) {
        ((bpInterNode *)node)->parent = (void *)interNode;
    } else {
        ((bpLeafNode *)node)->parent = (void *)interNode;
    }
}

void bpInterNodeModifyOffset(bpInterNode *interNode, int pos, int off) {
    off_t *begin = (off_t *)(interOffsetAddr(interNode, pos));
    while (pos < interNode->size) {
        *begin += off;
        pos ++;
        begin += sizeof(off_t);
    }
}

void bpInterNodeSetMeta(bpTree *t, bpInterNode *interNode, int keyLen) {
    if (keyLen > 0) {
        interNode->size ++;
        interNode->bytes += keyLen + sizeof(void *) + sizeof(off_t);
        interNode->keys += sizeof(off_t);
        interNode->children += (sizeof(off_t) + keyLen);
    } else {
        interNode->size --;
        interNode->bytes -= (-keyLen + sizeof(void *) + sizeof(off_t));
        interNode->keys -= sizeof(off_t);
        interNode->children -= (sizeof(off_t) - keyLen);
    }
}

void bpInterNodeMoveForNewKeyWithLeftChild(bpTree *t, bpInterNode *interNode, int pos, int len) {
    uint8_t *keyAddr = interChildrenAddr(interNode);
    uint8_t *offsetAddr = interKeyAddr(interNode, pos);
    uint8_t *childAddr = interKeyAddr(interNode, pos);
    if (pos < interNode->size) {
        keyAddr = interKeyAddr(interNode, pos);
    }
    uint8_t *childMoveBegin = childAddr;
    uint8_t *childMoveTo = childAddr + len + sizeof(void *) + sizeof(off_t);
    int childMoveLen = sizeof(void *) * (interNode->size - pos + 1);
    uint8_t *keyMoveBegin = keyAddr;
    uint8_t *keyMoveTo = keyAddr + len + sizeof(off_t);
    int keyMoveLen = childAddr - keyMoveBegin;
    uint8_t *offsetMoveBegin = offsetAddr;
    uint8_t *offsetMoveTo = offsetAddr + sizeof(off_t);
    int offsetMoveLen = keyAddr - offsetAddr;
    memmove(childMoveTo, childMoveBegin, childMoveLen + 1);
    memmove(keyMoveTo, keyMoveBegin, keyMoveLen);
    memmove(offsetMoveTo, offsetMoveBegin, offsetMoveLen);
    // set offset
    *(off_t *)offsetAddr = childAddr - interKeysAddr(interNode);
    bpInterNodeSetMeta(t, interNode, len);
    bpInterNodeModifyOffset(interNode, pos + 1, len);
}


void bpInterNodeMoveForNewKeyWithRightChild(bpTree *t, bpInterNode *interNode, int pos, int len) {
    uint8_t *keyAddr = interChildrenAddr(interNode);
    uint8_t *offsetAddr = interKeyAddr(interNode, pos);
    uint8_t *childAddr = interKeyAddr(interNode, pos + 1);
    if (pos < interNode->size) {
        keyAddr = interKeyAddr(interNode, pos);
    }
    uint8_t *childMoveBegin = childAddr;
    uint8_t *childMoveTo = childAddr + len + sizeof(void *) + sizeof(off_t);
    int childMoveLen = sizeof(void *) * (interNode->size - pos);
    uint8_t *keyMoveBegin = keyAddr;
    uint8_t *keyMoveTo = keyAddr + len + sizeof(off_t);
    int keyMoveLen = childAddr - keyMoveBegin;
    uint8_t *offsetMoveBegin = offsetAddr;
    uint8_t *offsetMoveTo = offsetAddr + sizeof(off_t);
    int offsetMoveLen = keyAddr - offsetAddr;
    memmove(childMoveTo, childMoveBegin, childMoveLen + 1);
    memmove(keyMoveTo, keyMoveBegin, keyMoveLen);
    memmove(offsetMoveTo, offsetMoveBegin, offsetMoveLen);
    // TODO: 修改offset
    *(off_t *)offsetAddr = childAddr - interKeysAddr(interNode);
    bpInterNodeSetMeta(t, interNode, len);
    bpInterNodeModifyOffset(interNode, pos + 1, len);
}


int _bpInterNodeInsertWithLeftChild(bpTree *t, bpInterNode *interNode, void *child, unsigned char *key) {
    if(interNode == NULL) {
        exit(-1);
    }

    // set parent
    setParent(child, interNode);

    int pos = OffBinarySearch(t, interNode, key);

    // 移动以获得空间
    int keyLen = strlen(key);
    if(keyLen > KEY_MAX_SIZE_IN_LN) keyLen = KEY_MAX_SIZE_IN_LN + sizeof(void *) + 1;
    else keyLen += 1; // for flag
    bpInterNodeMoveForNewKeyWithLeftChild(t, interNode, pos, keyLen);

    uint8_t *keyAddr = interKeyAddr(interNode, pos);
    uint8_t *childAddr = interChildAddr(interNode, pos);
    uint8_t *offsetAddr = interOffsetAddr(interNode, pos);

    // fill the flag and key
    if(keyLen == KEY_MAX_SIZE_IN_LN + sizeof(void *) + 1) {
        *keyAddr = 1;
        memcpy(keyAddr+1, key, KEY_MAX_SIZE_IN_LN);
        // TODO: handle long key
    } else {
        *keyAddr = 0;
        memcpy(keyAddr+1, key, keyLen - 1);
    }

    // fill the child
    bpInterNodeSetChildAddr(interNode, pos, child);

    if (interNode->size + 1 >= MAX_INTER_NODE_SIZE || interNode->bytes + KEY_MAX_SIZE_IN_LN > MAX_INTER_NODE_BYTES) {
        bpInterNodeSplit(t, interNode);
    }

    return 0;
}

int _bpInterNodeInsertWithRightChild(bpTree *t, bpInterNode *interNode, void *child, unsigned char *key) {
    if(interNode == NULL) {
        exit(-1);
    }

    // set parent
    setParent(child, interNode);

    int pos = OffBinarySearch(t, interNode, key);

    // 移动以获得空间
    int keyLen = strlen(key);
    if(keyLen > KEY_MAX_SIZE_IN_IN) keyLen = KEY_MAX_SIZE_IN_IN + sizeof(void *) + 1;
    else keyLen += 1; // for flag
    bpInterNodeMoveForNewKeyWithRightChild(t, interNode, pos, keyLen);

    uint8_t *keyAddr = interKeyAddr(interNode, pos);
    uint8_t *childAddr = interChildAddr(interNode, pos);
    uint8_t *offsetAddr = interOffsetAddr(interNode, pos);

    // fill the flag and key
    if(keyLen == KEY_MAX_SIZE_IN_IN + sizeof(void *) + 1) {
        *keyAddr = 1;
        memcpy(keyAddr+1, key, KEY_MAX_SIZE_IN_IN);
        // TODO: handle long key
    } else {
        *keyAddr = 0;
        memcpy(keyAddr+1, key, keyLen - 1);
    }

    // fill the child
    // memcpy(childAddr, &child1, sizeof(void *));

    bpInterNodeSetChildAddr(interNode, pos + 1, child);
    return 0;
}



// int _bpInterNodeInsert(bpTree *t, bpInterNode *interNode, void *child1, void *child2, unsigned char *key) {
//    if(interNode == NULL) {
//        exit(-1);
//    }
//
//    // set parent
//    setParent(child1, interNode);
//    setParent(child2, interNode);
//
//    int child = OffBinarySearch(t, interNode, key);
//    uint8_t *keyAddr = interKeyAddr(interNode, child);
//    uint8_t *childAddr = interChildAddr(interNode, child);
//    uint8_t *offsetAddr = interOffsetAddr(interNode, child);
//    off_t offset = interKeyOffset(interNode, child);
//
//    int keyLen = strlen(key);
//    if(keyLen > MAX_INTER_KEY_LEN) keyLen = MAX_INTER_KEY_LEN + sizeof(void *) + 1;
//    else keyLen += 1; // for flag
//    // move the children ptr
//    int totalLen = sizeof(void*) + sizeof(off_t) + keyLen;
//    memmove(childAddr + totalLen, childAddr, sizeof(void *)*(interNode->size + 1 - child));
//
//    // move the key
//    memmove(keyAddr + keyLen + sizeof(off_t), keyAddr, childAddr-keyAddr);
//
//    // move the offset
//    memmove(interNode->data + (child+1) * sizeof(off_t), interNode->data + child * sizeof(off_t), keyAddr - offsetAddr);
//
//    // modify data offset
//    interNode->keys += sizeof(off_t);
//    interNode->children += sizeof(off_t) + keyLen;
//
//    //get the new address
//    keyAddr = interKeyAddr(interNode, child);
//    childAddr = interChildAddr(interNode, child);
//    offsetAddr = interOffsetAddr(interNode, child);
//
//    // fill the offset, and modify offset for next key
//    *((off_t *)offsetAddr) = offset;
//    bpInterNodeModifyOffset(interNode, child + 1, keyLen);
//
//    // fill the flag and key
//    if(keyLen == MAX_INTER_KEY_LEN + sizeof(void *) + 1) {
//        *keyAddr = 1;
//        memcpy(keyAddr+1, key, MAX_INTER_KEY_LEN);
//
//    } else {
//        *keyAddr = 0;
//        memcpy(keyAddr+1, key, keyLen - 1);
//    }
//
////    // fill the child
//      memcpy(childAddr, &child1, sizeof(void *));
////    if (*(uint8_t *)child2 == LEAF_NODE){ // child1 is always new
////        void *old = *(void **)(childAddr + sizeof(void *));
////        if(child2 != old) { // free old child, only for leaf split
////            free(old);
////        }
////    }
//
////  if(child2 != NULL) memcpy(childAddr + sizeof(void *), &child2, sizeof(void *));
//    interNode->size ++;
//    interNode->bytes += totalLen;
//
//    return 0;
// }

uint64_t bpLeafNodeFind(bpTree *t, bpLeafNode *leafNode, unsigned char *key) {
    if (leafNode->type != LEAF_NODE) return -1;
    int success = 0;
    fpBinarySearch(t, leafNode, key, &success);
    if (success > 0) { // TODO: handle the hash conflict
        return leafNode->kv[success].value;
    }
    return -1;
}

int leafCompareWithKey(bpTree *t, bpLeafNode *leafNode, unsigned char *key, KV *kv) {
    uint8_t *s1 = key;
    //uint8_t *s2 = keyAddr;
    int b = 0;
    int res = 0;
    if (1 == prefixFlagFromFlags(kv->flags)) {
        res = keyCompareWithMove(&s1, leafNode->prefix, strlen(s1), leafNode->prefixLen);
        if (res != 0) {
            return res;
        }
    }
    res = keyCompareWithMove(&s1, kv->key, strlen(s1), kv->len);
    if (res != 0) {
        return res;
    }
    if (longFlagFromFlags(kv->flags) == 1) { // TODO: handle long key, keyAddr already changed

    }
    // matched, return the value
    if (strlen(s1) == 0) {
        return 0;
    } else {
        return 1;
    }
}

int handleHashConflict(bpTree *t, bpLeafNode *leafNode, unsigned char *key, fp_t fp, int high) {
    while((++(high)) <= leafNode->size) {
        fp_t kf = leafNode->fp[high].fingerprint;
        if(kf != fp) break;
        int pos = leafNode->fp[high].pos;
        KV *kv = &leafNode->kv[pos];
        if(leafCompareWithKey(t, leafNode, key, kv) == 0) {
            return high;
        }
    }
    return 0;
}

void fpBinarySearch(bpTree *t, bpLeafNode *leafNode, unsigned char *key, int *success) {
    uint8_t fp = murmur3_32(key, strlen(key));
    int low = -1;
    int len = leafNode->size;
    int high = len;
    int mid = 0;
    fp_t kf;
    while(low + 1 < high) {
        mid = low + (high - low)/2;
        kf = leafNode->fp[mid].fingerprint;
        if (kf < fp) {
            low = mid;
        } else{
            high = mid;
        }
    }
    KV *kv = &(leafNode->kv[leafNode->fp[high].pos]);
    // find the insert position
    if (high >= len || kf != fp) {
        *success = -high-1;
    } else { // maybe need to handle the hash conflict
        if(leafCompareWithKey(t, leafNode, key, kv) != 0) {
            int res = handleHashConflict(t, leafNode, key, fp, high);
            if(res == 0) {
                *success = -high-1;
            }
            else *success = res;
        }
        *success = high;
    }
    return ; // TODO: need to delete
}

int bpInsert(bpTree *t, unsigned char *key, uint64_t data) {
    if (key == NULL || 0 == strlen(key) || t == NULL) return -1;
    void *h = t->header;
    int l = t->level;
    if (l == 0 || h == NULL) return -1;
    if (l == 1) {
        return bpLeafNodeInsert(t, h, key, data);
    }
    bpLeafNode *ln = bpInterNodeFind(t, h, key);
    return bpLeafNodeInsert(t ,ln, key, data);
}

int getKey(bpLeafNode *leafNode, int id, unsigned char *data) {
    memset(data, 0, KEY_MAX);
    KV *kv = &leafNode->kv[leafNode->fp[id].pos];
    int l=0;
    if (longFlagFromFlags(kv->flags) == 1) {
        memcpy(data, leafNode->prefix, leafNode->prefixLen);
        l += leafNode->prefixLen;
        memcpy(data+l, kv->key, kv->len);
        l += kv->len;
        // TODO:handle long key
    }
    data[l] = '\0';
    return l;
}


int leafKeyCmp(bpLeafNode *leafNode,int c1, int c2) {
    KV *kv1 = &leafNode->kv[leafNode->fp[c1].pos];
    KV *kv2 = &leafNode->kv[leafNode->fp[c2].pos];
    if (prefixFlagFromFlags(kv1->flags) == prefixFlagFromFlags(kv2->flags)) {
        return keyCompare(kv1->key, kv2->key, kv1->len, kv2->len);
    }
    if (prefixFlagFromFlags(kv1->flags) == 1) {
        return keyCompare(leafNode->prefix, kv2->key, leafNode->prefixLen, kv2->len);
    }
    return keyCompare(kv1->key, leafNode->prefix, kv1->len, leafNode->prefixLen);
}

int partition(bpLeafNode *leafNode, int arr[], int left, int right) {
    int pivot = left;
    int index = pivot+1;
    for(int i = index; i <= right; i++) {
        if(leafKeyCmp(leafNode, i, pivot) == 1) {
            int temp = arr[i];
            arr[i] = arr[index];
            arr[index] = temp;
            index ++;
        }
    }
    int temp = arr[pivot];
    arr[pivot] = arr[index-1];
    arr[index-1] = temp;
    return index - 1;
}

void quickSort(bpLeafNode *leafNode, int arr[], int left, int right) {
    if(left < right) {
        int partitionIndex = partition(leafNode, arr, left, right);
        quickSort(leafNode, arr, left, partitionIndex-1);
        quickSort(leafNode, arr, partitionIndex+1, right);
    }
    return;
}


int CommonPrefix(bpLeafNode *leafNode, int left, int right) { // TODO: handle the long key
    KV *kvLeft = &leafNode->kv[left];
    KV *kvRight = &leafNode->kv[right];
    int i = 0;
    if(longFlagFromFlags(kvLeft->flags) == longFlagFromFlags(kvRight->flags)) {
        while(i < kvLeft->len && i < kvRight->len) {
            if (kvLeft->key[i] == kvRight->key[i]) {
                i++;
            } else return i-1;
        }
    } else {
        if (longFlagFromFlags(kvLeft->flags) == 1) {
            i = 0;
            while (i < kvRight->len && i < leafNode->prefixLen) {
                if (kvRight->key[i] == leafNode->prefix[i]) {
                    i++;
                } else return i-1;
            }
        } else {
            i = 0;
            while (i < kvRight->len && i < leafNode->prefixLen) {
                if (kvRight->key[i] == leafNode->prefix[i]) {
                    i++;
                } else return i-1;
            }
        }
    }
}


int bpLeafNodeSplit(bpTree *t, bpLeafNode *leafNode, unsigned char *key, uint64_t data) {
    bpLeafNode *newLeafLeft = bpLeafNodeNew();
    int id[MAX_LEAF_NODE_SIZE];
    int left[MAX_LEAF_NODE_SIZE];
    for(int i=0; i<leafNode->size; i++) {
        id[i] = i;
        left[i] = 0;
    }
    quickSort(leafNode, id, 0, leafNode->size-1);
    for(int i = 0; i < leafNode->size/2; i++) {
        left[id[i]] = 1;
    }
    // find the split key pos
    int split = id[leafNode->size/2];
    // get the split key
    int splitLeft = id[leafNode->size/2-1];;
    int splitRight = split;
    if(2 != leafNode->size) {
        splitRight = id[leafNode->size/2+1];
    }
    int splitLen = CommonPrefix(leafNode, splitLeft, splitRight);
    unsigned char *splitKey = malloc(KEY_MAX);
    memcpy(splitKey, leafNode->kv[leafNode->fp[split].pos].key, splitLen+1);
    // fill the leaf common prefix
    int newPrefixLen = CommonPrefix(leafNode, id[0], id[leafNode->size / 2 - 1]);
    if (newPrefixLen > 0) {
        newLeafLeft->prefixLen = newPrefixLen;
        memcpy(newLeafLeft->prefix, leafNode->kv[id[0]].key, newPrefixLen);
        newLeafLeft->prefix[newPrefixLen] = '\0';
    }
    int leafNodePrefixLen = CommonPrefix(leafNode, id[leafNode->size/2+1], id[leafNode->size-1]);
    if (leafNodePrefixLen > 0) {
        leafNode->prefixLen = leafNodePrefixLen;
        memcpy(newLeafLeft->prefix, leafNode->kv[id[leafNode->size/2+1]].key, leafNodePrefixLen);
        newLeafLeft->prefix[leafNodePrefixLen] = '\0';
    }

    // fill leaf Node
    unsigned char *insertKey = malloc(KEY_MAX);
    for(int i = 0; i < leafNode->size; i++) {
        if(1 == left[i]) { // fill left leafNode
            int l = getKey(leafNode, i, insertKey);
            bpLeafNodeInsert(t, newLeafLeft, insertKey, leafNode->kv[leafNode->fp[i].pos].value);
        } else {  // fill right leafNode
            _bpLeafNodeRemoveWithPos(t, leafNode, i);
        }
    }
    free(insertKey);
    _bpInterNodeInsertWithLeftChild(t, leafNode->parent, newLeafLeft, splitKey);
    free(splitKey);
}



int bpLeafNodeInsert(bpTree *t, bpLeafNode *leafNode, unsigned char *key, uint64_t data) {
    int success = 0;
    fpBinarySearch(t, leafNode, key, &success);
    if (success > 0) { // Already exists
        return -1; // TODO: Only need to change the value
    }
    _bpLeafNodeInsert(t, leafNode, key, data, -success-1);
}


int bitmapFindFirstZero(uint8_t bitmap[]) {
    int i = 0;
    int bits = sizeof(int);
    while(i < 8 * sizeof(int)) {
        if (bitmap[i/bits] & (0x1 << (i % bits))) {
            return i;
        }
        i++;
    }
    return -1;
}

int setBitmap(uint8_t bitmap[], int pos, int v) {
    int bits = sizeof(int);
    if (v) bitmap[pos / bits] |= (0x1 << (pos % bits));
    else bitmap[pos / bits] &= ~(0x1 << (pos % bits));
}

int _bpLeafNodeInsert(bpTree *t, bpLeafNode *leafNode, unsigned char *key, uint64_t data, int fpPos) {
    // determine if insert key have the common prefix
    uint8_t *s1 = key;
    int res = keyCompareWithMove(&s1, leafNode->prefix, strlen(s1), leafNode->prefixLen);

    // set the flag
    uint8_t flags = 0;
    if (res == 0) { // the insert key have the common prefix
        key = s1;
        // set the prefix flag
        flags |= 0b10000000;
    }

    // insert
    int pos = bitmapFindFirstZero(leafNode->bitmap);
    if(pos == -1) {
        exit(-1);
        return -1;
    }

    // set flags
    KV *kv = &leafNode->kv[pos];
    kv->flags = flags;

    int len = strlen(key);
    if (len < KEY_MAX_SIZE_IN_LN) {
        kv->len = len;
        memcpy(kv->key, key, len);
        kv->ptr = NULL;
    } else {
        kv->len = KEY_MAX_SIZE_IN_LN;
        memcpy(kv->key, key, len);
        kv->ptr = NULL; // need to handle;
    }

    kv->value = data;

    // insert the fingerprint
    // move the fingerprint to make it sorted
    for(int i = fpPos; i < leafNode->size; i++ ) {
        leafNode->fp[i+1] = leafNode->fp[i];
    }
    fp_t kf = bp_fp(key, strlen(key));
    fpAndOffset *fp = &leafNode->fp[fpPos];
    fp->fingerprint = kf;
    fp->pos = pos;
    if (leafNode->size + 1 >= MAX_LEAF_NODE_SIZE) {
        // TODO: need to split
        bpLeafNodeSplit(t, leafNode, key, data);
    }
    leafNode->size ++;
    setBitmap(leafNode->bitmap, pos, 1);
    return 0;
}



int bpLeafFindMax(bpLeafNode *leafNode) {
    if (leafNode->size == 0) return -1;
    if (leafNode->size < 1) return 0;
    int max = 0;
    for (int i = 0; i < leafNode->size; i++) {
        if (leafKeyCmp(leafNode, max, i) < 0) max = i;
    }
    return max;
}

int bpLeafFindMin(bpLeafNode *leafNode) {
    if (leafNode->size == 0) return -1;
    if (leafNode->size < 1) return 0;
    int min = 0;
    for (int i = 0; i < leafNode->size; i++) {
        if (leafKeyCmp(leafNode, min, i) > 0) min = i;
    }
    return min;
}

void bpLeafNodeShiftFromRight(bpTree *t, bpLeafNode *leafNode) {
    bpLeafNode *next = leafNode->next;
    if (next == NULL) return;;
    int min = leafNode->min;
    next->min = bpLeafFindMin(leafNode->next); // max must > 0
    unsigned char *key = malloc(KEY_MAX);
    if (key == NULL) return;
    int len = getKey(leafNode->next, min, key);
    bpLeafNodeInsert(t, leafNode, key, leafNode->kv[leafNode->fp[min].pos].value);
    _bpLeafNodeRemoveWithPos(t, leafNode->next, min);
    getKey(next, leafNode->min, key);
    bpInterNodeSet(t, leafNode->parent, leafNode->brothers, key);
    free(key);
}


void bpInterNodeSetChildAddr(bpInterNode *interNode, int pos, void *child) {
    uint8_t *childAddr = interChildAddr(interNode, pos);
    void **oral = (void **)childAddr;
    *oral = child;
    return;
}

void bpInterNodeSetOffset(bpInterNode *interNode, int pos, off_t offset) {
    uint8_t *offsetAddr = interOffsetAddr(interNode, pos);
    off_t *oral = (off_t *)offsetAddr;
    *oral = offset;
    return;
}


void bpInterNodeShiftFromRight(bpTree *t, bpInterNode *interNode) {
    bpInterNode *right = nextBrother(t, interNode);
    if (right == NULL) return;
    bpInterNode *parent = interNode->parent; // must != NULL
    // 将parent中对应的Key下降到当前节点中
    uint8_t *parentKeyAddr = interKeyAddr(parent, interNode->brothers);
    int parentKeyLen = interKeyLen(parent, interNode->brothers);
    uint8_t * rightKeyAddr = interKeyAddr(right, 0);
    uint8_t * rightChildAddr = interChildAddr(right, 0);
    int rightKeyLen = interKeyLen(right, 0);
    bpInterNodeMoveForNewKeyWithRightChild(t, interNode, interNode->size, parentKeyLen);
    // 填入Key
    memcpy(interKeyAddr(interNode, interNode->size - 1), parentKeyAddr, parentKeyLen);
    // 填入child
    uint8_t *childAddr = interChildAddr(interNode, interNode->size);
    memcpy(childAddr, rightChildAddr, sizeof(void *));
    // 将被移动的children的父节点设置为当前节点
    setParent(*(void **)childAddr, interNode);
    // 当前节点处理完毕

    // 处理parent
    uint8_t *moveBegin = parentKeyAddr + parentKeyLen;
    // 为新的key重新设置空间大小
    memmove(moveBegin + rightKeyLen - parentKeyLen, moveBegin, (uint8_t *)parent + parent->size - moveBegin + 1);
    memcpy(parentKeyAddr, rightKeyAddr, rightKeyLen);
    // 修改后面的offset
    bpInterNodeModifyOffset(parent, 1, rightKeyLen - parentKeyLen);
    // 修改parent的bytes
    parent->bytes += (rightKeyLen - parentKeyLen);
    parent->children += (rightKeyLen - parentKeyLen);

    // 处理右兄弟， 删除右兄弟的第一个Key以及其左孩子节点
    bpInterNodeRemoveLeftChild(t, right, 0);

    return; // 结束

}

void bpLeafNodeShiftFromLeft(bpTree *t, bpLeafNode *leafNode) {
    bpLeafNode *prev = leafNode->prev;
    int max = bpLeafFindMax(prev); // max must > 0
    unsigned char *key = malloc(KEY_MAX);
    if (key == NULL) return;
    int len = getKey(prev, max, key);
    bpLeafNodeInsert(t, leafNode, key, leafNode->kv[leafNode->fp[max].pos].value);
    _bpLeafNodeRemoveWithPos(t, prev, max);
    bpInterNodeSet(t, leafNode->parent, leafNode->brothers, key);
    free(key);
}


void bpInterNodeShiftFromLeft(bpTree *t, bpInterNode *interNode) {
    void **prevPtr = (void **)(interChildAddr(((bpInterNode *)(interNode->parent)), interNode->brothers + 1));
    bpInterNode *left = *prevPtr;
    bpInterNode *parent = interNode->parent; // must != NULL
    // 将parent中对应的Key下降到当前节点中
    int pos = interNode->brothers - 1;
    uint8_t *parentKeyAddr = interKeyAddr(parent, pos);
    int parentKeyLen = interKeyLen(parent, pos);
    bpInterNodeMoveForNewKeyWithLeftChild(t, interNode, 0, parentKeyLen);
    // 填入Key
    memcpy(interKeyAddr(interNode, 0), parentKeyAddr, parentKeyLen);
    // 填入child
    uint8_t *leftChildAddr = interChildAddr(left, left->size);
    memcpy(interChildAddr(interNode, 0), leftChildAddr, sizeof(void *));
    // 将被移动的children的父节点设置为当前节点
    uint8_t *childAddr = interChildAddr(interNode, 0);
    setParent(*(void **)childAddr, interNode);
    // 当前节点处理完毕

    // 处理parent
    uint8_t *leftKeyAddr = interKeyAddr(left, left->size - 1);
    int leftKeyLen = interKeyLen(left, left->size - 1);
    uint8_t *moveBegin = parentKeyAddr + parentKeyLen;
    // 为新的key重新设置空间大小
    memmove(moveBegin + leftKeyLen - parentKeyLen, moveBegin, (uint8_t *)parent + parent->size - moveBegin + 1);
    memcpy(parentKeyAddr, leftKeyAddr, leftKeyLen);
    // 修改后面的offset
    bpInterNodeModifyOffset(parent, pos + 1, leftKeyLen - parentKeyLen);
    // 修改parent的bytes
    parent->bytes += (leftKeyLen - parentKeyLen);
    parent->children += (leftKeyLen - parentKeyLen);

    // 处理左兄弟， 删除左兄弟的最后一个Key以及其右孩子节点
    bpInterNodeRemoveRightChild(t, left, left->size - 1);

    return; // 结束

}

int bpLeafNodeMergeFromLeftOrRight(bpTree *t, bpLeafNode *leafNode) {
    bpLeafNode *left = leafNode->prev;
    bpLeafNode *right = leafNode->next;
    if (NULL == left && NULL == right) return 0;
    if (left != NULL) {
        if (right == NULL || right->size > left->size ) return -1;
    }
    return 1;
}

int bpInterNodeMergeFromLeftOrRight(bpTree *t, bpInterNode *interNode) {
    bpInterNode *left = prevBrother(t, interNode);
    bpInterNode *right = nextBrother(t, interNode);
    if (NULL == left && NULL == right) return 0;
    if (left != NULL) {
        if (right == NULL || right->size > left->size ) return -1;
    }
    return 1;
}

int bpLeafNodeShiftFromLeftOrRight(bpTree *t, bpLeafNode *leafNode) {
    bpLeafNode *left = leafNode->prev;
    bpLeafNode *right = leafNode->next;
    if (NULL == left && NULL == right) return 0;
    if (left != NULL && left->size > LEAF_MIN_SIZE + 1) {
        if (right == NULL || right->size <= left->size ) return -1;
        else return 1;
    } else {
        if(right != NULL && right->size > LEAF_MIN_SIZE + 1) return 1;
    }
    return 0;
}

int bpInterNodeShiftFromLeftOrRight(bpTree *t, bpInterNode *interNode) {
    bpInterNode *left = prevBrother(t, interNode);
    bpInterNode *right = nextBrother(t, interNode);
    if (NULL == left && NULL == right) return 0;
    if (left != NULL && left->size > MIN_INTER_NODE_SIZE + 1) {
        if (right == NULL || right->size <= left->size ) return -1;
        else return 1;
    } else {
        if(right != NULL && right->size > MIN_INTER_NODE_SIZE + 1) return 1;
    }
    return 0;
}


int _bpLeafNodeMergeWithRight(bpTree *t, bpLeafNode *leafNode) {
    bpLeafNode *right = leafNode->next;
    if (right == NULL) exit(-1);
    unsigned char *key = malloc(KEY_MAX);
    memset(key, 0, KEY_MAX);
    for (int i = 0; i < leafNode->size; i++) {
        int len = getKey(leafNode, i, key);
        bpLeafNodeInsert(t, right, key, leafNode->kv[leafNode->fp[i].pos].value);
    }
    // remove the left
    bpInterNodeRemoveLeftChild(t, leafNode->parent, leafNode->brothers);
    right->prev = leafNode->prev;
    if (NULL != leafNode->prev) {
        ((bpLeafNode *)(leafNode->prev))->next = right;
    }
    right->brothers --;
    bpLeafNode *next = right->next;
    while(next != NULL) {
        next->brothers --;
        next = next->next;
    }
    free(key);
    free(leafNode);
    return 0;
}

int _bpLeafNodeMergeWithLeft(bpTree *t, bpLeafNode *leafNode) {
    bpLeafNode *left = leafNode->prev;
    if (left == NULL) exit(-1);
    unsigned char *key = malloc(KEY_MAX);
    memset(key, 0, KEY_MAX);
    for (int i = 0; i < leafNode->size; i++) {
        int len = getKey(leafNode, i, key);
        bpLeafNodeInsert(t, left, key, leafNode->kv[leafNode->fp[i].pos].value);
    }
    // remove the left
    bpInterNodeRemoveLeftChild(t, leafNode->parent, left->brothers);
    leafNode->prev = left->prev;
    if (leafNode->prev != NULL) {
        ((bpLeafNode *)(leafNode->prev))->next = leafNode;
    }
    leafNode->brothers --;
    bpLeafNode *next = next->next;
    while(next != NULL) {
        next->brothers --;
        next = next->next;
    }
    free(key);
    free(left);
    return 0;
}

int bpLeafNodeTryMerge(bpTree *t, bpLeafNode *leafNode) {
    if(leafNode->size >= LEAF_MIN_SIZE) return -1;
    int shift = bpLeafNodeShiftFromLeftOrRight(t, leafNode);
    if (shift > 0) {
        bpLeafNodeShiftFromRight(t, leafNode);
    } else if (shift < 0) {
        bpLeafNodeShiftFromLeft(t, leafNode);
    } else {
        int res = bpLeafNodeMergeFromLeftOrRight(t, leafNode);
        if (1 == res) {
            _bpLeafNodeMergeWithRight(t, leafNode);
        } else if (-1 == res) {
            _bpLeafNodeMergeWithLeft(t, leafNode);
        }
    }
    return 0;
}

bpInterNode *nextBrother(bpTree *t, bpInterNode *interNode) {
    bpInterNode *parent = interNode->parent;
    void **nextChild = (void **)(interChildAddr(parent, interNode->brothers + 1));
    return (bpInterNode *)(*nextChild);
}

bpInterNode *prevBrother(bpTree *t, bpInterNode *interNode) {
    bpInterNode *parent = interNode->parent;
    void **prevChild = (void **)(interChildAddr(parent, interNode->brothers - 1));
    return (bpInterNode *)(*prevChild);
}

void bpInterNodeSetChildrenBrothers(bpInterNode *interNode, int pos, int v) {
    uint8_t *childAddr = interChildAddr(interNode, pos);
    bpInterNode **begin = (bpInterNode **)childAddr;
    while (pos <= interNode->size) {
        (*begin)->brothers = v;
        v++;
        pos++;
        begin++;
    }
    return;
}

void _bpInterNodeMerge(bpTree *t, bpInterNode *left, bpInterNode *right) {
    if (left == NULL || right == NULL) return;
    bpInterNode *parent = left->parent;
    int pos = left->brothers;
    uint8_t *parentKeyAddr = interKeyAddr(parent, pos);
    uint8_t parentKeyLen = interKeyLen(parent, pos);
    int rightKeysLen = right->children - right->keys;
    int totalSize = right->size + 1;
    int oralKeysLen = left->children - left->keys;
    // don't need to move offset
    // move keys and children
    uint8_t *childMoveBegin = interChildrenAddr(left);
    uint8_t *childMoveTo = interChildrenAddr(left) + rightKeysLen + parentKeyLen;
    int childMoveLen = sizeof(void *) * (left->size + 1);
    uint8_t *keyMoveBegin = interKeysAddr(left);
    uint8_t *keyMoveTo = interKeysAddr(left) + totalSize * sizeof(off_t);
    int keyMoveLen = left->children - left->keys;
    memmove(childMoveTo, childMoveBegin, childMoveLen);
    memmove(keyMoveTo, keyMoveBegin, keyMoveLen);

    // set the meta
    left->keys += totalSize * sizeof(off_t);
    left->children += totalSize * sizeof(off_t) + rightKeysLen + parentKeyLen;
    left->size += totalSize;
    left->bytes += totalSize * (sizeof(void *) + sizeof(off_t)) + rightKeysLen + parentKeyLen;

    int oralSize = left->size;
    // insert offset
    uint8_t *offsetAddr = interOffsetAddr(left, oralSize);
    off_t *offsetBegin = (off_t *)(offsetAddr);
    off_t *rightOffsetBegin = (off_t *)right->data;
    *offsetBegin = oralKeysLen;
    for (int i = 0; i<right->size; i++) {
        offsetBegin ++;
        *offsetBegin = *rightOffsetBegin + oralKeysLen + parentKeyLen;
        rightOffsetBegin ++;
    }

    // insert key from parent
    memcpy(interKeyAddr(left, oralSize), parentKeyAddr, parentKeyLen);
    // insert key from right
    memcpy(interKeyAddr(left, oralSize + 1), interKeysAddr(right), rightKeysLen);

    // insert child from right
    setChildrenParent(right, left);
    memcpy(interChildAddr(left, oralSize + 1), interChildrenAddr(right), totalSize * sizeof(void *) + 1);

    // delete the parent key
    bpInterNodeRemoveRightChild(t, parent, left->brothers);

    bpInterNodeFree(right);

    // 将parent 第pos个之后孩子节点的brothers - 1, 重新设置brothers
    bpInterNodeSetChildrenBrothers(left, oralSize + 1, oralSize + 1);
    return;
}

void _bpInterNodeMergeWithRight(bpTree *t, bpInterNode *interNode) {
    bpInterNode *left = interNode;
    bpInterNode *right = nextBrother(t, interNode);
    _bpInterNodeMerge(t, left, right);
    return;
}

void _bpInterNodeMergeWithLeft(bpTree *t, bpInterNode *interNode) {
    if (interNode == NULL) return;
    bpInterNode *left = prevBrother(t, interNode);
    bpInterNode *right = interNode;
    _bpInterNodeMerge(t, left, right);
    return;
}


int bpInterNodeTryMerge(bpTree *t, bpInterNode *interNode) {
    if (interNode->size >= INTER_MIN_SIZE) return -1;
    int shift = bpInterNodeShiftFromLeftOrRight(t, interNode);
    if (shift > 0) {
        bpInterNodeShiftFromRight(t, interNode);
    } else if (shift < 0) {
        bpInterNodeShiftFromLeft(t, interNode);
    } else {
        int res = bpInterNodeMergeFromLeftOrRight(t, interNode);
        if (1 == res) {
            _bpInterNodeMergeWithRight(t, interNode);
        } else if (-1 == res) {
            _bpInterNodeMergeWithLeft(t, interNode);
        }
    }
    return 0;
}


void bpLeafNodeFpRemove(bpLeafNode *leafNode, int pos) {
    for(int i = pos; i < leafNode->size-1; i++) {
        leafNode->fp[pos] = leafNode->fp[pos+1];
    }
    return ;
}

int _bpLeafNodeRemoveWithPos(bpTree *t, bpLeafNode *leafNode, int pos) {
    if (pos < 0 || pos >= leafNode->size) return -1;
    fpAndOffset *fp = &leafNode->fp[pos];
    setBitmap(leafNode->bitmap, fp->pos, 0);
    bpLeafNodeFpRemove(leafNode, pos);
    leafNode->size --;
    return 1;
}

int _bpLeafNodeRemove(bpTree *t, bpLeafNode *leafNode, unsigned char *key) {
    int success = 0;
    fpBinarySearch(t, leafNode, key, &success);
    if (success < 0) { // not exists
        return -1; //
    }
    int pos = success;
    _bpLeafNodeRemoveWithPos(t, leafNode, pos);
    bpLeafNodeTryMerge(t, leafNode);
    return 0;
}


int bpRemove(bpTree *t, unsigned char *key) {
    if (key == NULL || 0 == strlen(key) || t == NULL) return -1;
    void *h = t->header;
    int l = t->level;
    if (l == 0 || h == NULL) return -1;
    if (l == 1) {
        return _bpLeafNodeRemove(t, h, key);
    }
    bpLeafNode *ln = bpInterNodeFind(t, h, key);
    return _bpLeafNodeRemove(t ,ln, key);
}

void bpInterNodeFree(bpInterNode *interNode) {
    if (interNode == NULL) return;
    bp_free(interNode);
}