#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define UNLIMIT
#define MAXARRAY 60000 /* this number, if too large, will cause a seg. fault!! */

struct my3DVertexStruct {
  int x, y, z;
  double distance;
};

int compare(const void *elem1, const void *elem2)
{
  /* D = [(x1 - x2)^2 + (y1 - y2)^2 + (z1 - z2)^2]^(1/2) */
  /* sort based on distances from the origin... */

  double distance1, distance2;

  distance1 = (*((struct my3DVertexStruct *)elem1)).distance;
  distance2 = (*((struct my3DVertexStruct *)elem2)).distance;

  return (distance1 > distance2) ? 1 : ((distance1 == distance2) ? 0 : -1);
}

/* 
 * Helper function to swap memory byte-by-byte.
 * This is required to handle the generic 'void *' types.
 */
void swap(void *a, void *b, size_t size) {
    char t;
    char *p = (char *)a;
    char *q = (char *)b;
    for (size_t i = 0; i < size; i++) {
        t = *p; 
        *p++ = *q; 
        *q++ = t;
    }
}

/*
 * The recursive QuickSort logic (Lomuto partition scheme).
 */
void my_qsort_recursive(void *base, int low, int high, size_t size, int (*compar)(const void *, const void *)) {
    if (low < high) {
        // We select the last element as the pivot
        char *pivot = (char *)base + high * size;
        int i = low - 1;

        for (int j = low; j < high; j++) {
            char *current = (char *)base + j * size;
            
            // If current element is smaller than or equal to pivot
            // based on the provided comparison function
            if (compar(current, pivot) <= 0) {
                i++;
                swap((char *)base + i * size, current, size);
            }
        }
        // Place the pivot in the correct position
        swap((char *)base + (i + 1) * size, pivot, size);
        
        int pi = i + 1;

        // Recursively sort elements before and after partition
        my_qsort_recursive(base, low, pi - 1, size, compar);
        my_qsort_recursive(base, pi + 1, high, size, compar);
    }
}

/*
 * The public interface matching standard qsort
 */
void my_qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    if (nmemb > 1) {
        my_qsort_recursive(base, 0, (int)nmemb - 1, size, compar);
    }
}

void error(){
    printf("Error: Mismatch between C and C_shadow\n");
    exit(-1);
}

struct my3DVertexStruct array[MAXARRAY];
struct my3DVertexStruct array_shadow[MAXARRAY];

int
main(int argc, char *argv[]) {
  

  FILE *fp, *fp_shadow;
  int i,count=0, count_shadow = 0;
  int x, y, z;
  
  if (argc<2) {
    fprintf(stderr,"Usage: qsort_large <file>\n");
    exit(-1);
  }
  else {
    fp = fopen(argv[1],"r");
    fp_shadow = fopen(argv[1], "r");
    
    while((fscanf(fp, "%d", &x) == 1) && (fscanf(fp, "%d", &y) == 1) && (fscanf(fp, "%d", &z) == 1) &&  (count < MAXARRAY)) {
      array[count].x = x;
      array[count].y = y;
      array[count].z = z;
      array[count].distance = sqrt(pow(x, 2) + pow(y, 2) + pow(z, 2));
      count++;
    }

    while((fscanf(fp_shadow, "%d", &x) == 1) && (fscanf(fp_shadow, "%d", &y) == 1) && (fscanf(fp_shadow, "%d", &z) == 1) &&  (count_shadow < MAXARRAY)) {
      array_shadow[count_shadow].x = x;
      array_shadow[count_shadow].y = y;
      array_shadow[count_shadow].z = z;
      array_shadow[count_shadow].distance = sqrt(pow(x, 2) + pow(y, 2) + pow(z, 2));

      count_shadow++;
    }
  }

  if (count != count_shadow){
    error();
  }

  for(i=0;i<count;i++){
    if(array[i].x != array_shadow[i].x || array[i].y != array_shadow[i].y || array[i].z != array_shadow[i].z){
      error();
    }

    printf("%d %d %d\n", array[i].x, array[i].y, array[i].z);
  }
  
  printf("\nSorting %d vectors based on distance from the origin.\n\n",count);
  my_qsort(array,count,sizeof(struct my3DVertexStruct),compare);
  my_qsort(array_shadow,count,sizeof(struct my3DVertexStruct),compare);
  
  for(i=0;i<count;i++){
    if(array[i].x != array_shadow[i].x || array[i].y != array_shadow[i].y || array[i].z != array_shadow[i].z){
      error();
    }

    printf("%d %d %d\n", array[i].x, array[i].y, array[i].z);
  }
  
  return 0;
}
