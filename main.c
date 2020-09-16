#include <stdio.h>
#include <stdlib.h>
#include<conio.h>
#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include "bplustree.h"
#include "murmurhash3.h"
#include "util.h"

static unsigned long x=123456789, y=362436069, z=521288629;
unsigned long xorshf96(void) {          //period 2^96-1
    unsigned long t;
    x ^= x << 16;
    x ^= x >> 5;
    x ^= x << 1;

    t = x;
    x = y;
    y = z;
    z = t ^ x ^ y;

    return z;
}


#define N 10//固定长度为10
#define MAX_SIZE 4
#define MAX_LENGTH 10
int getString(unsigned char *ch)
{
    int flag,charLengt;
    int i,j,k=0;
    // srand((unsigned)time(NULL));
     int len = rand()%MAX_LENGTH + 1;
    // int len = 4;
    for(j=0;j<len;j++)
    {
//        flag=rand()%2;
//        if(flag) ch[k++]='A'+rand()%26;
//        else ch[k++]='a'+rand()%26;
          ch[k++]='a'+rand()%26;
    }
    ch[k]='\0';
    k=0;
    return len;
}


bpTree *t;
void testLeafSplit() {
    t = bpTreeNew();
    for (int i = 0; i < 7; i++) {
    }
//    unsigned char hash = 8;
//    // printf("go\n");
//    bpInsert(t, &hash, 8, 1);
    return ;
}

void testLeafShift() {
    t = bpTreeNew();
    for (int i = 0; i < 9; i++) {
        unsigned char hash = i;
        bpInsert(t, &hash, i, 1);
    }
    unsigned char hash = 1;
    bpRemove(t, &hash, 1);
    bpFind(t, &hash, 1);
    return;
}

void printLeafNode(bpLeafNode *t) {
    printf("\nleafNode: size: %d, prefix: %s ", t->size, t->prefix );
//    printf("\nfp: ");
//    for(int i = 0; i < t->size; i++) {
//        printf("%d %d ", t->fp[i].fingerprint, t->fp[i].pos);
//    }
    printf("kv: ");
    for(int i = 0; i < t->size; i++) {
        KV *kv = &t->kv[t->fp[i].pos];
        unsigned char *key = kv->key;
        for (int j = 0; j < kv->len; j++) {
            printf("%c", *(key + j));
        }
        printf("    ");
    }
}

void print(void *t) {
    if(*(uint8_t *)t== INTER_NODE) {
        bpInterNode *interNode = (bpInterNode *)t;
        printInterNode(interNode);
        int size = interNode -> size;
        void **childAddr = (void **)(interChildrenAddr(interNode));
        void *node = *childAddr;
        int i=0;
        while(i <= interNode->size) {
            print(node);
            i ++;
            node = *(childAddr+i);
        }
    } else {
        printLeafNode((bpLeafNode *)t);
    }
}

void insertFromBigToSmall() {
    t = bpTreeNew();
    for (int i = 30; i > 1; i--) {
        unsigned char hash = i + '0';
        bpInsert(t, &hash, 0, 1);
    }

    if(t->header != NULL) print(t->header);
}

void randomInsert(int num) {
    unsigned char ch[10];
    t = bpTreeNew();
    for (int i = 0; i < num; i++) {
        // printf("%d", hash);
        int len = getString(ch);
        if (i == 58) {
            printf("asdfgd");
        }
        bpInsert(t, ch, 0, len);
        if (len != 0 && bpFind(t, ch, len) == -1) {
            printf("i = %d\n", i);
            printf("after insert %s", ch);
            printf("%d %s", bpFind(t, ch, len), ch);
        }
        // if(t->header != NULL) print(t->header);
    }
}

void testLeafNodeShiftFromLeft() {
    randomInsert(10);
    if(t->header != NULL) {
        printf("\n");
        print(t->header);
    }
    unsigned char *delete = "zvsrt";
    bpRemove(t, delete, 5);
    if(t->header != NULL) {
        printf("\n");
        print(t->header);
    }
    unsigned char *insert1 = "hx";
    bpInsert(t, insert1, 0, 2);
    if(t->header != NULL) {
        printf("\n");
        print(t->header);
    }
    unsigned char *delete2 = "meayl";
    bpRemove(t, delete2, 5);
    if(t->header != NULL) {
        printf("\n");
        print(t->header);
    }
}

void testLeafNodeShiftFromRight() {
    randomInsert(10);
    if(t->header != NULL) {
        printf("\n");
        print(t->header);
    }
    unsigned char *delete = "h";
    bpRemove(t, delete, 1);
    if(t->header != NULL) {
        printf("\n");
        print(t->header);
    }
}

