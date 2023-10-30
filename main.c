#define _GNU_SOURCE
#define _POSIX_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdbool.h>
#include "common.h"

void *mmapped_buffer;//Disco
struct DirectoryData* directory;
struct FATEntry* fat;
struct DataBlock* dataBlocks;

//Inizializzare il sistema di file
void initializeFileSystem(){
    int fd=open("filesystem.bin",O_RDWR|O_CREAT|O_TRUNC,DEFFILEMODE);
    if(fd<0)handle_error("Errore apertura fd\n");
    //Dim max per le strutture dinamiche
    size_t directorySize=MAXENTRY*sizeof(struct DirectoryData);
    size_t fatSize=FATSIZE*sizeof(struct FATEntry);
    size_t dataBlockSize=FATSIZE*sizeof(struct DataBlock);
    size_t subdirectorySize=MAX_SUB*sizeof(struct DirectoryData);
    size_t fileSize=MAX_FILE*sizeof(struct FileData);
    //Memoria dinamica per le strutture
    directory=(struct DirectoryData*)malloc(directorySize);
    fat=(struct FATEntry*)malloc(fatSize);
    dataBlocks=(struct DataBlock*)malloc(dataBlockSize);
    //Inizializza le strutture
    memset(directory,0,directorySize);
    memset(fat,0,fatSize);
    memset(dataBlocks,0,dataBlockSize);
    //Inizializza array dinamici 
    for(int i=0; i<MAXENTRY; i++){
        directory[i].directoryname[0]='\0';//inizializzo le directory
        //file
        directory[i].files=(struct FileData*)malloc(fileSize);
        memset(directory[i].files,0,fileSize);
        //subdirectories
        directory[i].sub_directories=(struct DirectoryData*)malloc(subdirectorySize);
        memset(directory[i].sub_directories,0,subdirectorySize);
    }
    //Setta memoria
    int len=directorySize+fatSize+dataBlockSize;
    if((mmapped_buffer=mmap(NULL,len,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0))==MAP_FAILED)handle_error("Errore apertura mmap fd\n");
    if(ftruncate(fd,len)!=0)handle_error("Errore truncate fd\n");
    //Scrivi le strutture nella memoria mappata
    memcpy(mmapped_buffer,directory,directorySize);
    memcpy(mmapped_buffer+directorySize,fat,fatSize);
    memcpy(mmapped_buffer+directorySize+fatSize,dataBlocks,dataBlockSize);
    //Chiudi il file descriptor
    close(fd);
}

//Chiude il sistema di file
void freeFileSystem(){
    for (int i=0;i<MAXENTRY;i++){
        free(directory[i].files);
        free(directory[i].sub_directories);
    }
    free(directory);
    free(fat);
    free(dataBlocks);
    //Dealloca la memoria
    if (mmapped_buffer!=NULL){
        if(munmap(mmapped_buffer,(MAXENTRY*sizeof(struct DirectoryData))+(FATSIZE*sizeof(struct FATEntry))+(FATSIZE*sizeof(struct DataBlock)))==-1)handle_error("Error munmap fd\n");
        mmapped_buffer=NULL;
    }
    remove("filesystem.bin");//elimina il file
}

//Funzioni possibili
int funzDisponibili(struct DirectoryData* dir){
    if(dir==NULL)handle_error("Directory corrente inesistente\n");
    int subp=0,filep=0;//se 0 non ci sono se 1 ci sono
    for(int i=0;i<MAX_SUB;i++){//Controlla se esistono subdirectory
        if(dir->sub_directories[i].directoryname[0]!='\0'){
            subp=1;
            break;
        }
    }
    for(int i=0;i<MAX_FILE;i++){//Controlla se esistono file
        if(dir->files[i].size!=0){
            filep=1;
            break;
        }
    }
    if(subp==0 && filep==0)return 1;//vuoto
    else if(subp==0 && filep==1)return 2;//solo file
    else if(subp==1 && filep==0)return 3;//solo subdirectory
    else if(subp==1 && filep==1)return 4;//entrambe
    return -1;
}

