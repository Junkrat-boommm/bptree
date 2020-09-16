//
// Created by ganquan on 2020/4/16.
//

#ifndef BPTREE_UTIL_H
#define BPTREE_UTIL_H

#include "bplustree.h"

#include <sys/time.h>
/*
 * Cute timer macros
 * Usage:
 * declare_timer;
 * start_timer {
 *   ...
 * } stop_timer("Took %lu us", elapsed);
 */
#define declare_timer uint64_t elapsed; \
   struct timeval st, et;

#define start_timer gettimeofday(&st,NULL);

#define stop_timer(msg, args...) ;do { \
   gettimeofday(&et,NULL); \
   elapsed = ((et.tv_sec - st.tv_sec) * 1000000) + (et.tv_usec - st.tv_usec) + 1; \
   printf("(%s,%d) [%6lums] " msg "\n", __FUNCTION__ , __LINE__, elapsed/1000, ##args); \
} while(0)

/*
* Cute way to measure memory usage.
*/
#define declare_memory_counter \
   struct rusage __memory; \
   uint64_t __previous_mem = 0, __used_mem; \
   getrusage(RUSAGE_SELF, &__memory); \
   __previous_mem = __memory.ru_maxrss;

#define get_memory_usage(msg, args...) \
   getrusage(RUSAGE_SELF, &__memory); \
   __used_mem = __memory.ru_maxrss - __previous_mem; \
   __previous_mem = __memory.ru_maxrss; \
   printf("(%s,%d) [%lu MB total - %lu MB since last measure] " msg "\n", __FUNCTION__ , __LINE__, __previous_mem/1024, __used_mem/1024, ##args);


int keyCompare(uint8_t *c1, uint8_t *c2, int l1, int l2);
int keyCompareWithMove(uint8_t **c1, uint8_t *c2, int l1, int l2);
int keyContain(uint8_t *c1, uint8_t *c2, int l1, int l2);
#endif //BPTREE_UTIL_H
