#include <errno.h>
#include <stdio.h>
#include <string.h>

//Macros for handling errors
#define handle_error_en(en, msg)    do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)
#define handle_error(msg)           do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define MAX_FILE 10//max file per directory
#define MAX_SUB 10//max subdirectory per directory
#define MAXENTRY 128//totale directory nel sistema
#define FILESIZE 512//grandezza iniziale del file
#define FATSIZE 1024//max blocchi disponibili
#define LENFAT (sizeof(struct FATEntry)*FATSIZE)
#define LENBLOCK (sizeof(struct DataBlock)*FATSIZE)
#define LENDIR (sizeof(struct DirectoryData)*MAXENTRY)

//Struttura dati apertura file
typedef struct filehandler{
    int current_pos;//puntatore nel file, cambia nel tempo
    int last_pos;//dove stava il puntatore nel file, prima dell'apertura
    int ind_blocco;//indice blocco per dati, cambia...
    int size_old;//grandezza file, prima dell'apertura
}FileHandle;

//Struttura per file
struct FileData{
    char filename[20];  
    int size;//Dimensione del file, default e poi aumenta quando scrivi
    int start_block;//Blocco di partenza nella FAT
    int ind_puntatore;//Posizione del puntatore
    struct DirectoryData *par_directory;
};

//Struttura per directory
struct DirectoryData{
    char directoryname[20];
    int dir_indice; 
    int parentdir_indice; 
    struct FileData files[10];
    struct DirectoryData *sub_directories[10];
    struct DirectoryData *par_directory;//NULL se root
};

//Struttura per FAT
struct FATEntry{
    int used;//0 indica che il blocco è libero,1 e occupato
    int next;//-1 indica non ha il prossimo, >0 indica il prossimo blocco
};

//Struttura per i blocchi di dati
struct DataBlock{
    char data[1024];//Blocco dati da 1024 byte
};

//Struttura che contiene tutto
struct FileSystem{
    struct DirectoryData directory[MAXENTRY];
    struct FATEntry fat[FATSIZE];
    struct DataBlock dataBlocks[FATSIZE];
};

//FUNZIONI

//Funzioni controllo
int containsForbiddenChars(const char *input);

//Funzioni file
void createFile(char* filename, int size, struct DirectoryData parentDirectory, void *mmapped_buffer);
void eraseFile(char* filename, struct DirectoryData parentDirectory, void *mmapped_buffer);
void writeFile(char* filename,struct DirectoryData parentDirectory, void *mmapped_buffer);
void readFile(char* filename,struct DirectoryData parentDirectory, void *mmapped_buffer);

//Scrive nel file partendo dal puntatore del file
void seekFile(char* filename,struct DirectoryData parentDirectory, void *mmapped_buffer);
void readFileFrom(char* filename,struct DirectoryData parentDirectory, void *mmapped_buffer);
void writeFileForm(char* filename,struct DirectoryData parentDirectory, void *mmapped_buffer);
int searchFile(char* filename,struct DirectoryData parentDirectory);

//Funzioni directory
void createDir(char* directoryname,struct DirectoryData parentDirectory,void *mmapped_buffer);
int eraseDir(char* directoryname,struct DirectoryData parentDirectory, void *mmapped_buffer);
void eraseDirAux(int ind,void *mmapped_buffer);
struct DirectoryData changeDir(struct DirectoryData dir,char *dirname,void *mmapped_buffer);
void listDir(char* dirname,struct DirectoryData dir,void *mmapped_buffer);

//Funzioni ausiliarie
FileHandle openFile(struct FileData new_file,int mode,void *mmapped_buffer);
struct DirectoryData puntatore_dir(int k,void *mmapped_buffer);

//Cerca in tutto il sistema
void searchFileAll(char* filename, void *mmapped_buffer);
int searchFileAllAux(char* filename,struct DirectoryData dir,char* per,void *mmapped_buffer);

//Aggiunge un blocco al file se disponibile
int addBloccoVuoto(int ind_now,void *mmapped_buffer);