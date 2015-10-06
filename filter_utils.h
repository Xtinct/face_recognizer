#include "shared_variables.h"


#define WIDTH 640
#define HEIGHT 480

#define KERNEL_SMALL_SIZE 9
#define KERNEL_EXTENDED_SIZE 25

#define FILTER_COUNT 6

static const int box_filters[FILTER_COUNT][KERNEL_SMALL_SIZE] = {
  {   0,  0,  0,  0, 1,  0,  0,  0,  0  },  //normal
  {   1,  1,  1,  1, 2,  1,  1,  1,  1  },  //blur
  {   0,  0, -2,  0, 2,  0,  1,  0,  0  },  //raised
  {   0,  0,  1,  0, 0,  0,  1,  0,  0  },  //motion
  {  -1, -1, -1, -1, 8, -1, -1, -1, -1  },  //edge
  {  -1, -1, -1, -1, 9, -1, -1, -1, -1  }  //sharpen
};

typedef struct {
  unsigned short filter_size;
  const int *filter;
  int *indexes;
} Arg;

typedef struct {
  void (*func)(const Arg *, uint8_t * input);
  const Arg arg;
} Callable;

void box_filter(const Arg *arg,uint8_t * input);

static int basic_kernel_indexes[KERNEL_SMALL_SIZE] = {
 -(WIDTH + 1), -WIDTH, -(WIDTH - 1),
 -1,              0,       1,
  WIDTH - 1,    WIDTH,   WIDTH + 1
} ;


static const Callable filters[FILTER_COUNT][KERNEL_SMALL_SIZE] = {
    /*  filter                                    params    */
    { &box_filter,  {.filter = box_filters[0], .filter_size = 9, .indexes = basic_kernel_indexes  } },
    { &box_filter,  {.filter = box_filters[1], .filter_size = 9, .indexes = basic_kernel_indexes  } },
    { &box_filter,  {.filter = box_filters[2], .filter_size = 9, .indexes = basic_kernel_indexes  } },
    { &box_filter,  {.filter = box_filters[3], .filter_size = 9, .indexes = basic_kernel_indexes  } },
    { &box_filter,  {.filter = box_filters[4], .filter_size = 9, .indexes = basic_kernel_indexes  } },
    { &box_filter,  {.filter = box_filters[5], .filter_size = 9, .indexes = basic_kernel_indexes  } }
};



static const char* filter_names[6] = {
  "Normal",
  "Box",
  "Raised",
  "Motion",
  "Edge detection",
  "Sharpen"
};
