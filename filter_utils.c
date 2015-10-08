#include "filter_utils.h"
#include <math.h>

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


void median_filter(const Arg *arg,uint8_t * input) {
  size_t x;
  size_t y;
  uint8_t * ary = NULL;
  uint8_t * temp = (uint8_t*) malloc (WIDTH*HEIGHT*3);

  int *indexes = arg->indexes;
  int median_vector[3][arg->filter_size];

  memcpy(temp, input, WIDTH*HEIGHT*3);

  int i,index;

    for (y =0; y < HEIGHT-1; y++)
      for (x = 0; x < WIDTH-1; x++)
      {

        index = y * WIDTH + x;
        ary = input + (index) * 3;

        for(i = 0; i < arg->filter_size; i++)
          /* prevent wrong indexing */
          if(index + arg->indexes[i] > 0)
          {
            median_vector[0][i] = (temp + (index + arg->indexes[i] ) * 3)[0];
            median_vector[1][i] = (temp + (index + arg->indexes[i] ) * 3)[1];
            median_vector[2][i] = (temp + (index + arg->indexes[i] ) * 3)[2];
          }

          for (i = 0; i < 3; ++i)
            sort_asc(median_vector[i], arg->filter_size);

          ary[0] = median_vector[0][4];
          ary[1] = median_vector[1][4];
          ary[2] = median_vector[2][4];
      }
      free(temp);
}


void sobel_filter(const Arg *arg,uint8_t * input) {

  size_t x;
  size_t y;
  uint8_t * ary = NULL;
  uint8_t * temp = (uint8_t*) malloc (WIDTH*HEIGHT*3);

  memcpy(temp, input, WIDTH*HEIGHT*3);

  int R_1, R_2,G_1, G_2,B_1, B_2,i,index;

  for (y =0; y < HEIGHT-1; y++)
    for (x = 0; x < WIDTH-1; x++)
    {
      R_1 = R_2 = G_1 = G_2 = B_1 = B_2 = 0;
      index = y * WIDTH + x;
      ary = input + (index) * 3;

      for(i = 0; i < arg->filter_size; i++)
        /* prevent wrong indexing */
        if(index + arg->indexes[i] > 0)
        {
          R_1 += (temp + (index + arg->indexes[i] ) * 3)[0] * arg->filter[i];
          G_1 += (temp + (index + arg->indexes[i] ) * 3)[1] * arg->filter[i];
          B_1 += (temp + (index + arg->indexes[i] ) * 3)[2] * arg->filter[i];

          R_2 += (temp + (index + arg->indexes[i] ) * 3)[0] * arg->filter[i + arg->filter_size];
          G_2 += (temp + (index + arg->indexes[i] ) * 3)[1] * arg->filter[i + arg->filter_size];
          B_2 += (temp + (index + arg->indexes[i] ) * 3)[2] * arg->filter[i + arg->filter_size];
        }


      ary[0] = max(0, min(255, sqrt(pow(R_1,2) + pow(R_2,2)) ));
      ary[1] = max(0, min(255, sqrt(pow(G_1,2) + pow(G_2,2)) ));
      ary[2] = max(0, min(255, sqrt(pow(B_1,2) + pow(B_2,2)) ));
    }
  free(temp);
}


void sort_asc(int *in, int length) {

  int i, j;
  int min_element_index;

  for(i = 0; i < length-1; i++){
    min_element_index = i;
    for(j = i+1; j < length; j++){
      if (min(in[min_element_index], in[j]) == in[j]){
        min_element_index = j;
      }
    }

    swap(&in[min_element_index], &in[i]);
  }
}

void swap(int *a, int *b){

  int temp = *a;

  *a = *b;
  *b = temp;
}