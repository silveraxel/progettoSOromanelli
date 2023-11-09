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
int len,fd;
struct FileSystem fs;

//Inizializzare il sistema di file
void creazioneSistema(){
    fd=open("filesystem.bin",O_RDWR|O_CREAT,0644);
    if(fd<0)handle_error("Errore: Crea fd\n");
    //Dim max per le strutture 
    len=sizeof(struct FileSystem);
    //Setta memoria
    if(ftruncate(fd,len)!=0)handle_error("Errore: Truncate fd\n");
    if((mmapped_buffer=mmap(NULL,len,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0))==MAP_FAILED)handle_error("Errore apertura mmap fd\n");
    //Inizializza le strutture
    for(int i=0; i<MAXENTRY; i++){
        //inizializzo le fs.directory
        fs.directory[i].directoryname[0]='\0';//per controlli futuri
        //file
        for(int j=0;j<MAX_FILE;j++){
            fs.directory[i].files[j].size=0;//per controlli futuri
        }
        //subdirectories
        for(int j=0;j<MAX_SUB;j++){
            fs.directory[i].sub_directories[j]=(struct DirectoryData*)malloc(sizeof(struct DirectoryData));
            if(fs.directory[i].sub_directories[j]==NULL)handle_error("Errore: Allocazione struttura sub directory della directory\n");
            //memset(directory[i].sub_directories[j],0,sizeof(struct DirectoryData));
            fs.directory[i].sub_directories[j]->directoryname[0]='\0';//per controlli futuri
        }
    }
}

//Setto valori iniziali 
void initializeFileSystem(){
    //Inizializza FAT
    for(int i=0;i<FATSIZE;i++){
        fs.fat[i].used=0;
        fs.fat[i].next=-1;
    }
    //Root
    strcpy(fs.directory[0].directoryname,"root");
    fs.directory[0].par_directory=NULL;
    fs.directory[0].dir_indice=0;
    fs.directory[0].parentdir_indice=-1;//Solo root
}

//Recupero del file system
void recuperoSistema(){
    fd=open("filesystem.bin",O_RDWR,0644);
    if(fd<0)handle_error("Errore: Crea fd\n");
    //Dim max per le strutture 
    len=sizeof(struct FileSystem);
    if((mmapped_buffer=mmap(NULL,len,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0))==MAP_FAILED)handle_error("Errore recupero mmap fd\n");
    memcpy(&fs, mmapped_buffer, len);
    for(int i=0; i<MAXENTRY; i++){
        //Aggiorniamo il valore null
        //subdirectories
        for(int j=0;j<MAX_SUB;j++){
            fs.directory[i].sub_directories[j]=(struct DirectoryData*)malloc(sizeof(struct DirectoryData));
            if(fs.directory[i].sub_directories[j]==NULL)handle_error("Errore: Allocazione struttura sub directory della directory\n");
            fs.directory[i].sub_directories[j]->directoryname[0]='\0';//per controlli futuri
        }
    }
}

void recuperoGerarchia(){
    for (int i=0; i<MAXENTRY;i++){
        for (int j=0; j<MAX_FILE;j++){
            if(fs.directory[i].files[j].size>0)fs.directory[i].files[j].par_directory=&fs.directory[i];
        }
        if(fs.directory[i].parentdir_indice!=-1){
            int padre=fs.directory[i].parentdir_indice;
            fs.directory[i].par_directory=&fs.directory[padre];
            for (int j=0; j<MAX_SUB;j++){
                if(fs.directory[padre].sub_directories[j]->directoryname[0]=='\0'){
                    fs.directory[padre].sub_directories[j]=&fs.directory[i];
                    break;
                }
            }
        }
    }
}

//Chiudo e dealloco
void freeFileSystem(){  
    //DEVO ANNULLARE I PUNTATORI
    for (int i=0;i<MAXENTRY;i++){
        fs.directory[i].par_directory=NULL;
        for(int j=0;j<MAX_SUB;j++){
            fs.directory[i].sub_directories[j]=NULL;
        }
        for(int j=0;j<MAX_FILE;j++){
            fs.directory[i].files[j].par_directory=NULL;
        }
    }
    //modifico la memoria
    memcpy(mmapped_buffer, &fs,sizeof(struct FileSystem));
    if(msync(mmapped_buffer,len,MS_SYNC) != 0)handle_error("Errore: msync");
    //Dealloca la memoria
    if(mmapped_buffer!=NULL){
        if(munmap(mmapped_buffer,len)==-1)handle_error("Error munmap fd\n");
    }
    //Chiudi il file descriptor
    close(fd);
}

