//
// Created by ganquan on 2020/4/16.
//
#include "bplustree.h"
#include "bp_malloc.h"
#include "murmurhash3.h"
#include "util.h"



//#define interKeyLen(n, i) ((i == n->size - 1) ? (n->children - n->keys) : *(off_t *)(n->data + (i+1) * sizeof(off_t))) - *(off_t *)(n->data + i * sizeof(off_t))
//#define leafKeyLen(addr) *(uint32_t *)((uint8_t *)addr+FLAGS_LEN)
//#define interKeyOffset(n, i) *(off_t *)(n->data + i * sizeof(off_t))
//#define interKeyAddr(n, i) n->data + n->keys + interKeyOffset(n, i)
//#define interChildAddr(n, i) n->data + n->children + sizeof(void *) * i
//#define interOffsetAddr(n, i) n->data + sizeof(off_t) * i
//#define interKeysAddr(n) n->data + n->keys
//#define interChildrenAddr(n) n->data + n->children
//#define leafFpsAddr(n) n->data + PREFIX_LEN
//#define keyFpAddr(n, i) leafFpsAddr(n) + i * (FP_OFFSET_LEN)
//#define leafKeysAddr(n) n->data + PREFIX_LEN + LEAF_NODE_SIZE * (FP_OFFSET_LEN)
//#define leafKeyOffset(n, i) *((off_t *) (keyFpAddr(leafNode, i) + sizeof(fp_t)))
//#define leafKeyAddr(n, i) leafKeysAddr(n) + leafKeyOffset(n, i)
//#define prefixFlag(addr) (*(uint8_t *)addr) >> 7
//#define longFlag(addr) (*((uint8_t *)addr)) << 1 >> 7
//#define prefixFlagFromFlags(flags) flags >> 7
//#define longFlagFromFlags(flags) flags << 1 >> 7
//#define valueAddr(addr) ((uint8_t *)addr + FLAGS_LEN + sizeof(uint32_t) + leafKeyLenInNode(addr) + sizeof(addr_t))

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
    bpInterNode *node = bp_malloc(4096);
    if (node == NULL) return NULL;
    node->type = INTER_NODE;
    node->bytes = INTER_NODE_HEADER_LEN;
    node->size = 0;
    node->keys = node->children = 0;
    node->parent = NULL;
    return node;
}

bpLeafNode *bpLeafNodeNew(void) {
    bpLeafNode *ln = bp_malloc(sizeof(*ln));
    if (ln == NULL) return NULL;
    ln->type = LEAF_NODE;
    ln->bytes = LEAF_NODE_HEADER_LEN;
    ln->size = 0;
    ln->prefixLen = 0;
    ln->prev = ln->next = ln->parent = NULL;
    memset(ln->bitmap, 0, 8);
    return ln;
}

uint64_t bpFind(bpTree *t, unsigned char *key, int len) {
    if (key == NULL || 0 == strlen(key) || t == NULL) return -1;
    void *h = t->header;
    int l = t->level;
    if (l == 0 || h == NULL) return -1;
    if (l == 1) {
        return bpLeafNodeFind(t, h, key, len);
    }
    else {
        bpLeafNode *ln = bpInterNodeFind(t, h, key, len);
        return bpLeafNodeFind(t, ln, key, len);
    }
}

// find the leafNode
void *bpInterNodeFind(bpTree *t, bpInterNode *interNode, unsigned char *key, int len) {
    if (interNode == NULL) return NULL;
    size_t size = interNode->size;
    uint8_t type = interNode->type;
    if (type != INTER_NODE || size == 0) return NULL;
    int child = OffBinarySearch(t, interNode, key, len);
    void **next = (void **)(interChildAddr(interNode, child));
    if (*(uint8_t *)(*next) == INTER_NODE) {
        return bpInterNodeFind(t, (bpInterNode *)(*next), key, len);
    } else return (*next);
}


int OffBinarySearch(bpTree *t, bpInterNode *interNode, unsigned char *key, int len) {
    int low = -1;
    int size = interNode->size;
    int high = size;
    // off_t offset = interNode->offset;
    while (low + 1 < high) {
        int mid = low + (high - low)/2;
        off_t offset = interKeyOffset(interNode, mid);
        unsigned char *keyAddr = interNode->data + interNode->keys + interKeyOffset(interNode, mid);// TODO: handle long key
        if (1 != keyCompare(keyAddr + 1, (uint8_t *)key, interKeyLen(interNode, mid) - 1, len)) {
            low = mid;
        } else {
            high = mid;
        }
    }
    return high;
}

// set children's parent
void setChildrenParent(bpInterNode *interNode, bpInterNode *parent) {
    void **childAddr = (void **)(interChildrenAddr(interNode));
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
    uint8_t *offsetAddr = interOffsetAddr(interNode, pos);
    uint8_t *childAddr = interChildAddr(interNode, (pos + 1));
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
    if (interNode->size == 0) {
        void **childAddr = (void **)(interChildrenAddr(interNode));
        void *node = (bpInterNode *)*childAddr;
        // printInterNode(node);
        t->header = node;
        setParent(node, NULL);
    }
    else bpInterNodeTryMerge(t, interNode);
}

