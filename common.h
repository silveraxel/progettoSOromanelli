#include <errno.h>
#include <stdio.h>
#include <string.h>
#define _POSIX_SOURCE
#include <dirent.h>

// macros for handling errors
#define handle_error_en(en, msg)    do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)
#define handle_error(msg)           do { perror(msg); exit(EXIT_FAILURE); } while (0)

//Funzioni da implementare

FILE * createFile();
FILE * eraseFile();

void writeFile();
void readFile();
void seekFile();

DIR * createDirectory();
DIR * eraseDirectory();
DIR * changeDirectory();
DIR * listDirectory();

//Funzioni ausiliarie

void printCornice();

void rek_mkdir(char *path);

FILE *fopen_mkdir(char *path, char *mode);

void stampaFileDirectory();