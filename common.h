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

//Struttura dati apertura file
typedef struct filehandler{
    int current_pos;//puntatore nel file, cambia nel tempo
    int last_pos;//dove stava il puntatore nel file, prima dell'apertura
    int ind_blocco;//indice blocco per dati, cambia...
    int size_old;//grandezza file, prima dell'apertura
}FileHandle;

//Struttura per file
struct FileData{
    char filename[50];  
    int size;//Dimensione del file, default e poi aumenta quando scrivi
    int start_block;//Blocco di partenza nella FAT
    int ind_puntatore;//Posizione del puntatore
    struct DirectoryData *par_directory;
};

//Struttura per directory
struct DirectoryData{
    char directoryname[50]; 
    struct FileData *files;
    struct DirectoryData *sub_directories;
    struct DirectoryData *par_directory;//NULL se root
};

//Struttura per FAT
struct FATEntry{
    int used;//0 indica che il blocco è libero, 1 indica che è allocato
    int next;//Numero del prossimo blocco nella catena di allocazione, default=-1
};

//Struttura per i blocchi di dati
struct DataBlock{
    char data[1024];//Blocco dati da 1024 byte
};

//FUNZIONI

//Funzioni controllo
int containsForbiddenChars(const char *input);

//Funzioni file
void createFile(char* filename, int size, struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks);
void eraseFile(char* filename, struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks);
void writeFile(char* filename,struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks); //potentially extending the file boundaries
//Scrive nel file partendo dal puntatore del file
void writeFileForm(char* filename,struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks);
void readFile(char* filename,struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks);
//Legge nel file partendo dal puntatore del file
void readFileFrom(char* filename,struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks);
//Sposta il puntatore dentro al file
void seekFile(char* filename,struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks);
struct FileData* searchFile(char* filename,struct DirectoryData* dir);

//Funzioni directory
void createDir(char* directoryname,struct DirectoryData* parentDirectory,struct DirectoryData* directory);
void eraseDir(char* directoryname,struct DirectoryData* parentDirectory,struct DirectoryData* directory,struct FATEntry* fat,struct DataBlock* dataBlocks);
void eraseDirAux(struct DirectoryData* dir,struct DirectoryData* directory,struct FATEntry* fat,struct DataBlock* dataBlocks);
struct DirectoryData* changeDir(struct DirectoryData* dir,char *dirname);
void listDir(char* dirname,struct DirectoryData* dir);

//Funzioni ausiliarie
FileHandle openFile(struct FileData* new_file,int mode,struct FATEntry* fat,struct DataBlock* dataBlocks);
//Copia che non modifica l'originale
struct DirectoryData* copy_dir(struct DirectoryData* dir);
//Puntatore della directory corrrente
struct DirectoryData* dir_corrente(struct DirectoryData* dir);
//Cerca in tutto il sistema
void searchFileAll(char* filename,struct DirectoryData* dir);
int searchFileAllAux(char* filename,struct DirectoryData* dir,char* per);
//Aggiunge un blocco al file se disponibile
int addBloccoVuoto(int ind_now,struct FATEntry* fat,struct DataBlock* dataBlocks);