// 删除中间节点中的第pos个key以及一个孩子节点
int bpInterNodeRemoveLeftChild(bpTree *t, bpInterNode *interNode, int pos) {
    uint8_t *keyAddr = interKeyAddr(interNode, pos);
    uint8_t *offsetAddr = interOffsetAddr(interNode, pos);
    uint8_t *childAddr = interChildAddr(interNode, pos);
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
    if (interNode->size == 0) {
        void **childAddr = (void **)(interChildrenAddr(interNode));
        void *node = *childAddr;
        t->header = node;
    }
    else bpInterNodeTryMerge(t, interNode);
}


int bpInterNodeSet(bpTree *t, bpInterNode *interNode, unsigned char *key, int len, int leftOrright) {
    int pos = OffBinarySearch(t, interNode, key, len);
    if (leftOrright == 1) {
        pos --;
    }
    uint8_t *data = interNode->data;
    uint8_t *keyAddr = interKeyAddr(interNode, pos);
    int oralLen = interKeyLen(interNode, pos);
    uint8_t *moveBegin = keyAddr + oralLen;
    int keyLen = len; // TODO: handle long key
    if(keyLen > KEY_MAX_SIZE_IN_IN) keyLen = KEY_MAX_SIZE_IN_IN + sizeof(void *) + 1;
    else keyLen += 1;
    int moveLen = (uint8_t *)interNode + interNode->bytes - moveBegin;
    memmove(moveBegin + keyLen - oralLen, moveBegin,  moveLen + 1);
    if(keyLen == KEY_MAX_SIZE_IN_IN + sizeof(void *) + 1) {
        *keyAddr = 1;
        memcpy(keyAddr+1, key, KEY_MAX_SIZE_IN_IN);
    } else {
        *keyAddr = 0;
        memcpy(keyAddr+1, key, keyLen - 1);
    }
    bpInterNodeModifyOffset(interNode, pos+1, keyLen - oralLen);
    interNode->children += (keyLen - oralLen);
    interNode->bytes += (keyLen - oralLen);
    return 0;
}

void bpInterNodeInsert(bpTree *t, bpInterNode *interNode, void *left, void *right, unsigned char *key, int LeftOrRight, int len) {
        if (interNode == NULL) {    // need to malloc a new interNode
        bpInterNode *newHeader = bpInterNodeNew();
        // set offset
        *(off_t *)newHeader->data = 0;

        int keyLen = len;
        if(keyLen > KEY_MAX_SIZE_IN_IN) keyLen = KEY_MAX_SIZE_IN_IN + sizeof(void *);
        keyLen += 1; // for flag

        int totalLen = sizeof(void *) * 2 + sizeof(off_t) + keyLen;
        newHeader->size ++;
        newHeader->bytes += totalLen;
        newHeader->keys += sizeof(off_t);
        newHeader->children += sizeof(off_t) + keyLen;

        uint8_t *data = newHeader->data;
        uint8_t *keyAddr = data + newHeader->keys;
        // set the flag and key
        if(keyLen == KEY_MAX_SIZE_IN_IN + sizeof(void *) + 1) {
            *keyAddr = 1;
            keyAddr ++;
            memcpy(keyAddr, key, KEY_MAX_SIZE_IN_IN);
        } else {
            *keyAddr = 0;
            keyAddr ++;
            memcpy(keyAddr, key, keyLen - 1);
        }
//        // set the children
        uint8_t *childAddr = interChildrenAddr(newHeader);
        void **childBegin = (void **)childAddr;
        *(childBegin) = left;
        *(childBegin + 1) = right;
//
        setChildrenParent(newHeader, newHeader);
        // bpInterNodeSetChildrenBrothers(newHeader, 0, 0);
        t -> level ++;
        t -> header = newHeader;
        return;
    } else if (-1 == LeftOrRight) {
        _bpInterNodeInsertWithLeftChild(t, interNode, left, key, len);
    } else if (1 == LeftOrRight) {
        _bpInterNodeInsertWithRightChild(t, interNode, right, key, len);
    }
    return ;
}


