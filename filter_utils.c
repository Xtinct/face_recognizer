#include "filter_utils.h"

void box_filter(const Arg *arg,uint8_t * input) {
  size_t x;
  size_t y;
  uint8_t * ary = NULL;
  uint8_t * temp = (uint8_t*) malloc (WIDTH*HEIGHT*3);

  int *indexes = arg->indexes;

  memcpy(temp, input, WIDTH*HEIGHT*3);

  int R,G,B,i,index,sum = 0;

  for(i = 0; i< arg->filter_size; i++)
    sum += arg->filter[i];
  if(sum == 0)
    sum = 1;

    for (y =0; y < HEIGHT-1; y++)
      for (x = 0; x < WIDTH-1; x++)
      {
        R = G = B = 0;
        index = y * WIDTH + x;
        ary = input + (index) * 3;

        for(i = 0; i < arg->filter_size; i++)
          /* prevent wrong indexing */
          if(index + arg->indexes[i] > 0)
          {
            R += (temp + (index + arg->indexes[i] ) * 3)[0] * arg->filter[i];
            G += (temp + (index + arg->indexes[i] ) * 3)[1] * arg->filter[i];
            B += (temp + (index + arg->indexes[i] ) * 3)[2] * arg->filter[i];
          }

        ary[0] = max(0, min(255, R/sum));
        ary[1] = max(0, min(255, G/sum));
        ary[2] = max(0, min(255, B/sum));
      }
      free(temp);
}