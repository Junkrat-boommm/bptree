#include <stdio.h>
#include <stdlib.h>
#include<conio.h>
#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include "bplustree.h"
#include "murmurhash3.h"

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
int getString(unsigned char *ch)
{
    int flag,charLengt;
    int i,j,k=0;
    // srand((unsigned)time(NULL));
    // int len = rand()%MAX_SIZE;
    int len = 4;
    for(j=0;j<len;j++)
    {
        flag=rand()%2;
        if(flag) ch[k++]='A'+rand()%26;
        else ch[k++]='a'+rand()%26;
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
void printInterNode(bpInterNode *t) {
    printf("\ninterNode: size: %d ", t->size);
//    printf("\nfp: ");
//    for(int i = 0; i < t->size; i++) {
//        printf("%d %d ", t->fp[i].fingerprint, t->fp[i].pos);
//    }
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
#define NB_INSERTS 4
void main() {
//    int arr[6];
//    for (int i = 0; i<6; i++) arr[i] = i;
//    quickSort(arr, 0, 5);
//    for(int i=0; i<6 ;i++) printf("%d ", arr[i]);
    unsigned char ch[10];
    t = bpTreeNew();
    printf("test begin\n");
    for (int i = 0; i < 50; i++) {
        // printf("%d", hash);
        int len = getString(ch);
        // printf("%s  ", ch);
        bpInsert(t, ch, 0, len);
    }
    printf("insert over\n");
    if(t->header != NULL) print(t->header);
    printf("\ntest over\n");
    return;
}