int bpInterNodeSplit(bpTree *t, bpInterNode *interNode) {
    int split = interNode->size/2;
    unsigned char *splitKey = malloc(KEY_MAX_SIZE_IN_IN);
    if(splitKey == NULL) return -1;
    int splitKeyLen = interKeyLen(interNode, split) - 1;
    memcpy(splitKey, interKeyAddr(interNode,split) + 1, splitKeyLen);// TODO: handle long key
    bpInterNode *newNode = bpInterNodeNew();

    int keyOffset = interKeyOffset(interNode, (split + 1));
    unsigned char *keyAddr = interKeysAddr(interNode) + keyOffset;
    uint8_t *offsetAddr = interOffsetAddr(interNode, split);
    uint8_t *childAddr = interChildAddr(interNode, split);

    // handle newNode
    newNode->keys = newNode->children = sizeof(off_t) * split;
    memcpy(newNode->data, interNode->data, newNode->keys);// set offset
    int keyMoveLen = interKeyOffset(interNode, split);
    memcpy(interKeysAddr(newNode), interKeysAddr(interNode), keyMoveLen);
    newNode->children += keyMoveLen;
    int childrenMoveLen = sizeof(void *) * (split+1);
    memcpy(interChildrenAddr(newNode), interChildrenAddr(interNode), childrenMoveLen);
    newNode->size = split;
    newNode->bytes = newNode->children + childrenMoveLen;

    // handle the original interNode
    int leftKeyLen = interNode->children - interNode->keys -  (interKeyOffset(interNode, (split + 1)));
    int leftChildrenLen = sizeof(void *) * (interNode->size - split);
    int deleteKeyLen = interKeyOffset(interNode, (split + 1));
    int deleteOffsetLen = sizeof(off_t) * (split + 1);
    int deleteChildrenLen = sizeof(void *) * (split + 1);
    unsigned char *oldChildrenBegin = interChildAddr(interNode, (split + 1));
    uint8_t *oldKeysBegin = interKeyAddr(interNode, (split + 1));
    interNode->keys -= (deleteOffsetLen);
    interNode->children -= (deleteOffsetLen + deleteKeyLen);

    // move the offset
    memmove(interNode->data, offsetAddr + sizeof(off_t), (interNode->size - split - 1) * sizeof(off_t));
    // modify the offset
    bpInterNodeModifyOffset(interNode, 0, -deleteKeyLen);
    // move the keys
    memmove(interKeysAddr(interNode), oldKeysBegin, leftKeyLen);
    // move the children
    memmove(interChildrenAddr(interNode), oldChildrenBegin, leftChildrenLen + 1);

    interNode->size -= (split + 1);
    interNode->bytes -= (deleteOffsetLen + deleteKeyLen + deleteChildrenLen);
    unsigned char *test = interKeyAddr(interNode, 0);
    // set parent for the children of new node
    setChildrenParent(newNode, newNode);

    bpInterNodeInsert(t, interNode->parent, newNode, interNode, splitKey, -1, splitKeyLen);
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
        begin += 1;
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
    uint8_t *date = interNode->data;
    uint8_t *keyAddr = interChildrenAddr(interNode);
    uint8_t *offsetAddr = interOffsetAddr(interNode, pos);
    uint8_t *childAddr = interChildAddr(interNode, pos);
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
    int offset = keyAddr - (interKeysAddr(interNode));
    *(off_t *)offsetAddr = offset;
    bpInterNodeSetMeta(t, interNode, len);
    bpInterNodeModifyOffset(interNode, pos + 1, len);
}


