#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#define _POSIX_SOURCE
#include <dirent.h>
#include "common.h"

FILE *fptr;
DIR *dir;

int main(){
    printf("Creazione FILE SYSTEM FAT...\n");

    fptr = fopen("filename.txt", "w");

    printf("File descriptor: %p\n",fptr);
    
    char buff[16+1];
    
    sprintf(buff,"%p",fptr);


    //memcpy(buff, fptr, sizeof(fptr));
    
    //FILE* fd=(FILE*)(buff);

    printf("File descriptor: %s\n",buff);

    FILE *fptr2;
    sscanf(buff,"%p",&fptr2);

    printf("File descriptor2: %p\n",fptr2);

    fprintf(fptr2, "Some text");

    // Write some text to the file
    fclose(fptr);

    return 0;
}
