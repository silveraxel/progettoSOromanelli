#include <errno.h>
#include <stdio.h>
#include <string.h>

// macros for handling errors
#define handle_error_en(en, msg)    do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)
#define handle_error(msg)           do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define MAX_FILE 10//max sub per directory
#define MAX_SUB 10//max sub per directory
#define MAXENTRY 128//totale dir nel sistema
#define FILESIZE 512//file size iniziale
#define FATSIZE 1024//max blocchi disponibili
#define DATABLOCKSIZE 10//size blocchi data

//Struttura dati return apertura file
typedef struct filehandler{
    int current_pos;//puntatore nel file, si sposta
    int last_pos;//dove stava il puntatore nel file, rimane fermo
    int ind_blocco;
    int size_old;
}FileHandle;

//Struttura per i metadati del file
struct FileData{
    char filename[50];  
    int size;//Dimensione del file, default e poi aumenta quando scrivi
    int start_block;//Blocco di partenza nella FAT
    int ind_puntatore;
    struct DirectoryData *par_directory;
};

//Struttura per i metadati del file
struct DirectoryData{
    char directoryname[50]; 
    struct FileData *files;
    struct DirectoryData *sub_directories;
    struct DirectoryData *par_directory;//NULL se root
};

//Struttura per la "FAT"
struct FATEntry{
    int used;//0 indica che il blocco è libero, 1 indica che è allocato
    int next;//Numero del prossimo blocco nella catena di allocazione, default=-1
};

struct DataBlock{
    char data[1024];//blocco dati da 1024 byte
};

//Funzioni
int containsForbiddenChars(const char *input);
void createFile(char* filename, int size, struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks);
void eraseFile(char* filename, struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks);
int addBloccoVuoto(int ind_now,struct FATEntry* fat,struct DataBlock* dataBlocks);
void writeFile(char* filename,struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks); //potentially extending the file boundaries
void writeFileForm(char* filename,struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks);
void readFile(char* filename,struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks);
void readFileFrom(char* filename,struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks);
void seekFile(char* filename,struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks);
struct FileData* searchFile(char* filename,struct DirectoryData* dir);
void searchFileAll(char* filename,struct DirectoryData* dir);
int searchFileAllAux(char* filename,struct DirectoryData* dir,char* per);
FileHandle openFile(struct FileData* new_file,int mode,struct FATEntry* fat,struct DataBlock* dataBlocks);
void createDir(char* directoryname,struct DirectoryData* parentDirectory,struct DirectoryData* directory);

void eraseDir(char* directoryname,struct DirectoryData* parentDirectory,struct DirectoryData* directory,struct FATEntry* fat,struct DataBlock* dataBlocks);
void eraseDirAux(struct DirectoryData* dir,struct DirectoryData* directory,struct FATEntry* fat,struct DataBlock* dataBlocks);

struct DirectoryData* changeDir(struct DirectoryData* dir,char *dirname);
void listDir(char* dirname,struct DirectoryData* dir);

//funzioni ausiliarie
struct DirectoryData* copy_dir(struct DirectoryData* dir);
struct DirectoryData* dir_corrente(struct DirectoryData* dir);