void bpInterNodeMoveForNewKeyWithRightChild(bpTree *t, bpInterNode *interNode, int pos, int len) {
    uint8_t *keyAddr = interChildrenAddr(interNode);
    uint8_t *offsetAddr = interOffsetAddr(interNode, pos);
    uint8_t *childAddr = interChildAddr(interNode, pos + 1);
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


int _bpInterNodeInsertWithLeftChild(bpTree *t, bpInterNode *interNode, void *child, unsigned char *key, int len) {
    if(interNode == NULL) {
        exit(-1);
    }

    // set parent
    setParent(child, interNode);

    int pos = OffBinarySearch(t, interNode, key, len);

    // 移动以获得空间
    int keyLen = len;
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

    // bpInterNodeSetChildrenBrothers(interNode, pos, )

    if (interNode->size + 1 >= MAX_INTER_NODE_SIZE || interNode->bytes + KEY_MAX_SIZE_IN_LN > MAX_INTER_NODE_BYTES) {
        // unsigned char *key = interKeyAddr(interNode, 2);
        // printInterNode(interNode);
        bpInterNodeSplit(t, interNode);
    }

    return 0;
}

int _bpInterNodeInsertWithRightChild(bpTree *t, bpInterNode *interNode, void *child, unsigned char *key, int len) {
    if(interNode == NULL) {
        exit(-1);
    }

    // set parent
    setParent(child, interNode);

    int pos = OffBinarySearch(t, interNode, key, len);

    // 移动以获得空间
    int keyLen = len;
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

uint64_t bpLeafNodeFind(bpTree *t, bpLeafNode *leafNode, unsigned char *key, int len) {
    if (leafNode->type != LEAF_NODE) return -1;
    int success = 0;
    fpBinarySearch(t, leafNode, key, &success, len);
    if (success >= 0) { // TODO: handle the hash conflict
        return leafNode->kv[leafNode->fp[success].pos].value;
    }
    return -1;
}

int leafCompareWithKey(bpTree *t, bpLeafNode *leafNode, unsigned char *key, KV *kv, int len) {
    uint8_t *s1 = key;
    //uint8_t *s2 = keyAddr;
    int b = 0;
    int res = 0;
    if (1 == prefixFlagFromFlags(kv->flags)) {
        res = keyCompareWithMove(&s1, leafNode->prefix, len, leafNode->prefixLen);
        len -= leafNode->prefixLen;
        if (res != 0) {
            return res;
        }
    }
    res = keyCompareWithMove(&s1, kv->key, len, kv->len);
    len -= kv->len;
    if (res != 0) {
        return res;
    }
    if (longFlagFromFlags(kv->flags) == 1) { // TODO: handle long key, keyAddr already changed

    }
    // matched, return the value
    if (len == 0) {
        return 0;
    } else {
        return 1;
    }
}

int handleHashConflict(bpTree *t, bpLeafNode *leafNode, unsigned char *key, fp_t fp, int high, int len) {
    while((++(high)) <= leafNode->size) {
        fp_t kf = leafNode->fp[high].fingerprint;
        if(kf != fp) return 0;
        int pos = leafNode->fp[high].pos;
        KV *kv = &leafNode->kv[pos];
        if(leafCompareWithKey(t, leafNode, key, kv, len) == 0) {
            return high;
        }
    }
    return 0;
}

void fpBinarySearch(bpTree *t, bpLeafNode *leafNode, unsigned char *key, int *success, int len) {
    uint8_t fp = getHash(key, len);
    int low = -1;
    int size = leafNode->size;
    int high = size;
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
    if (high >= size || leafNode->fp[high].fingerprint != fp) {
        *success = -high-1;
    } else { // maybe need to handle the hash conflict
        if(leafCompareWithKey(t, leafNode, key, kv, len) != 0) {
            int res = handleHashConflict(t, leafNode, key, fp, high, len);
            if(res == 0) {
                *success = -high-1;
            }
            else *success = res;
            return;
        }
        *success = high;
    }
    return ; // TODO: need to delete
}

int bpInsert(bpTree *t, unsigned char *key, uint64_t data, int len) {
    if (key == NULL || 0 == len || t == NULL) return -1;
    void *h = t->header;
    int l = t->level;
    if (l == 0 || h == NULL) return -1;
    if (l == 1) {
        return bpLeafNodeInsert(t, h, key, data, len);
    }
    bpLeafNode *ln = bpInterNodeFind(t, h, key, len);
    return bpLeafNodeInsert(t ,ln, key, data, len);
}

int getKey(bpLeafNode *leafNode, int id, unsigned char *data) {
    memset(data, 0, KEY_MAX);
    KV *kv = &leafNode->kv[leafNode->fp[id].pos];
    int l=0;
    if (prefixFlagFromFlags(kv->flags) == 1) {
        memcpy(data, leafNode->prefix, leafNode->prefixLen);
        l += leafNode->prefixLen;
        // TODO:handle long key
    }
    memcpy(data+l, kv->key, kv->len);
    l += kv->len;
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
        if(leafKeyCmp(leafNode, arr[i], arr[pivot]) == -1) {
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
    KV *kvLeft = &leafNode->kv[leafNode->fp[left].pos];
    KV *kvRight = &leafNode->kv[leafNode->fp[right].pos];
    int i = 0;
    if(prefixFlagFromFlags(kvLeft->flags) == prefixFlagFromFlags(kvRight->flags)) {
        while(i < kvLeft->len && i < kvRight->len) {
            if (kvLeft->key[i] == kvRight->key[i]) {
                i++;
            } else return i;
        }
    } else {
        if (prefixFlagFromFlags(kvLeft->flags) == 1) {
            i = 0;
            while (i < kvRight->len && i < leafNode->prefixLen) {
                if (kvRight->key[i] == leafNode->prefix[i]) {
                    i++;
                } else return i;
            }
        } else {
            i = 0;
            while (i < kvRight->len && i < leafNode->prefixLen) {
                if (kvRight->key[i] == leafNode->prefix[i]) {
                    i++;
                } else return i;
            }
        }
    }
}


void bpLeafNodeSplit(bpTree *t, bpLeafNode *leafNode, unsigned char *key, uint64_t data, int len) {
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
    // 这里只需要寻找split和前一个Key的公共前缀即可
//    if(2 != leafNode->size) {
//        splitRight = id[leafNode->size/2+1];
//    }
    int splitLen = CommonPrefix(leafNode, splitLeft, splitRight);
    splitLen ++;
    unsigned char *splitKey = malloc(KEY_MAX);
    // TODO: delete begin
    int pos = leafNode->fp[split].pos;
    KV *kv = &leafNode->kv[pos];
    memcpy(splitKey, kv->key, splitLen);
    splitKey[splitLen + 1] = '\0';
    // fill the leaf common prefix
//    int newPrefixLen = CommonPrefix(leafNode, id[0], id[leafNode->size / 2 - 1]);
//    if (newPrefixLen > 0) {
//        newLeafLeft->prefixLen = newPrefixLen;
//        memcpy(newLeafLeft->prefix, leafNode->kv[id[0]].key, newPrefixLen);
//        newLeafLeft->prefix[newPrefixLen] = '\0';
//    }
//    int leafNodePrefixLen = CommonPrefix(leafNode, id[leafNode->size/2+1], id[leafNode->size-1]);
//    if (leafNodePrefixLen > 0) {
//        leafNode->prefixLen = leafNodePrefixLen;
//        memcpy(leafNode->prefix, leafNode->kv[id[leafNode->size/2+1]].key, leafNodePrefixLen);
//        leafNode->prefix[leafNodePrefixLen] = '\0';
//    }

    // fill leaf Node
    // _bpLeafNodeRemoveWithPos(t, leafNode, split);
    // leafNode->size ++; // 先把size恢复，否则在后续的删除中会出现定位错误
    unsigned char *insertKey = malloc(KEY_MAX);
    for(int i = 0; i < leafNode->size; i++) {
        if(1 == left[i] && i != split) { // fill left leafNode
            int l = getKey(leafNode, i, insertKey);
            bpLeafNodeInsert(t, newLeafLeft, insertKey, leafNode->kv[leafNode->fp[i].pos].value, l);
        } else {  // fill right leafNode
        }
    }
    // printf("split insert over!\n");
    int delete = 0;
    int oralSize = leafNode->size;
    for (int i = 0; i < oralSize; i++) {
        if (left[i] == 1) {  // delete
            _bpLeafNodeRemoveWithPos(t, leafNode, i - delete);
            delete ++;
        }
    }
    newLeafLeft ->prev = leafNode -> prev;
    newLeafLeft -> next = leafNode;
    if (leafNode -> prev) {
        ((bpLeafNode *)(leafNode->prev))->next = newLeafLeft;
    }
    leafNode -> prev = newLeafLeft;
    bp_free(insertKey);
    bpInterNodeInsert(t, leafNode->parent, newLeafLeft, leafNode, splitKey, -1, splitLen);
    if(splitKey != NULL) bp_free(splitKey);
    return ;
}



int bpLeafNodeInsert(bpTree *t, bpLeafNode *leafNode, unsigned char *key, uint64_t data, int len) {
    int success = 0;
    fpBinarySearch(t, leafNode, key, &success, len);
    if (success >= 0) { // Already exists
        return -1; // TODO: Only need to change the value
    }
    _bpLeafNodeInsert(t, leafNode, key, data, -success-1, len);
}


int bitmapFindFirstZero(uint8_t bitmap[]) {
    int i = 0;
    int bits = 8;
    while(i < 8 * sizeof(int)) {
        if (!(bitmap[i/bits] & (0x1 << (i % bits)))) {
            // printf("i = %d\n", i);
            return i;
        }
        i++;
    }
    return -1;
}

int setBitmap(uint8_t bitmap[], int pos, int v) {
    int bits = 8;
    if (v) bitmap[pos / bits] |= (0x1 << (pos % bits));
    else bitmap[pos / bits] &= ~(0x1 << (pos % bits));
}

int _bpLeafNodeInsert(bpTree *t, bpLeafNode *leafNode, unsigned char *key, uint64_t data, int fpPos, int len) {
    // determine if insert key have the common prefix
    uint8_t *s1 = key;
    int res = keyContain(s1, leafNode->prefix, len, leafNode->prefixLen);

    // set the flag
    uint8_t flags = 0;
    if (res == 1) { // the insert key have the common prefix
        key = key + leafNode->prefixLen;
        len = len - leafNode->prefixLen;
        // set the prefix flag
        flags |= 0b10000000;
    }

    // insert
    int pos = bitmapFindFirstZero(leafNode->bitmap);
    if(pos == -1) {
        printf("exit");
        // exit(-1);
        return -1;
    }

    // set flags
    KV *kv = &leafNode->kv[pos];
    kv->flags = flags;

    if (len < KEY_MAX_SIZE_IN_LN) {
        kv->len = len;
        memcpy(kv->key, key, len+1);
        kv->ptr = NULL;
    } else {
        kv->len = KEY_MAX_SIZE_IN_LN;
        memcpy(kv->key, key, len+1);
        kv->ptr = NULL; // need to handle;
    }

    kv->value = data;

    // insert the fingerprint
    // move the fingerprint to make it sorted
    for(int i = leafNode->size - 1; i >= fpPos; i-- ) {
        leafNode->fp[i+1] = leafNode->fp[i];
    }
    fp_t kf = bp_fp(key, len);
    fpAndOffset *fp = &leafNode->fp[fpPos];
    fp->fingerprint = kf;
    fp->pos = pos;
    leafNode->size += 1;
    setBitmap(leafNode->bitmap, pos, 1);
    if (leafNode->size >= MAX_LEAF_NODE_SIZE) {
        printf("split\n");
        bpLeafNodeSplit(t, leafNode, key, data, len);
    }
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

int bpLeafFindMaxExceptMax(bpLeafNode *leafNode, int pos) {
    if (leafNode->size == 0) return -1;
    if (leafNode->size < 1) return 0;
    int max = 0;
    if (pos == max) max = 1;
    for (int i = 0; i < leafNode->size; i++) {
        if (i != pos && leafKeyCmp(leafNode, max, i) < 0) max = i;
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

int bpLeafFindMinExceptMin(bpLeafNode *leafNode, int pos) {
    if (leafNode->size == 0) return -1;
    if (leafNode->size < 1) return 0;
    int min = 0;
    if (pos == min) min = 1;
    for (int i = 0; i < leafNode->size; i++) {
        if (i != pos && leafKeyCmp(leafNode, min, i) > 0) min = i;
    }
    return min;
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


void bpInterNodeShiftFromRight(bpTree *t, bpInterNode *interNode, int keyPos) {
    bpInterNode *parent = interNode->parent; // must != NULL
    // 将parent中对应的Key下降到当前节点中
    bpInterNode *right = nextBrother(t, interNode, keyPos);
    if (right == NULL) return;
    uint8_t *parentKeyAddr = interKeyAddr(parent, keyPos);
    int parentKeyLen = interKeyLen(parent, keyPos);
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
    // TODO: 这里可以改善，将上移到parent中的key长度搞小一点，就是说
    bpLeafNode *prev = leafNode->prev;
    if (prev == NULL) return;;
    int max = bpLeafFindMax(prev); // max must > 0
    unsigned char *key = malloc(KEY_MAX);
    if (key == NULL) return;
    int len = getKey(prev, max, key);
    bpLeafNodeInsert(t, leafNode, key, prev->kv[prev->fp[max].pos].value, len);

    int Second = bpLeafFindMaxExceptMax(prev, max);
    int insertKeyLen = CommonPrefix(leafNode, max, Second); // 优化
    bpInterNodeSet(t, leafNode->parent, key, insertKeyLen, -1);
    _bpLeafNodeRemoveWithPos(t, prev, max);
    free(key);
}


void bpLeafNodeShiftFromRight(bpTree *t, bpLeafNode *leafNode) {
    bpLeafNode *next = leafNode->next;
    if (next == NULL) return;;
    int min = bpLeafFindMin(next); // max must > 0
    unsigned char *key = malloc(KEY_MAX);
    if (key == NULL) return;
    int len = getKey(next, min, key);
    bpLeafNodeInsert(t, leafNode, key, next->kv[next->fp[min].pos].value, len);
    int Second = bpLeafFindMinExceptMin(next, min); // max must > 0
    getKey(next, Second, key);
    int insertKeyLen = CommonPrefix(leafNode, min, Second); // 优化
    bpInterNodeSet(t, leafNode->parent, key, insertKeyLen+1, 1); //next->kv[next->fp[newMin].pos].len
    _bpLeafNodeRemoveWithPos(t, next, min);
    free(key);
}


void bpInterNodeShiftFromLeft(bpTree *t, bpInterNode *interNode, int keyPos) {
    bpInterNode *left = prevBrother(t, interNode, keyPos);
    bpInterNode *parent = interNode->parent; // must != NULL
    // 将parent中对应的Key下降到当前节点中
    int pos = keyPos - 1;
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
    if ((NULL == left || leafNode->parent != left->parent) && (NULL == right || leafNode->parent != right->parent)) return 0;
    if (left != NULL && leafNode->parent == left->parent) {
        if ((right == NULL || leafNode->parent != right->parent) || right->size > left->size ) return -1;
    }
    return 1;
}

int bpInterNodeMergeFromLeftOrRight(bpTree *t, bpInterNode *interNode, int keyPos) {
    bpInterNode *left = prevBrother(t, interNode, keyPos);
    bpInterNode *right = nextBrother(t, interNode, keyPos);
    if (NULL == left && NULL == right) return 0;
    if (left != NULL) {
        if (right == NULL || right->size > left->size ) return -1;
    }
    return 1;
}

int bpLeafNodeShiftFromLeftOrRight(bpTree *t, bpLeafNode *leafNode) {
    bpLeafNode *left = leafNode->prev;
    if (left != NULL && left->parent != leafNode->parent) left = NULL;
    bpLeafNode *right = leafNode->next;
    if (right != NULL && right->parent != leafNode->parent) right = NULL;
    if (NULL == left && NULL == right) return 0;
    if (left != NULL && left->size > LEAF_MIN_SIZE) {
        if (right == NULL || right->size <= left->size ) return -1;
        else return 1;
    } else {
        if(right != NULL && right->size > LEAF_MIN_SIZE) return 1;
    }
    return 0;
}

int bpInterNodeShiftFromLeftOrRight(bpTree *t, bpInterNode *interNode, int pos) {
    bpInterNode *left = prevBrother(t, interNode, pos);
    bpInterNode *right = nextBrother(t, interNode, pos);
    if (NULL == left && NULL == right) return 0;
    if (left != NULL && left->size > MIN_INTER_NODE_SIZE) {
        if (right == NULL || right->size <= left->size ) return -1;
        else return 1;
    } else {
        if(right != NULL && right->size > MIN_INTER_NODE_SIZE) return 1;
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
        bpLeafNodeInsert(t, right, key, leafNode->kv[leafNode->fp[i].pos].value, len);
    }
    // remove the left
    KV *kv = &leafNode->kv[0];
    unsigned char *firstKey = kv->key;
    int firstKeyLen = kv->len;
    int keyPos = OffBinarySearch(t, leafNode->parent, firstKey, firstKeyLen);
    bpInterNodeRemoveLeftChild(t, leafNode->parent, keyPos);
    right->prev = leafNode->prev;
    if (NULL != leafNode->prev) {
        ((bpLeafNode *)(leafNode->prev))->next = right;
    }
    free(key);
    bpLeafNodeFree(leafNode);
    return 0;
}

int _bpLeafNodeMergeWithLeft(bpTree *t, bpLeafNode *leafNode) {
    bpLeafNode *left = leafNode->prev;
    if (left == NULL) exit(-1);
    unsigned char *key = malloc(KEY_MAX);
    memset(key, 0, KEY_MAX);
    for (int i = 0; i < leafNode->size; i++) {
        int len = getKey(leafNode, i, key);
        bpLeafNodeInsert(t, left, key, leafNode->kv[leafNode->fp[i].pos].value, len);
    }
    // remove the right
    KV *kv = &left->kv[0];
    unsigned char *firstKey = kv->key;
    int firstKeyLen = kv->len;
    int keyPos = OffBinarySearch(t, left->parent, firstKey, firstKeyLen);
    bpInterNodeRemoveRightChild(t, left->parent, keyPos);
    leafNode->next = leafNode->next;
    if (leafNode->next != NULL) {
        ((bpLeafNode *)(leafNode->next))->prev = left;
    }
    free(key);
    bpLeafNodeFree(leafNode);
    return 0;
}

int bpLeafNodeTryMerge(bpTree *t, bpLeafNode *leafNode) {
    if (leafNode == NULL || leafNode->parent == NULL) return 0;
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

bpInterNode *nextBrother(bpTree *t, bpInterNode *interNode, int pos) {
    bpInterNode *parent = interNode->parent;
    if (pos == interNode->size) return NULL;
    void **nextChild = (void **)(interChildAddr(parent, (pos + 1)));
    return (bpInterNode *)(*nextChild);
}

bpInterNode *prevBrother(bpTree *t, bpInterNode *interNode, int pos) {
    bpInterNode *parent = interNode->parent;
    if (pos <= 1) return NULL;
    void **prevChild = (void **)(interChildAddr(parent, pos - 1));
    return (bpInterNode *)(*prevChild);
}

//void bpInterNodeSetChildrenBrothers(bpInterNode *interNode, int pos, int v) {
//    void **childAddr = (void **)(interChildAddr(interNode, pos));
//    void *node = *childAddr;
//    while (pos <= interNode->size) {
//        if(*(uint8_t *)node == LEAF_NODE) {
//            ((bpLeafNode *)node)->brothers = v;
//            v++;
//            pos++;
//            node = *(++childAddr);
//        } else {
//            ((bpInterNode *)node)->brothers = v;
//            v++;
//            pos++;
//            node = *(++childAddr);
//        }
//    }
//    return;
//}

void _bpInterNodeMerge(bpTree *t, bpInterNode *left, bpInterNode *right, int keyPos) {
    if (left == NULL || right == NULL) return;
    bpInterNode *parent = left->parent;
    int pos = keyPos;
    uint8_t *parentKeyAddr = interKeyAddr(parent, pos);
    uint8_t parentKeyLen = interKeyLen(parent, pos);
    int rightKeysLen = right->children - right->keys;
    int totalSize = right->size + 1;
    int oralKeysLen = left->children - left->keys;
    // don't need to move offset
    // move keys and children
    uint8_t *childMoveBegin = interChildrenAddr(left);
    uint8_t *childMoveTo = interChildrenAddr(left) + rightKeysLen + parentKeyLen + totalSize * sizeof(off_t);
    int childMoveLen = sizeof(void *) * (left->size + 1);
    uint8_t *keyMoveBegin = interKeysAddr(left);
    uint8_t *keyMoveTo = interKeysAddr(left) + totalSize * sizeof(off_t);
    int keyMoveLen = left->children - left->keys;
    memmove(childMoveTo, childMoveBegin, childMoveLen);
    memmove(keyMoveTo, keyMoveBegin, keyMoveLen);

    int oralSize = left->size;
    // set the meta
    left->keys += totalSize * sizeof(off_t);
    left->children += totalSize * sizeof(off_t) + rightKeysLen + parentKeyLen;
    left->size += totalSize;
    left->bytes += totalSize * (sizeof(void *) + sizeof(off_t)) + rightKeysLen + parentKeyLen;

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
    memcpy(interKeyAddr(left, (oralSize + 1)), interKeysAddr(right), rightKeysLen);

    // printInterNode(left);
    // insert child from right
    setChildrenParent(right, left);
    memcpy(interChildAddr(left, oralSize + 1), interChildrenAddr(right), totalSize * sizeof(void *) + 1);
    // printInterNode(left);
    // delete the parent key
    bpInterNodeRemoveRightChild(t, parent, keyPos);

    bpInterNodeFree(right);

    // 将parent 第pos个之后孩子节点的brothers - 1, 重新设置brothers
    // bpInterNodeSetChildrenBrothers(left, oralSize + 1, oralSize + 1);
    return;
}

void _bpInterNodeMergeWithRight(bpTree *t, bpInterNode *interNode, int keyPos) {
    bpInterNode *left = interNode;
    bpInterNode *right = nextBrother(t, interNode, keyPos);
    _bpInterNodeMerge(t, left, right, keyPos);
    return;
}

void _bpInterNodeMergeWithLeft(bpTree *t, bpInterNode *interNode, int keyPos) {
    if (interNode == NULL) return;
    bpInterNode *left = prevBrother(t, interNode, keyPos);
    bpInterNode *right = interNode;
    _bpInterNodeMerge(t, left, right, keyPos);
    return;
}


int bpInterNodeTryMerge(bpTree *t, bpInterNode *interNode) {
    if (interNode == NULL || interNode->parent == NULL) return 0;
    if (interNode->size >= INTER_MIN_SIZE) return -1;
    unsigned char *interNodeFirstKey = interKeyAddr(interNode, 0);
    int interNodeFirstKeyLen = interKeyLen(interNode, 0) - 1;// TODO: handle long key
    int keyPos = OffBinarySearch(t, interNode, interNodeFirstKey, interNodeFirstKeyLen);
    int shift = bpInterNodeShiftFromLeftOrRight(t, interNode, keyPos);
    if (shift > 0) {
        bpInterNodeShiftFromRight(t, interNode, keyPos);
    } else if (shift < 0) {
        bpInterNodeShiftFromLeft(t, interNode, keyPos);
    } else {
        int res = bpInterNodeMergeFromLeftOrRight(t, interNode, keyPos);
        if (1 == res) {
            _bpInterNodeMergeWithRight(t, interNode, keyPos);
        } else if (-1 == res) {
            _bpInterNodeMergeWithLeft(t, interNode, keyPos);
        }
    }
    return 0;
}


void bpLeafNodeFpRemove(bpLeafNode *leafNode, int pos) {
    for(int i = pos; i < leafNode->size-1; i++) {
        leafNode->fp[i] = leafNode->fp[i+1];
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


int _bpLeafNodeRemove(bpTree *t, bpLeafNode *leafNode, unsigned char *key, int len) {
    int success = 0;
    fpBinarySearch(t, leafNode, key, &success, len);
    if (success < 0) { // not exists
        return -1; //
    }
    int pos = success;
    _bpLeafNodeRemoveWithPos(t, leafNode, pos);
    bpLeafNodeTryMerge(t, leafNode);
    return 0;
}


int bpRemove(bpTree *t, unsigned char *key, int len) {
    if (key == NULL || 0 == strlen(key) || t == NULL) return -1;
    void *h = t->header;
    int l = t->level;
    if (l == 0 || h == NULL) return -1;
    if (l == 1) {
        return _bpLeafNodeRemove(t, h, key, len);
    }
    bpLeafNode *ln = bpInterNodeFind(t, h, key, len);
    return _bpLeafNodeRemove(t ,ln, key, len);
}

void bpInterNodeFree(bpInterNode *interNode) {
    if (interNode == NULL) return;
    bp_free(interNode);
}

void bpLeafNodeFree(bpLeafNode *leafNode) {
    if (leafNode == NULL) return;
    bp_free(leafNode);
}

void printInterNode(bpInterNode *t) {
    printf("\ninterNode: size: %d ", t->size);
    printf("kv: ");
    int size = t->size;
    for(int i = 0; i < size; i++) {
        // int len = interKeyLen(t, i) - 1;
        int end = ((i == t->size - 1) ? (t->children - t->keys) : *(off_t *)(t->data + (i+1) * sizeof(off_t))) ;
        int begin = *(off_t *)(t->data + i * sizeof(off_t));
        int len = end - begin;
        uint8_t *keyAddr = interKeyAddr(t, i);
        int j = 1;
        for (; j < len; j++) {
            char c = *(keyAddr+j);
            printf("%c", c);
        }
        printf("    ");
    }
}