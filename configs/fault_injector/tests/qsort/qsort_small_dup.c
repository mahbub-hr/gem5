#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define UNLIMIT
#define MAXARRAY 60000 /* this number, if too large, will cause a seg. fault!! */

struct myStringStruct {
  char qstring[128];
};

int compare(const void *elem1, const void *elem2)
{
  int result;
  
  result = strcmp((*((struct myStringStruct *)elem1)).qstring, (*((struct myStringStruct *)elem2)).qstring);

  return (result < 0) ? 1 : ((result == 0) ? 0 : -1);
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

struct myStringStruct array[MAXARRAY];
struct myStringStruct array_shadow[MAXARRAY];
int
main(int argc, char *argv[]) {
  // struct myStringStruct array[MAXARRAY];
  FILE *fp;
  int i,count=0;
  
  if (argc<2) {
    fprintf(stderr,"Usage: qsort_small <file>\n");
    exit(-1);
  }
  else {
    fp = fopen(argv[1],"r");
    
    while(((count < MAXARRAY) && (fscanf(fp, "%s", array[count].qstring) == 1))) {
      strncpy(array_shadow[count].qstring, array[count].qstring, 128);
      count++;
    }
  }
  printf("\nSorting %d elements.\n\n",count);
  qsort(array,count,sizeof(struct myStringStruct),compare);
  qsort(array_shadow,count,sizeof(struct myStringStruct),compare);

  for(i=0;i<count;i++){
    if(strcmp(array[i].qstring, array_shadow[i].qstring) != 0){
      error();
    }
    printf("%s\n", array[i].qstring);
  }
  return 0;
}
