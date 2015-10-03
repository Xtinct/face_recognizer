#include "filter_utils.h"


void apply_filter(uint8_t * input, const int* filter, int kernel_size) {
  size_t x;
  size_t y;
  uint8_t * ary = NULL;
  uint8_t * temp = (uint8_t*) malloc (WIDTH*HEIGHT*3);

  int *indexes;

  memcpy(temp, input, WIDTH*HEIGHT*3);

  switch(kernel_size)
  {
    case KERNEL_EXTENDED_SIZE:
      //TODO add indices for extended kernel
      break;
    default:
      indexes = basic_kernel_indexes;
      break;
  }

  int R,G,B,i,index,sum = 0;

  for(i = 0; i< kernel_size; i++)
    sum += filter[i];
  if(sum == 0)
    sum = 1;

    for (y =0; y < HEIGHT-1; y++)
      for (x = 0; x < WIDTH-1; x++)
      {
        R = G = B = 0;
        index = y * WIDTH + x;
        ary = input + (index) * 3;

        for(i = 0; i < kernel_size; i++)
          /* prevent wrong indexing */
          if(index + indexes[i] > 0)
          {
            R += (temp + (index + indexes[i] ) * 3)[0] * filter[i];
            G += (temp + (index + indexes[i] ) * 3)[1] * filter[i];
            B += (temp + (index + indexes[i] ) * 3)[2] * filter[i];
          }

        ary[0] = max(0, min(255, R/sum));
        ary[1] = max(0, min(255, G/sum));
        ary[2] = max(0, min(255, B/sum));
      }
      free(temp);
}