int main(){
    char buffer[1024];//Input
    size_t buf_len = sizeof(buffer);
    initializeFileSystem();
    //Inizializza FAT
    for(int i=0;i<FATSIZE;i++){
        fat[i].used=0;
        fat[i].next=-1;
    }
    //Root
    strcpy(directory[0].directoryname,"root");
    directory[0].par_directory=NULL;
    //Inizializza root
    for(int i=0;i<MAX_FILE;i++){
        directory[0].files[i].size=0;//per controlli futuri
    }
    for(int i=0; i<MAX_SUB;i++){
        directory[0].sub_directories[i].directoryname[0]='\0';//per controlli futuri
    }
    struct DirectoryData *dir_root=dir_corrente(&directory[0]);
    struct DirectoryData *dir_corr=dir_corrente(&directory[0]);//Cambia con le operazioni
    printf("Creazione File System.....\n");
    //Inizio programma
    while(true){
        printf("***********************************************************************************************************\n");
        listDir("CORR",dir_corr);
        //Stabilisce funzioni disponibili
        int ope_disponibili=funzDisponibili(dir_corr);
        printf("Operazioni disponibili:\n");
        if(ope_disponibili==-1)handle_error("Errore controllo operazioni disponibili\n");
        if(ope_disponibili>=1){
            printf("\tCREATEFILE, crea un file nella directory %s\n",dir_corr->directoryname);
            printf("\tCREATEDIR, crea una subdirectory nella directory %s\n",dir_corr->directoryname);
            printf("\tLISTDIR, stampa la directory %s o una delle sue subdirectory\n",dir_corr->directoryname);
            printf("\tSEARCHFILEALL, cerca un file in tutto il file system\n");
        }
        if(ope_disponibili==4 || ope_disponibili==2){
            printf("\tSEARCHFILE, cerca il file nella directory %s\n",dir_corr->directoryname);
            printf("\tSEEKFILE, sposta il puntatore dentro al file, della directory %s\n",dir_corr->directoryname);
            printf("\tREADFILE, legge il file della directory %s\n",dir_corr->directoryname);
            printf("\tREADFILEP, legge il file partendo dal puntatore, della directory %s\n",dir_corr->directoryname);
            printf("\tWRITEFILE, scrive nel file della directory %s\n",dir_corr->directoryname);
            printf("\tWRITEFILEP, scrive nel file partendo dal puntatore, della directory %s\n",dir_corr->directoryname);
            printf("\tERASEFILE, cancella il file dalla directory %s\n",dir_corr->directoryname);
        }
        if(ope_disponibili==4 || ope_disponibili==3){
            printf("\tERASEDIR, cancella una subdirectory dalla directory %s\n",dir_corr->directoryname);
        }
        if(ope_disponibili==4 || ope_disponibili==3 || strcmp(dir_corr->directoryname,"root")!=0){
            printf("\tCHANGEDIR, torna indietro o si sposta in una subdirectory della directory %s\n",dir_corr->directoryname);
        }
        printf("Che vuoi fare, inserisci il nome della operazione in MAIUSCOLO o QUIT per chiudere? ");
        fflush(stdout);
        if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
            fprintf(stderr, "Error while reading from stdin, exiting...\n");
            exit(EXIT_FAILURE);
        }
        buffer[strlen(buffer)-1]='\0';
        if(strlen(buffer)==0)continue;
        if(strcmp(buffer,"QUIT")==0)break;
        //DEBUG printf("buffer: %s, numero: %d\n",buffer,ope_disponibili);
        //Controllo input
        if(ope_disponibili>=1 && strcmp(buffer,"CREATEFILE")==0){
            while(true){
                printf("Scrivi il nome del file: ");
                fflush(stdout);
                if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
                    fprintf(stderr, "Error while reading from stdin, exiting...\n");
                    exit(EXIT_FAILURE);
                }
                buffer[strlen(buffer)-1]='\0';
                if(strlen(buffer)!=0)break;
            }
            createFile(buffer,FILESIZE,dir_corr,fat,dataBlocks);
        }else if(ope_disponibili>=1 && strcmp(buffer,"CREATEDIR")==0){
            while(true){
                printf("Scrivi il nome della subdirectory: ");
                fflush(stdout);
                if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
                    fprintf(stderr, "Error while reading from stdin, exiting...\n");
                    exit(EXIT_FAILURE);
                }
                buffer[strlen(buffer)-1]= '\0';
                if(strlen(buffer)!=0)break; 
            }
            createDir(buffer,dir_corr,directory);
        }else if(ope_disponibili>=1 && strcmp(buffer,"LISTDIR")==0){
            while(true){
                printf("Scrivi il nome della subdirectory o CORR per stampare quella corrente: ");
                if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
                    fprintf(stderr, "Error while reading from stdin, exiting...\n");
                    exit(EXIT_FAILURE);
                }
                buffer[strlen(buffer)-1]='\0';
                if(strlen(buffer)!=0)break; 
            }
            listDir(buffer,dir_corr);
        }else if(ope_disponibili>=1 && strcmp(buffer,"SEARCHFILEALL")==0){
            while(true){
                printf("Scrivi il nome del file da cercare nel file system: ");
                fflush(stdout);
                if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
                    fprintf(stderr, "Error while reading from stdin, exiting...\n");
                    exit(EXIT_FAILURE);
                }
                buffer[strlen(buffer)-1]='\0';
                if(strlen(buffer)!=0)break;
            }
            searchFileAll(buffer,dir_root);
        }else if((ope_disponibili==2 || ope_disponibili==4) && strcmp(buffer,"SEARCHFILE")==0){
            while(true){
                printf("Scrivi il nome del file da cercare: ");
                if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
                    fprintf(stderr, "Error while reading from stdin, exiting...\n");
                    exit(EXIT_FAILURE);
                }
                buffer[strlen(buffer)-1]='\0';
                if(strlen(buffer)!=0)break; 
            }
            struct FileData* file=searchFile(buffer,dir_corr);
            if(file==NULL)printf("Non ho trovato il file\n");
            else printf("Ho trovato il file\n");
        }else if((ope_disponibili==2 || ope_disponibili==4) && strcmp(buffer,"SEEKFILE")==0){
            while(true){
                printf("Scrivi il nome del file a cui spostare il puntatore: ");
                fflush(stdout);
                if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
                    fprintf(stderr, "Error while reading from stdin, exiting...\n");
                    exit(EXIT_FAILURE);
                }
                buffer[strlen(buffer)-1]='\0';
                if(strlen(buffer)!=0)break; 
            }
            seekFile(buffer,dir_corr,fat,dataBlocks);
        }else if((ope_disponibili==2 || ope_disponibili==4) && strcmp(buffer,"READFILE")==0){
            while(true){
                printf("Scrivi il nome del file da leggere: ");
                if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
                    fprintf(stderr, "Error while reading from stdin, exiting...\n");
                    exit(EXIT_FAILURE);
                }
                buffer[strlen(buffer)-1]='\0';
                if(strlen(buffer)!=0)break; 
            }
            readFile(buffer,dir_corr,fat,dataBlocks);
        }else if((ope_disponibili==2 || ope_disponibili==4) && strcmp(buffer,"READFILEP")==0){
            while(true){
                printf("Scrivi il nome del file da leggere partendo dal suo puntatore: ");
                fflush(stdout);
                if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
                    fprintf(stderr, "Error while reading from stdin, exiting...\n");
                    exit(EXIT_FAILURE);
                }
                buffer[strlen(buffer)-1]='\0';
                if(strlen(buffer)!=0)break; 
            }
            readFileFrom(buffer,dir_corr,fat,dataBlocks);
        }else if((ope_disponibili==2 || ope_disponibili==4) && strcmp(buffer,"WRITEFILE")==0){
            while(true){
                printf("Scrivi il nome del file su cui scrivere: ");
                if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
                    fprintf(stderr, "Error while reading from stdin, exiting...\n");
                    exit(EXIT_FAILURE);
                }
                buffer[strlen(buffer)-1]='\0';
                if(strlen(buffer)!=0)break; 
            }
            writeFile(buffer,dir_corr,fat,dataBlocks);  
        }else if((ope_disponibili==2 || ope_disponibili==4) && strcmp(buffer,"WRITEFILEP")==0){
            while(true){
                printf("Scrivi il nome del file su cui scrivere partendo dal puntatore: ");
                fflush(stdout);
                if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
                    fprintf(stderr, "Error while reading from stdin, exiting...\n");
                    exit(EXIT_FAILURE);
                }
                buffer[strlen(buffer)-1]='\0';
                if(strlen(buffer)!=0)break; 
            }
            writeFileForm(buffer,dir_corr,fat,dataBlocks);
        }else if((ope_disponibili==2 || ope_disponibili==4) && strcmp(buffer,"ERASEFILE")==0){
            while(true){
                printf("Scrivi il nome del file da eliminare: ");
                if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
                    fprintf(stderr, "Error while reading from stdin, exiting...\n");
                    exit(EXIT_FAILURE);
                }
                buffer[strlen(buffer)-1]='\0';
                if(strlen(buffer)!=0)break; 
            }
            eraseFile(buffer,dir_corr,fat,dataBlocks);
            /*DEBUG
            for(int i=0;i<FATSIZE;i++){
                if(fat[i].used!=0)printf("\tfat[%d]\n",i);
            }*/
        }else if((ope_disponibili==3 || ope_disponibili==4 || strcmp(dir_corr->directoryname,"root")!=0) && strcmp(buffer,"CHANGEDIR")==0){
            while(true){
                if(strcmp(dir_corr->directoryname,"root")!=0 && dir_corr->par_directory!=NULL) printf("Scrivi il nome della subdirectory o PREC per andare alla parentdirectory %s: ",dir_corr->par_directory->directoryname);
                else printf("Scrivi il nome della subdirectory: ");
                if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
                    fprintf(stderr, "Error while reading from stdin, exiting...\n");
                    exit(EXIT_FAILURE);
                }
                buffer[strlen(buffer)-1]='\0';
                if(strlen(buffer)!=0)break; 
            }
            dir_corr=changeDir(dir_corr,buffer);
        }else if((ope_disponibili==3 || ope_disponibili==4) && strcmp(buffer,"ERASEDIR")==0){
            while(true){
                printf("Scrivi il nome della subdirectory da eliminare: ");
                if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
                    fprintf(stderr, "Error while reading from stdin, exiting...\n");
                    exit(EXIT_FAILURE);
                }
                buffer[strlen(buffer)-1]='\0';
                if(strlen(buffer)!=0)break; 
            }
            eraseDir(buffer,dir_corr,directory,fat,dataBlocks);
            /*DEBUG
            for(int i=0;i<MAXENTRY;i++){
                if(directory[i].directoryname[0]!='\0'){
                    printf("\tdir: %s\n",directory[i].directoryname);//directory presenti
                    for(int s=0;s<MAX_SUB;s++){//sub_dir presenti
                        if (directory[i].sub_directories[s].directoryname[0]!='\0'){
                            printf("\t\tsubdir: %s\n",directory[i].sub_directories[s].directoryname);
                        }
                    }
                    for(int f=0;f<MAX_FILE;f++){//file presenti
                        if (directory[i].files[f].size!=0){
                            printf("\t\tfile: %s\n",directory[i].files[f].filename);
                        }
                    } 
                }
            }
            for(int i=0;i<FATSIZE;i++){//blocchi occupati
                if(fat[i].used!=0)printf("\tfat[%d]\n",i);
            }
            for(int i=0;i<FATSIZE;i++){//dati nei blocchi
                printf("data[%d]=%s\n",i,dataBlocks[i].data);
            }
            */
        }
    }
    //Fine programma
    printf("Chiusura File System.....\n");
    freeFileSystem();
    return 0;
}  