void testLeafNodeMergeFromLeft() {
    randomInsert(10);
    if(t->header != NULL) {
        printf("\n");
        print(t->header);
    }
    unsigned char *delete = "h";
    bpRemove(t, delete, 1);
    if(t->header != NULL) {
        printf("\n");
        print(t->header);
    }
    unsigned char *delete1 = "jprep";
    bpRemove(t, delete1, 5);
    if(t->header != NULL) {
        printf("\n");
        print(t->header);
    }
}

void testLeafNodeMergeAfterSplit() {
    randomInsert(20);
    if(t->header != NULL) {
        printf("\n");
        print(t->header);
    }
    unsigned char *delete = "h";
    bpRemove(t, delete, 1);
    if(t->header != NULL) {
        printf("\n");
        print(t->header);
    }

    unsigned char *insert = "fff";
    bpInsert(t, insert, 0, 3);
    if(t->header != NULL) {
        printf("\n");
        print(t->header);
    }
//    unsigned char *delete1 = "jprep";
//    bpRemove(t, delete1, 5);
//    if(t->header != NULL) {
//        printf("\n");
//        print(t->header);
//    }
}

void testInterNodeDeleteAll() {
    randomInsert(6);
    if(t->header != NULL) {
        printf("\n");
        print(t->header);
    }
    unsigned char *delete = "h";
    bpRemove(t, delete, 1);
    if(t->header != NULL) {
        printf("\n");
        print(t->header);
    }
}

#define NB_INSERTS 10000000LU


int bench_data_structures(void) {
    declare_timer;
    declare_memory_counter;


    /*
     * RBTREE
     */

    /*
     * RAX - https://github.com/antirez/rax
     */



    /*
     * ART - https://github.com/armon/libart
     */
//    art_tree t;
//    art_tree_init(&t);
//
//    start_timer {
//        struct index_entry *e;
//        for(size_t i = 0; i < NB_INSERTS; i++) {
//            uint64_t hash = xorshf96()%NB_INSERTS;
//            e = malloc(sizeof(*e));
//            art_insert(&t, (unsigned char*)&hash, sizeof(hash), e);
//        }
//    } stop_timer("ART - Time for %lu inserts/replace (%lu inserts/s)", NB_INSERTS, NB_INSERTS*1000000LU/elapsed);
//
//    start_timer {
//        for(size_t i = 0; i < NB_INSERTS; i++) {
//            uint64_t hash = xorshf96()%NB_INSERTS;
//            art_search(&t, (unsigned char*)&hash, sizeof(hash));
//        }
//    } stop_timer("ART - Time for %lu finds (%lu finds/s)", NB_INSERTS, NB_INSERTS*1000000LU/elapsed);
//
//    get_memory_usage("ART");

    /*
     * BTREE
     */
//    btree_t * b = btree_create();
//
//    start_timer {
//        struct index_entry e;
//        for(size_t i = 0; i < NB_INSERTS; i++) {
//            uint64_t hash = xorshf96()%NB_INSERTS;
//            btree_insert(b, (unsigned char*)&hash, sizeof(hash), &e);
//        }
//    } stop_timer("BTREE - Time for %lu inserts/replace (%lu inserts/s)", NB_INSERTS, NB_INSERTS*1000000LU/elapsed);
//
//    start_timer {
//        struct index_entry e;
//        for(size_t i = 0; i < NB_INSERTS; i++) {
//            uint64_t hash = xorshf96()%NB_INSERTS;
//            btree_find(b, (unsigned char*)&hash, sizeof(hash), &e);
//        }
//    } stop_timer("BTREE - Time for %lu finds (%lu finds/s)", NB_INSERTS, NB_INSERTS*1000000LU/elapsed);
//
//    get_memory_usage("BTREE");

    start_timer {
        uint64_t e;
        for(size_t i = 0; i < NB_INSERTS; i++) {
            uint64_t hash = xorshf96()%NB_INSERTS;
            bpInsert(b, (unsigned char*)&hash, e, sizeof(hash));
        }
    } stop_timer("BTREE - Time for %lu inserts/replace (%lu inserts/s)", NB_INSERTS, NB_INSERTS*1000000LU/elapsed);

    start_timer {
        struct index_entry e;
        for(size_t i = 0; i < NB_INSERTS; i++) {
            uint64_t hash = xorshf96()%NB_INSERTS;
            btree_find(b, (unsigned char*)&hash, sizeof(hash), &e);
        }
    } stop_timer("BTREE - Time for %lu finds (%lu finds/s)", NB_INSERTS, NB_INSERTS*1000000LU/elapsed);

    get_memory_usage("BTREE");
    return 0;
}

#define NB_INSERTS 4
void main() {
    printf("test begin\n");
    testLeafNodeMergeAfterSplit();
    printf("\ntest over\n");
    return;
}




