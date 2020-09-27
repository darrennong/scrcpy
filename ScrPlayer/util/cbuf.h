// generic circular buffer (bounded queue) implementation
#ifndef CBUF_H
#define CBUF_H

#include <stdbool.h>

// To define a circular buffer type of 20 ints:
//     struct cbuf_int CBUF(int, 20);
//
// data has length CAP + 1 to distinguish empty vs full.
#define CBUF(TYPE, CAP) { \
    TYPE data[(CAP) + 1]; \
    size_t head; \
    size_t tail; \
}

#define cbuf_size_(PCBUF) \
    (sizeof((PCBUF)->data) / sizeof(*(PCBUF)->data))

#define cbuf_is_empty(PCBUF) \
    ((PCBUF)->head == (PCBUF)->tail)

#define cbuf_is_full(PCBUF) \
    (((PCBUF)->head + 1) % cbuf_size_(PCBUF) == (PCBUF)->tail)

#define cbuf_init(PCBUF) \
    (void) ((PCBUF)->head = (PCBUF)->tail = 0)
// ����ʹ�ô˺�ǰ���ж� !cbuf_is_full
#define cbuf_push(PCBUF, ITEM) \
            (PCBUF)->data[(PCBUF)->head] = (ITEM); \
            (PCBUF)->head = ((PCBUF)->head + 1) % cbuf_size_(PCBUF); \
// ����ʹ�ô˺�ǰ���ж� !cbuf_is_empty
#define cbuf_take(PCBUF, PITEM) \
            *(PITEM) = (PCBUF)->data[(PCBUF)->tail]; \
            (PCBUF)->tail = ((PCBUF)->tail + 1) % cbuf_size_(PCBUF);
#endif
