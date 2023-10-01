#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#define _POSIX_SOURCE
#include <dirent.h>


FILE* createFile(){}
FILE * eraseFile(){}

void writeFile(){}
void readFile(){}
void seekFile(){}

DIR * createDirectory(){}
DIR * eraseDirectory(){}
DIR * changeDirectory(){}
DIR * listDirectory(){}





// funzioni ausiliarie 

void printCornice(){
    printf("**********************************************************\n");
}


void rek_mkdir(char *path) {
    char *sep = strrchr(path, '/');
    if(sep != NULL) {
        *sep = 0;
        rek_mkdir(path);
        *sep = '/';
    }
    if(mkdir(path, 0777) && errno != EEXIST)
        printf("error while trying to create '%s'\n%m\n", path); 
}

FILE *fopen_mkdir(char *path, char *mode) {
    char *sep = strrchr(path, '/');
    if(sep) { 
        char *path0 = strdup(path);
        path0[ sep - path ] = 0;
        rek_mkdir(path0);
        free(path0);
    }
    return fopen(path,mode);
}

DIR *directory;
    struct dirent *file;
    struct stat info;
    int entries = 0 ;

    // entering the directory
    directory = opendir("//home//richard" );
    if ( directory == NULL )
    {
        perror("the directory couldn't be accessed or does not exist");
        return(2);
    }

void stampaFileDirectory(){
    printf("No   type         name              size           TimeStamp \n\n");
    while((file = readdir(directory)))
    {
        if( file->d_name[0] == '.' )
        { // then hidden file, so leave hidden
            continue;
        }

        entries++;

        char buffer[1024];
        strcpy( buffer, "//home//richard//" );
        strcat( buffer, file->d_name );
        if (stat( buffer, &info ) == -1)
        {
            perror( buffer );
            continue;
        }
        // show the number of the entry
        printf("%2d  ",entries);

        // determine if file or directory
        if(S_ISDIR(info.st_mode))
            printf("Dir ");
        else
            printf("File");

        // display the name of the file
        printf("%20s",file->d_name);

        // display the size of the file
        printf("%10ld",info.st_size);

        // show the last modified time
        if(!(S_ISDIR(info.st_mode)))
            printf("%30s\n",ctime(&info.st_mtime));
        else puts("\n");
    }
}