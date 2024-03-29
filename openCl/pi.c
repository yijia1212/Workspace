#include <stdio.h>
#include "omp.h"

static long num_steps = 100000;
double step;
void main(){
  int i;
  double x, pi, sum = 0.0;
  step = 1.0/(double)num_steps;
#pragma omp parallel for
{
  for(i = 0; i < num_steps; i ++){
    x = (i + 0.5)*step;
    sum = sum + 4.0/(1.0 + x*x);
  }
}x
  pi = step*sum;
  printf("PI is %f", pi);

}
