#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))
#define MIN(X,Y) (X) < (Y) ? (X) : (Y)
#define MAX(X,Y) (X) > (Y) ? (X) : (Y)

typedef struct size {
    uint16_t width;
    uint16_t height;
} size_w,size;

typedef struct point {
    int32_t x;
    int32_t y;
}point;

typedef struct position {
    // The video screen size may be different from the real device screen size,
    // so store to which size the absolute position apply, to scale it
    // accordingly.
    size_w screen_size;
    struct point point;
}position;

struct port_range {
    uint16_t first;
    uint16_t last;
};

static point inverse_point(struct point point, struct size size) {
    point.x = size.width - point.x;
    point.y = size.height - point.y;
    return point;
}
#endif
