#include "shared_variables.h"


#define WIDTH 640
#define HEIGHT 480

#define KERNEL_SMALL_SIZE 9
#define KERNEL_EXTENDED_SIZE 25

#define FILTER_COUNT 6

static const int filters[FILTER_COUNT][KERNEL_SMALL_SIZE] = {
  {   0,  0,  0,  0, 1,  0,  0,  0,  0  },  //normal
  {   1,  1,  1,  1, 2,  1,  1,  1,  1  },  //blur
  {   0,  0, -2,  0, 2,  0,  1,  0,  0  },  //raised
  {   0,  0,  1,  0, 0,  0,  1,  0,  0  },  //motion
  {  -1, -1, -1, -1, 8, -1, -1, -1, -1  },  //edge
  {  -1, -1, -1, -1, 9, -1, -1, -1, -1  }  //sharpen
};

static int basic_kernel_indexes[KERNEL_SMALL_SIZE] = {
 -(WIDTH + 1), -WIDTH, -(WIDTH - 1),
 -1,              0,       1,
  WIDTH - 1,    WIDTH,   WIDTH + 1
} ;


static const char* filter_names[6] = {
  "Normal",
  "Box",
  "Raised",
  "Motion",
  "Edge detection",
  "Sharpen"
};


void apply_filter(uint8_t * input, const int* filter, int kernel_size);