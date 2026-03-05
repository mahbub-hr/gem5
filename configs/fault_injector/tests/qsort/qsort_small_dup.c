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
