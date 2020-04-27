#include <stdio.h>
#include "bplustree.h"
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

//int partition(int arr[], int left, int right) {
//    int pivot = left;
//    int index = pivot+1;
//    for(int i = index; i <= right; i++) {
//        if(arr[i] < arr[pivot]) {
//            int temp = arr[i];
//            arr[i] = arr[index];
//            arr[index] = temp;
//            index ++;
//        }
//    }
//    int temp = arr[pivot];
//    arr[pivot] = arr[index-1];
//    arr[index-1] = temp;
//    return index - 1;
//}
//
//void quickSort(int arr[], int left, int right) {
//    if(left < right) {
//        int partitionIndex = partition(arr, left, right);
//        quickSort(arr, left, partitionIndex-1);
//        quickSort(arr, partitionIndex+1, right);
//    }
//    return;
//}

bpTree *t;
void test_leafSplit() {
    t = bpTreeNew();
    for (int i = 0; i < 7; i++) {
        unsigned char hash = i;
        bpInsert(t, &hash, i, 1);
    }
    unsigned char hash = 8;
    // printf("go\n");
    bpInsert(t, &hash, 8, 1);
    return ;
}

void main() {

//    int arr[6];
//    for (int i = 0; i<6; i++) arr[i] = i;
//    quickSort(arr, 0, 5);
//    for(int i=0; i<6 ;i++) printf("%d ", arr[i]);
    test_leafSplit();
    printf("afdgsf\n");
    return;
}