//Funzioni possibili
int funzDisponibili(struct DirectoryData* dir){
    if(dir==NULL)handle_error("Errore: Directory corrente inesistente\n");
    int subp=0,filep=0;//se 0 non ci sono se 1 ci sono
    for(int i=0;i<MAX_SUB;i++){//Controlla se esistono subfs.directory
        if(dir->sub_directories[i]->directoryname[0]!='\0'){
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
    else if(subp==1 && filep==0)return 3;//solo subfs.directory
    else if(subp==1 && filep==1)return 4;//entrambe
    return -1;
}

int main(int argc,char *argv[]){
    char buffer[1024];//Input
    size_t buf_len = sizeof(buffer);
    int erase=-1;
    if(argc>1)erase=atoi(argv[1]);
    //CREAZIONE DISCO
    if(erase==1){
        creazioneSistema();
        initializeFileSystem();
    }else{
        recuperoSistema();
        recuperoGerarchia();
    }
    //FINE DISCO
    struct DirectoryData *dir_root=dir_corrente(&fs.directory[0]);
    if(dir_root==NULL)handle_error("Errore: Inizializzazione variabile dir_root\n");
    struct DirectoryData *dir_corr=dir_corrente(&fs.directory[0]);//Cambia con le operazioni
    if(dir_root==NULL)handle_error("Errore: Inizializzazione variabile dir_corr\n");
    printf("Creazione File System.....\n");
    //Inizio programma
    while(true){
        printf("***********************************************************************************************************\n");
        listDir("CORR",dir_corr);
        //Stabilisce funzioni disponibili
        int ope_disponibili=funzDisponibili(dir_corr);
        printf("\nOperazioni disponibili:\n");
        if(ope_disponibili==-1)handle_error("Errore: Controllo operazioni disponibili\n");
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
            createFile(buffer,FILESIZE,dir_corr,fs.fat,fs.dataBlocks);
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
            createDir(buffer,dir_corr,fs.directory);
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
            seekFile(buffer,dir_corr,fs.fat,fs.dataBlocks);
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
            readFile(buffer,dir_corr,fs.fat,fs.dataBlocks);
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
            readFileFrom(buffer,dir_corr,fs.fat,fs.dataBlocks);
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
            writeFile(buffer,dir_corr,fs.fat,fs.dataBlocks);
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
            writeFileForm(buffer,dir_corr,fs.fat,fs.dataBlocks);
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
            eraseFile(buffer,dir_corr,fs.fat,fs.dataBlocks);
            /*DEBUG
            for(int i=0;i<FATSIZE;i++){
                if(fat[i].used!=0)printf("\tfat[%d]\n",i);
            }*/
        }else if((ope_disponibili==3 || ope_disponibili==4 || strcmp(dir_corr->directoryname,"root")!=0) && strcmp(buffer,"CHANGEDIR")==0){
            while(true){
                if(strcmp(dir_corr->directoryname,"root")!=0 && dir_corr->par_directory!=NULL) printf("Scrivi il nome della subfs.directory o PREC per andare alla parentfs.directory %s: ",dir_corr->par_directory->directoryname);
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
            eraseDir(buffer,dir_corr,fs.directory,fs.fat,fs.dataBlocks);
            /*DEBUG
            for(int i=0;i<MAXENTRY;i++){
                if(fs.directory[i].fs.directoryname[0]!='\0'){
                    printf("\tdir: %s\n",fs.directory[i].fs.directoryname);//fs.directory presenti
                    for(int s=0;s<MAX_SUB;s++){//sub_dir presenti
                        if (fs.directory[i].sub_directories[s].fs.directoryname[0]!='\0'){
                            printf("\t\tsubdir: %s\n",fs.directory[i].sub_directories[s].fs.directoryname);
                        }
                    }
                    for(int f=0;f<MAX_FILE;f++){//file presenti
                        if (fs.directory[i].files[f].size!=0){
                            printf("\t\tfile: %s\n",fs.directory[i].files[f].filename);
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
        sleep(1);
    }
    //Fine programma
    printf("Chiusura File System.....\n");
    freeFileSystem();
    return 0;
}  