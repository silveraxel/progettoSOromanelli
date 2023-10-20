#define _GNU_SOURCE
#define _POSIX_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdbool.h>
#include "common.h"



//funzioni controlli
int containsForbiddenChars(const char *input){
    //controllo caratteri speciali
    const char *forbiddenChars = ",?|/:*\"\\<>";
    for (int i = 0; i < strlen(forbiddenChars); i++) {
        if (strchr(input, forbiddenChars[i]) != NULL) {
            printf("Non puoi inserire uno di questi caratteri nel nome: %s. ",forbiddenChars);
            return 1; // Trovato un carattere vietato
        }
    }
    return 0; // Nessun carattere trovato
}

void createFile(char* filename, int size, struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks){
    if(parentDirectory==NULL)handle_error("parentDirectory inesistente\n");
    if(containsForbiddenChars(filename)==1){//scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    //verifica esistenza del nome
    if(strlen(filename)<sizeof(parentDirectory->files[0].filename)){
        printf("Errore: Il nome del file non va bene... Operazione annullata\n");
        for(int i=0; i<MAX_FILE;i++){
            if(parentDirectory->files[i].size!=0 && strcmp(parentDirectory->files[i].filename,filename)==0){
                printf("Errore: Cambia il nome del file, gia' esiste\n");
                return;
            }
        }
    }else{
        printf("Errore: Nome della directory troppo lungo\n");
        return;
    }
    //cerca uno spazio libero nella directory
    int k=-1;
    for (int i=0; i<MAX_FILE;i++){
        if(parentDirectory->files[i].size==0){//implica vuoto
            k=i;
            break;
        }
    }
    if(k==-1){
        printf("Errore: spazio insufficiente nella directory per un nuovo file\n");
        return;
    }
    //cerca blocchi liberi nella FAT
    //calcola quanti blocchi sono necessari
    int necessari=(int)ceil((double)(size+sizeof(struct DataBlock)-1)/sizeof(struct DataBlock));  
    int bstart=-1,bsecond=-1;
    int allocati=0;
    for(int i=0;i<FATSIZE;i++){//servono 2 blocchi, poi aumentera'
        if(fat[i].used==0){
            allocati++;
            if(allocati==1){
                bstart=i;
            }else if(allocati==2){
                bsecond=i;
                break;
            }
        }
    }
    //controllo pre-assegnazione
    if(bstart==-1 || bsecond==-1){
        printf("Errore: spazio insufficiente nella fat per un nuovo file\n");
        return;
    }
    //conferma posizione nella fat
    fat[bstart].used=1;
    fat[bstart].next=bsecond;
    fat[bsecond].used=1;
    //assegna valori al file
    struct FileData new_file;
    if(strncpy(new_file.filename,filename,sizeof(new_file.filename))==NULL)handle_error("Errore copia nome creazione\n");
    new_file.size=size;
    new_file.start_block=bstart;
    new_file.ind_puntatore=0;
    new_file.par_directory=parentDirectory;
    parentDirectory->files[k]=new_file;//aggiorno la directory
    printf("Creazione del file %s con successo\n",filename);
}

struct FileData* searchFile(char* filename,struct DirectoryData* dir){
    struct FileData* new_file=NULL;
    for(int i=0;i<MAX_FILE;i++){
        if(strcmp(dir->files[i].filename,filename)==0){
            new_file=&(dir->files[i]);
            return new_file;
        }
    }
    return new_file;
}

void searchFileAll(char* filename,struct DirectoryData* dir){
    printf("Presenza di file con nome %s nel sistema\n",filename);
    int ret=searchFileAllAux(filename,dir,"");
    if(ret==0)printf("Non ho trovato nessun file con nome %s\n",filename);
    printf("Ricerca finita\n");
}

int searchFileAllAux(char* filename,struct DirectoryData* dir,char* per){
    struct FileData* new_file=NULL;
    int ret=0;
    char percorso[1024];
    strcpy(percorso,per);
    strcat(percorso,"/");
    strncat(percorso,dir->directoryname,strlen(dir->directoryname));
    new_file=searchFile(filename,dir);
    if(new_file!=NULL){
        printf("Esiste il file %s nella directory %s\n",filename,percorso);
        ret++;
    }
    for(int i=0; i<MAX_SUB;i++){
        if(dir->sub_directories[i].directoryname[0]!='\0'){
            ret+=searchFileAllAux(filename,&dir->sub_directories[i],percorso);
        }
    }
    return ret;
}

FileHandle openFile(struct FileData* new_file,int mode,struct FATEntry* fat,struct DataBlock* dataBlocks){
    FileHandle fh;
    fh.ind_blocco=new_file->start_block;
    fh.size_old=0;
    int count=new_file->start_block;
    while(count!=-1){//lunghezza file
        int len=0;
        //calcola lunghezza dataBlock.data
        while (len<sizeof(dataBlocks[count].data) && dataBlocks[count].data[len] != '\0') {
            len++;
        }
        fh.size_old+=len;
        count=fat[count].next;
    }
    fh.last_pos=new_file->ind_puntatore;
    if(mode==0){//lettura
        fh.current_pos=0;
    }else{//scrittura   
        fh.current_pos=fh.size_old;//punto dopo l'utltimo carattere
    }
    
    return fh;
}

void eraseFile(char* filename, struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks){
    if(parentDirectory==NULL)handle_error("parentDirectory inesistente\n");
    if(containsForbiddenChars(filename)==1){//scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    struct FileData* new_file=searchFile(filename,parentDirectory);
    if(new_file==NULL){
        printf("Non esiste il file %s nella directory %s\n",filename,parentDirectory->directoryname);
        return;
    }
    if(new_file->size==0){
        printf("Errore: spazio per file gia' vuoto\n");
        return;
    }
    //memset(directory[i].files,0,fileSize);
    int ind=new_file->start_block;
    while(ind!=-1){//libero la memoria
        memset(&dataBlocks[ind],0,sizeof(struct DataBlock));
        fat[ind].used=0;
        int temp=fat[ind].next;
        fat[ind].next=-1;    
        ind=temp;
    }
    memset(new_file,0,sizeof(struct FileData));
    new_file->size=0;//addora
    printf("File %s eliminato\n",filename);
}

int addBloccoVuoto(int ind_now,struct FATEntry* fat,struct DataBlock* dataBlocks){
    //cerca nella fat
    int k=-1;
    for(int i=0;i<FATSIZE;i++){//servono 2 blocchi, poi aumentera'
        if(fat[i].used==0){
            k=i;
            break;
        }
    }
    //controllo pre-assegnazione
    if(k==-1){
        printf("Errore: spazio insufficiente nella fat per aumentare lo spazio nel file... Operazione annullata\n");
        return -1;
    }
    fat[ind_now].next=k;
    fat[k].used=1;
    return 0;
}

void writeFile(char* filename,struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks){
    if(parentDirectory==NULL)handle_error("parentDirectory inesistente\n");
    if(containsForbiddenChars(filename)==1){//scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    struct FileData* new_file=searchFile(filename,parentDirectory);
    if(new_file==NULL){
        printf("Non esiste il file %s nella directory %s\n",filename,parentDirectory->directoryname);
        return;
    }
    //il file esiste quindi scrivo
    FileHandle fh=openFile(new_file,1,fat,dataBlocks);
    if(fh.ind_blocco==-1){
        printf("Errore: Non riesce ad aprire il file\n");
        return;
    }
    printf("Scrittura del file %s\n",filename);
    //lavoro sporco
    char buffer[1024];
    printf("Input nel file, max 1023 caratteri(write): ");
    if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
        fprintf(stderr, "Error while reading from stdin, exiting...\n");
        exit(EXIT_FAILURE);
    }
    //calcolo
    if(fh.size_old==0){
        strncpy(dataBlocks[fh.ind_blocco].data, buffer,strlen(buffer));
        printf("File inizializzato correttamente... ");
    }else{
        int blocchi_pieni=(int)floor((double)fh.size_old/1024);
        int ret=0;
        while(blocchi_pieni>0){
            fh.current_pos-=1024;
            fh.ind_blocco=fat[fh.ind_blocco].next;
            //debug printf("indice blocco(spostamento):%d\n",fh.ind_blocco);
            ret=1;
            blocchi_pieni--;
        }
        if(ret==0){//non ha blocchi pieni==>ha un blocco vuoto
            strncpy(dataBlocks[fh.ind_blocco].data+fh.current_pos,buffer,sizeof(dataBlocks[fh.ind_blocco].data)-fh.current_pos);
            if(strlen(buffer)+fh.current_pos>1024){
                fh.ind_blocco=fat[fh.ind_blocco].next;
                strncpy(dataBlocks[fh.ind_blocco].data,buffer+(sizeof(dataBlocks[fh.ind_blocco].data)-fh.current_pos),strlen(buffer)-(sizeof(dataBlocks[fh.ind_blocco].data)-fh.current_pos));
            }
        }else if(ret==1){//ha almeno un blocco pieno==>no blocco vuoto
            strncpy(dataBlocks[fh.ind_blocco].data+fh.current_pos,buffer,sizeof(dataBlocks[fh.ind_blocco].data)-fh.current_pos);
            if(strlen(buffer)+fh.current_pos>1024){
                if(addBloccoVuoto(fh.ind_blocco,fat,dataBlocks)==-1)return;
                fh.ind_blocco=fat[fh.ind_blocco].next;
                strncpy(dataBlocks[fh.ind_blocco].data,buffer+(sizeof(dataBlocks[fh.ind_blocco].data)-fh.current_pos),strlen(buffer)-(sizeof(dataBlocks[fh.ind_blocco].data)-fh.current_pos));
            }else if(strlen(buffer)+fh.current_pos==1024 && fat[fh.ind_blocco].next==-1){
                if(addBloccoVuoto(fh.ind_blocco,fat,dataBlocks)==-1)return;
            }
        }
    }
    new_file->ind_puntatore+=strlen(buffer);
    printf("Operazione conclusa con successo\n");
}

void writeFileForm(char* filename,struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks){
    if(parentDirectory==NULL)handle_error("parentDirectory inesistente\n");
    if(containsForbiddenChars(filename)==1){//scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    struct FileData* new_file=searchFile(filename,parentDirectory);
    if(new_file==NULL){
        printf("Non esiste il file %s nella directory %s\n",filename,parentDirectory->directoryname);
        return;
    }
    //il file esiste quindi scrivo
    FileHandle fh=openFile(new_file,1,fat,dataBlocks);
    if(fh.ind_blocco==-1){
        printf("Errore: Non riesce ad aprire il file\n");
        return;
    }
    printf("Scrittura del file %s\n",filename);
    //lavoro sporco
    char buffer[1024];
    printf("Input nel file, max 1023 caratteri(write): ");
    if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
        fprintf(stderr, "Error while reading from stdin, exiting...\n");
        exit(EXIT_FAILURE);
    }
    int nblocchi=(int)floor((double)fh.last_pos/1024);
    while(nblocchi>0){
        fh.last_pos-=1024;
        fh.ind_blocco=fat[fh.ind_blocco].next;
        nblocchi--;
    }
    char temp[1024];
    strncpy(temp,dataBlocks[fh.ind_blocco].data,sizeof(dataBlocks[0].data));
    //debug printf("%s\n",temp);
    strncpy(dataBlocks[fh.ind_blocco].data+fh.last_pos,buffer,sizeof(dataBlocks[fh.ind_blocco].data)-fh.last_pos);
    if(strlen(buffer)+fh.last_pos>1024){
        if(fat[fh.ind_blocco].next==-1){
            if(addBloccoVuoto(fh.ind_blocco,fat,dataBlocks)==-1)return;
        }
        fh.ind_blocco=fat[fh.ind_blocco].next;
        strncpy(temp,dataBlocks[fh.ind_blocco].data,sizeof(dataBlocks[0].data));
        strncpy(dataBlocks[fh.ind_blocco].data,buffer+(sizeof(dataBlocks[fh.ind_blocco].data)-fh.last_pos),strlen(buffer)-(sizeof(dataBlocks[fh.ind_blocco].data)-fh.last_pos));
        strncpy(dataBlocks[fh.ind_blocco].data+(strlen(buffer)-(sizeof(dataBlocks[fh.ind_blocco].data)-fh.last_pos)),temp+(strlen(buffer)-(sizeof(dataBlocks[fh.ind_blocco].data)-fh.last_pos)),sizeof(dataBlocks[fh.ind_blocco].data)-(strlen(buffer)-(sizeof(dataBlocks[fh.ind_blocco].data)-fh.last_pos)));
    }else{
        strncpy(dataBlocks[fh.ind_blocco].data+(fh.last_pos+strlen(buffer)),temp+(fh.last_pos+strlen(buffer)),sizeof(dataBlocks[fh.ind_blocco].data)-(fh.last_pos+strlen(buffer)));
    }
    new_file->ind_puntatore+=strlen(buffer);
    printf("Operazione conclusa con successo\n");
}

void readFile(char* filename,struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks){
    if(parentDirectory==NULL)handle_error("parentDirectory inesistente\n");
    if(containsForbiddenChars(filename)==1){//scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    struct FileData* new_file=searchFile(filename,parentDirectory);
    if(new_file==NULL){
        printf("Non esiste il file %s nella directory %s\n",filename,parentDirectory->directoryname);
        return;
    }
    //il file esiste quindi leggo
    FileHandle fh=openFile(new_file,0,fat,dataBlocks);
    if(fh.ind_blocco==-1){
        printf("Errore: Non riesce ad aprire il file\n");
        return;
    }
    printf("Lettura del file %s\n",filename);
    while(fh.ind_blocco!=-1){
        for(int i=0;i<1024;i++){
            if(dataBlocks[fh.ind_blocco].data[i]==0)break;
            printf("%c",dataBlocks[fh.ind_blocco].data[i]);
        }
        fh.ind_blocco=fat[fh.ind_blocco].next;
    }
    printf("Operazione conclusa con successo\n");
}

void readFileFrom(char* filename,struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks){
    if(parentDirectory==NULL)handle_error("parentDirectory inesistente\n");
    if(containsForbiddenChars(filename)==1){//scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    struct FileData* new_file=searchFile(filename,parentDirectory);
    if(new_file==NULL){
        printf("Non esiste il file %s nella directory %s\n",filename,parentDirectory->directoryname);
        return;
    }
    //il file esiste quindi leggo
    FileHandle fh=openFile(new_file,0,fat,dataBlocks);
    if(fh.ind_blocco==-1){
        printf("Errore: Non riesce ad aprire il file\n");
        return;
    }
    printf("Lettura del file %s\n",filename);
    int nblocchi=(int)floor((double)fh.last_pos/1024);
    while(nblocchi>0){
        fh.last_pos-=1024;
        fh.ind_blocco=fat[fh.ind_blocco].next;
        nblocchi--;
    }
    for(int i=fh.last_pos;i<1024;i++){
        if(dataBlocks[fh.ind_blocco].data[i]==0)break;
        printf("%c",dataBlocks[fh.ind_blocco].data[i]);
    }
    fh.ind_blocco=fat[fh.ind_blocco].next;
    while(fh.ind_blocco!=-1){
        for(int i=0;i<1024;i++){
            if(dataBlocks[fh.ind_blocco].data[i]==0)break;
            printf("%c",dataBlocks[fh.ind_blocco].data[i]);
        }
        fh.ind_blocco=fat[fh.ind_blocco].next;
    }
    new_file->ind_puntatore=fh.size_old;
    printf("Operazione conclusa con successo\n");
}

void seekFile(char* filename,struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks){
    if(parentDirectory==NULL)handle_error("parentDirectory inesistente\n");
    if(containsForbiddenChars(filename)==1){//scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    struct FileData* new_file=searchFile(filename,parentDirectory);
    if(new_file==NULL){
        printf("Non esiste il file %s nella directory %s\n",filename,parentDirectory->directoryname);
        return;
    }
    //il file esiste quindi scrivo
    FileHandle fh=openFile(new_file,1,fat,dataBlocks);//mode 2 per seek
    if(fh.ind_blocco==-1){
        printf("Errore: Non riesce ad aprire il file\n");
        return;
    }
    printf("Seek del file %s, posizione ora %d\n",filename,fh.last_pos);
    int d=-1;
    char buffer[10];
    while(d<0 || d>fh.size_old){
        printf("Inserisci un numero compreso tra 0-%d: ",fh.size_old);
        if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
            fprintf(stderr, "Error while reading from stdin, exiting...\n");
            exit(EXIT_FAILURE);
        }
        buffer[strlen(buffer)-1]=0;
        d=atoi(buffer);
    }
    new_file->ind_puntatore=d;
    printf("Puntatore del del file %s spostato nel punto %d\n",filename,new_file->ind_puntatore);
}

void createDir(char* directoryname,struct DirectoryData* parentDirectory,struct DirectoryData* directory){
    if(parentDirectory==NULL)handle_error("parentDirectory inesistente\n");
    if(directory==NULL)handle_error("directory inesistente\n");
    if(containsForbiddenChars(directoryname)==1){//scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    if(strlen(directoryname)<sizeof(parentDirectory->sub_directories[0].directoryname)){
        //verifica esistenza del nome
        for(int i=0; i<MAX_SUB;i++){
            if(parentDirectory->sub_directories[i].directoryname[0]!='\0' && strcmp(parentDirectory->sub_directories[i].directoryname,directoryname)==0){
                printf("Errore: Cambia il nome della directory, gia' esiste\n");
                return;
            }
        }
    }else{
        printf("Errore: Nome della directory troppo lungo\n");
        return;
    }
    //cerca uno spazio libero nella directory
    printf("debug1");
    int k=-1;
    for(int i=0;i<MAX_SUB;i++){
        //debug printf("numero sub[%d]",i);
        if (parentDirectory->sub_directories[i].directoryname[0]=='\0'){
            k=i;
            break;
        }
    }
    if(k==-1){
        printf("Errore: spazio insufficiente nella directory per una nuova directory\n");
        return;
    }
    //cerca uno spazio libero nella struttura directory
    printf("debug2");
    int j=-1;
    for(int i=0;i<MAXENTRY;i++){
        printf("numero dir %d",i);
        if (directory[i].directoryname[0]=='\0'){
            j=i;
            break;
        }
    }
    if(j==-1){
        printf("Errore: spazio insufficiente nella struct directory per una nuova directory\n");
        return;
    }
    //crea subdirectory
    printf("debug5");
    if(strncpy(directory[j].directoryname,directoryname,sizeof(directory[j].directoryname))==NULL)handle_error("Errore copia stringa per nome\n");
    
    //inizializza
    printf("debug6");
    for(int i=0;i<MAX_FILE;i++){
        directory[j].files[i].size=0;//per controlli futuri
    }
    for(int i=0; i<MAX_SUB;i++){
        directory[j].sub_directories[i].directoryname[0]='\0';//per controlli futuri
    }
    printf("debug7");
    directory[j].par_directory=parentDirectory;
    parentDirectory->sub_directories[k]=directory[j];
    printf("Creazione della subdirectory %s con successo\n",directoryname);
    printf("debug8");
}

void eraseDir(char* directoryname,struct DirectoryData* parentDirectory,struct DirectoryData* directory,struct FATEntry* fat,struct DataBlock* dataBlocks){
    if(parentDirectory==NULL)handle_error("parentDirectory inesistente\n");
    if(directory==NULL)handle_error("directory inesistente\n");
    if(containsForbiddenChars(directoryname)==1){//scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    //cerco dir
    struct DirectoryData* discard_dir=NULL;
    for(int s=0;s<MAX_SUB;s++){//trovo la subdirectory
        if(parentDirectory->sub_directories[s].directoryname[0]!='\0'){
            if(strcmp(directoryname,parentDirectory->sub_directories[s].directoryname)==0){
                discard_dir=dir_corrente(&parentDirectory->sub_directories[s]);
                break;
            }
        }
    }
    if(discard_dir==NULL){
        printf("Non esiste la subdirectory %s\n",directoryname);
        return;//non esiste la dir che voglio eliminare
    }
    eraseDirAux(discard_dir,directory,fat,dataBlocks);
    printf("La subdirectory %s e' stata eliminata\n",directoryname);
}

void eraseDirAux(struct DirectoryData* dir,struct DirectoryData* directory,struct FATEntry* fat,struct DataBlock* dataBlocks){
    if(directory==NULL)handle_error("directory inesistente\n");
    if(dir==NULL)handle_error("dir inesistente\n");

    for(int f=0;f<MAX_FILE;f++){//elimina file
        if(dir->files[f].size!=0){
            eraseFile(dir->files[f].filename, dir,fat,dataBlocks);
        }
    }       
    for(int s=0;s<MAX_SUB;s++){//elimina sub_dir
        if (dir->sub_directories[s].directoryname[0]!='\0'){
            eraseDirAux(&dir->sub_directories[s],directory,fat,dataBlocks);
        }
    }
    for(int i=0;i<MAXENTRY;i++){
        if(directory[i].directoryname[0]!='\0' && strcmp(directory[i].directoryname,dir->directoryname)==0){
            directory[i].par_directory=NULL;
            memset(directory[i].directoryname,0,sizeof(directory[i].directoryname));
            directory[i].directoryname[0]='\0';
        }
    }
    dir->par_directory=NULL;
    memset(dir->directoryname,0,sizeof(dir->directoryname));
    dir->directoryname[0]='\0';
}

struct DirectoryData* changeDir(struct DirectoryData* dir,char *dirname){
    struct DirectoryData* new_dir=NULL;
    if(containsForbiddenChars(dirname)==1){//scritto male 
        printf("Chiusura operazione per errore... \n");
        return dir;
    }
    if(strcmp(dirname,"PREC")==0){//torna indietro
        if(dir->par_directory==NULL){
            printf("Ti trovi nella root, non puoi andare indietro\n");
            return dir;
        }
        new_dir=dir_corrente(dir->par_directory);
        printf("Ora ti trovi nella directory %s\n",new_dir->directoryname);
        return new_dir;
    }else{
        for(int i=0;i<MAX_SUB;i++){
            if(strcmp(dirname,dir->sub_directories[i].directoryname)==0){
                new_dir=dir_corrente(&dir->sub_directories[i]);
                printf("Ora ti trovi nella directory %s\n",new_dir->directoryname);
                return new_dir;
            }
        }
        printf("Non esiste la directory %s, rimarrai nella directory %s\n",dirname,dir->directoryname);
        return dir;
    }
}

void listDir(char* dirname,struct DirectoryData* dir){
    if(containsForbiddenChars(dirname)==1){//scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    if(strcmp(dirname,"CORR")==0){//corrente
        printf("Subdirectory presenti in %s:\n",dir->directoryname);
        int count=1;
        for(int i=0;i<MAX_SUB;i++){
            if(dir->sub_directories[i].directoryname[0]!='\0'){
                printf("[%d]\t%s\n",count,dir->sub_directories[i].directoryname);
                count++;
            }
        }
        if(count==1)printf("\tNon esistono\n");
        count=1;
        printf("File presenti in %s:\n",dir->directoryname);
        for(int i=0;i<MAX_FILE;i++){
            if(dir->files[i].size!=0){
                printf("[%d]\t%s\n",count,dir->files[i].filename);
                count++;
            }
        }
        if(count==1)printf("\tNon esistono\n");
    }else{
    //cerca la dir nella directory corrente
        struct DirectoryData* copy=NULL;
        for(int i=0;i<MAX_SUB;i++){
            if(dir->sub_directories[i].directoryname[0]!='\0'){
                if(strcmp(dirname,dir->sub_directories[i].directoryname)==0){//se trovata stampa
                    copy=copy_dir(&dir->sub_directories[i]);
                    printf("Subdirectory presenti in %s:\n",copy->directoryname);
                    int count=1;
                    for(int i=0;i<MAX_SUB;i++){
                        if(copy->sub_directories[i].directoryname[0]!='\0'){
                            printf("[%d]\t%s\n",count,copy->sub_directories[i].directoryname);
                            count++;
                        }
                    }
                    if(count==1)printf("\tNon esistono\n");
                    count=1;
                    printf("File presenti in %s:\n",copy->directoryname);
                    for(int i=0;i<MAX_FILE;i++){
                        if(copy->files[i].size!=0){
                            printf("[%d]\t%s\n",count,copy->files[i].filename);
                            count++;
                        }
                    }
                    if(count==1)printf("\tNon esistono\n");
                    free(copy);
                    return;
                }
            }
        }
        printf("Non esiste la directory %s nella directory corrente\n",dirname);
    }
}

//funzioni ausiliarie
struct DirectoryData* copy_dir(struct DirectoryData* dir){//senza modifiche all'originale
    if(dir==NULL)return NULL;
    //Crea una copia
    struct DirectoryData* dir_corr= (struct DirectoryData*)malloc(sizeof(struct DirectoryData));
    if(dir_corr==NULL)handle_error("Errore di allocazione di memoria.\n");
    // Copia i dati dal nodo originale al nuovo nodo
    strncpy(dir_corr->directoryname,dir->directoryname,sizeof(dir_corr->directoryname));
    dir_corr->files= dir->files;
    dir_corr->sub_directories= dir->sub_directories;
    dir_corr->par_directory=dir->par_directory;
    return dir_corr;
}

struct DirectoryData* dir_corrente(struct DirectoryData* dir){//modifiche all'originale
    if(dir==NULL)return NULL;
    return dir;
}