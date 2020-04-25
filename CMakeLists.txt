cmake_minimum_required(VERSION 3.15)
project(bptree C)

set(CMAKE_C_STANDARD 99)

add_executable(bptree main.c bplustree.h util.h util.c bplustree.c pagecache.h pahecache.c bp_malloc.h murmurhash3.h murmurhash3.c)