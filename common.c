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



int containsForbiddenChars(const char *input){
    //Controllo caratteri speciali
    const char *forbiddenChars = ",?|/:*\"\\<>";
    for (int i = 0; i < strlen(forbiddenChars); i++) {
        if (strchr(input, forbiddenChars[i]) != NULL) {
            printf("Non puoi inserire uno di questi caratteri nel nome: %s. ",forbiddenChars);
            return 1; //Trovato un carattere vietato
        }
    }
    return 0; //Nessun carattere trovato
}

void createFile(char* filename, int size, struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks){
    if(parentDirectory==NULL)handle_error("Errore: ParentDirectory inesistente\n");
    if(fat==NULL)handle_error("Errore: Fat inesistente\n");
    if(dataBlocks==NULL)handle_error("Errore: DataBlocks inesistente\n");
    if(containsForbiddenChars(filename)==1){//Scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    //Verifica esistenza del nome
    if(strlen(filename)<sizeof(parentDirectory->files[0].filename)){
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
    //Cerca uno spazio libero nella directory
    int k=-1;
    for (int i=0; i<MAX_FILE;i++){
        if(parentDirectory->files[i].size==0){//implica vuoto
            k=i;
            break;
        }
    }
    if(k==-1){
        printf("Errore: Spazio insufficiente nella directory per un nuovo file\n");
        return;
    }
    //Cerca blocchi liberi nella FAT
    int necessari=(int)ceil((double)(size+sizeof(struct DataBlock)-1)/sizeof(struct DataBlock));//2  
    int bstart=-1,bsecond=-1;
    int allocati=0;
    for(int i=0;i<FATSIZE;i++){//servono 2 blocchi, poi aumenta
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
    //Controllo pre-assegnazione
    if(bstart==-1 || bsecond==-1){
        printf("Errore: Spazio insufficiente nella fat per un nuovo file\n");
        return;
    }
    //Conferma posizione nella fat
    fat[bstart].used=1;
    fat[bstart].next=bsecond;
    fat[bsecond].used=1;
    //Assegna valori al file
    struct FileData new_file;
    if(strncpy(new_file.filename,filename,sizeof(new_file.filename))==NULL)handle_error("Errore: Copia nome file durante creazione\n");
    new_file.size=size;
    new_file.start_block=bstart;
    new_file.ind_puntatore=0;
    new_file.par_directory=parentDirectory;
    parentDirectory->files[k]=new_file;//aggiorno la directory
    printf("Creazione del file %s con successo\n",filename);
}

struct FileData* searchFile(char* filename,struct DirectoryData* dir){
    if(dir==NULL)handle_error("Errore: dir inesistente\n");
    struct FileData* new_file=NULL;
    for(int i=0;i<MAX_FILE;i++){//cerca nella directory
        if(dir->files[i].size!=0 && strcmp(dir->files[i].filename,filename)==0){
            new_file=&(dir->files[i]);
            return new_file;
        }
    }
    return new_file;
}

void searchFileAll(char* filename,struct DirectoryData* dir){
    if(dir==NULL)handle_error("Errore: Dir inesistente\n");
    printf("Inizio la ricerca del file con nome %s nel sistema\n",filename);
    int ret=searchFileAllAux(filename,dir,"");
    if(ret==0)printf("\tNon ho trovato nessun file\n");
    printf("Ricerca finita\n");
}

int searchFileAllAux(char* filename,struct DirectoryData* dir,char* per){
    if(dir==NULL)handle_error("Errore: Dir inesistente\n");
    struct FileData* new_file=NULL;
    int ret=0;
    //Costruisce il percorso del file
    char percorso[1024];
    if(strcpy(percorso,per)==NULL)handle_error("Errore: Copia percorso durante ricerca\n");
    if(strcat(percorso,"/")==NULL)handle_error("Errore: Concatenazione percorso durante ricerca\n");
    if(strncat(percorso,dir->directoryname,strlen(dir->directoryname))==NULL)handle_error("Errore: Concatenazione percorso durante ricerca\n");
    new_file=searchFile(filename,dir);
    if(new_file!=NULL){
        printf("\tEsiste il file %s nella directory %s\n",filename,percorso);
        ret++;
    }else{
        for(int i=0; i<MAX_SUB;i++){//cerca nelle subdirectory di root
            if(dir->sub_directories[i].directoryname[0]!='\0'){
                ret+=searchFileAllAux(filename,&dir->sub_directories[i],percorso);
            }
        }
    }
    return ret;
}

FileHandle openFile(struct FileData* new_file,int mode,struct FATEntry* fat,struct DataBlock* dataBlocks){
    if(new_file==NULL)handle_error("Errore: new_file inesistente\n");
    if(fat==NULL)handle_error("Errore: Fat inesistente\n");
    if(dataBlocks==NULL)handle_error("Errore: DataBlocks inesistente\n");
    FileHandle fh;
    fh.ind_blocco=new_file->start_block;//primo blocco di dati
    fh.size_old=0;
    int count=new_file->start_block;
    while(count!=-1){
        int len=0;
        //Calcola lunghezza file
        while (len<sizeof(dataBlocks[count].data) && dataBlocks[count].data[len] != '\0') {
            len++;
        }
        fh.size_old+=len;
        count=fat[count].next;
    }
    fh.last_pos=new_file->ind_puntatore;
    if(mode==0){//Lettura
        fh.current_pos=0;
    }else{//Scrittura   
        fh.current_pos=fh.size_old;//punto dopo l'utltimo carattere
    }
    return fh;
}

void eraseFile(char* filename, struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks){
    if(parentDirectory==NULL)handle_error("Errore: ParentDirectory inesistente\n");
    if(fat==NULL)handle_error("Errore: Fat inesistente\n");
    if(dataBlocks==NULL)handle_error("Errore: DataBlocks inesistente\n");
    if(containsForbiddenChars(filename)==1){//Scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    struct FileData* new_file=searchFile(filename,parentDirectory);
    if(new_file==NULL){
        printf("\tNon esiste il file %s nella directory %s\n",filename,parentDirectory->directoryname);
        return;
    }
    if(new_file->size==0){
        printf("Errore: Spazio per file gia' vuoto\n");
        return;
    }
    int ind=new_file->start_block;
    while(ind!=-1){//Libero la memoria
        memset(&dataBlocks[ind],0,sizeof(struct DataBlock));
        fat[ind].used=0;
        int temp=fat[ind].next;
        fat[ind].next=-1;    
        ind=temp;
    }
    memset(new_file,0,sizeof(struct FileData));
    new_file->size=0;//per controlli
    printf("\tFile %s eliminato\n",filename);
}

int addBloccoVuoto(int ind_now,struct FATEntry* fat,struct DataBlock* dataBlocks){
    if(fat==NULL)handle_error("Errore: Fat inesistente\n");
    if(dataBlocks==NULL)handle_error("Errore: DataBlocks inesistente\n");
    //Cerca nella fat
    int k=-1;
    for(int i=0;i<FATSIZE;i++){
        if(fat[i].used==0){
            k=i;
            break;
        }
    }
    //Controllo pre-assegnazione
    if(k==-1){
        printf("Errore: Spazio insufficiente nella fat per aumentare lo spazio del file... Operazione annullata\n");
        return -1;
    }
    fat[ind_now].next=k;
    fat[k].used=1;
    return 0;
}

void writeFile(char* filename,struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks){
    if(parentDirectory==NULL)handle_error("Errore: ParentDirectory inesistente\n");
    if(fat==NULL)handle_error("Errore: Fat inesistente\n");
    if(dataBlocks==NULL)handle_error("Errore: DataBlocks inesistente\n");
    if(containsForbiddenChars(filename)==1){//scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    struct FileData* new_file=searchFile(filename,parentDirectory);
    if(new_file==NULL){
        printf("\tNon esiste il file %s nella directory %s\n",filename,parentDirectory->directoryname);
        return;
    }
    //File esiste quindi scrivo
    FileHandle fh=openFile(new_file,1,fat,dataBlocks);
    if(fh.ind_blocco==-1){
        printf("Errore: Non riesce ad aprire il file\n");
        return;
    }
    printf("Scrittura del file %s\n",filename);
    //Scrive
    char buffer[1024];
    printf("Input nel file, max 1023 caratteri(write): ");
    if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
        fprintf(stderr, "Error while reading from stdin, exiting...\n");
        exit(EXIT_FAILURE);
    }
    if(fh.size_old==0){//Prima scrittura
        if(strncpy(dataBlocks[fh.ind_blocco].data, buffer,strlen(buffer))==NULL)handle_error("Errore: Scrittura nel file, prima volta\n");
        printf("File inizializzato correttamente... ");
    }else{
        int blocchi_pieni=(int)floor((double)fh.size_old/1024);
        int ret=0;
        while(blocchi_pieni>0){//Ci porta la primo blocco dove posso scrivere
            fh.current_pos-=1024;
            fh.ind_blocco=fat[fh.ind_blocco].next;
            //debug printf("indice blocco(spostamento):%d\n",fh.ind_blocco);
            ret=1;
            blocchi_pieni--;
        }
        if(ret==0){//NON ha blocchi pieni==>ha un blocco vuoto(perche minimo 2) 
            if(strncpy(dataBlocks[fh.ind_blocco].data+fh.current_pos,buffer,sizeof(dataBlocks[fh.ind_blocco].data)-fh.current_pos)==NULL)handle_error("Errore: Scrittura nel file,ret==0\n");
            if(strlen(buffer)+fh.current_pos>1024){
                fh.ind_blocco=fat[fh.ind_blocco].next;//cambio senza aggiungere
                if(strncpy(dataBlocks[fh.ind_blocco].data,buffer+(sizeof(dataBlocks[fh.ind_blocco].data)-fh.current_pos),strlen(buffer)-(sizeof(dataBlocks[fh.ind_blocco].data)-fh.current_pos))==NULL)handle_error("Errore: Scrittura nel file,ret==0,nuovo blocco\n");
            }
        }else if(ret==1){//Ha almeno un blocco pieno==>no blocco vuoto
            if(strncpy(dataBlocks[fh.ind_blocco].data+fh.current_pos,buffer,sizeof(dataBlocks[fh.ind_blocco].data)-fh.current_pos)==NULL)handle_error("Errore: Scrittura nel file,ret==1\n");
            if(strlen(buffer)+fh.current_pos>1024){
                if(addBloccoVuoto(fh.ind_blocco,fat,dataBlocks)==-1)return;//esce e non continua a scrivere
                fh.ind_blocco=fat[fh.ind_blocco].next;
                if(strncpy(dataBlocks[fh.ind_blocco].data,buffer+(sizeof(dataBlocks[fh.ind_blocco].data)-fh.current_pos),strlen(buffer)-(sizeof(dataBlocks[fh.ind_blocco].data)-fh.current_pos))==NULL)handle_error("Errore: Scrittura nel file,ret==1,nuovo blocco\n");
            }else if(strlen(buffer)+fh.current_pos==1024 && fat[fh.ind_blocco].next==-1){
                //Se riempe il blocco di dati, aumento lo spazio in anticipo
                if(addBloccoVuoto(fh.ind_blocco,fat,dataBlocks)==-1)return;
            }
        }
    }
    new_file->ind_puntatore+=strlen(buffer);
    if(fh.size_old+strlen(buffer)>512) new_file->size+=strlen(buffer);
    printf("Operazione conclusa con successo\n");
}

void writeFileForm(char* filename,struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks){
    if(parentDirectory==NULL)handle_error("Errore: ParentDirectory inesistente\n");
    if(fat==NULL)handle_error("Errore: Fat inesistente\n");
    if(dataBlocks==NULL)handle_error("Errore: DataBlocks inesistente\n");
    if(containsForbiddenChars(filename)==1){//Scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    struct FileData* new_file=searchFile(filename,parentDirectory);
    if(new_file==NULL){
        printf("\tNon esiste il file %s nella directory %s\n",filename,parentDirectory->directoryname);
        return;
    }
    //File esiste quindi scrivo
    FileHandle fh=openFile(new_file,1,fat,dataBlocks);
    if(fh.ind_blocco==-1){
        printf("Errore: Non riesce ad aprire il file\n");
        return;
    }
    printf("Scrittura del file %s\n",filename);
    //Scrive
    char buffer[1024];
    printf("Input nel file, max 1023 caratteri(write): ");
    if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
        fprintf(stderr, "Error while reading from stdin, exiting...\n");
        exit(EXIT_FAILURE);
    }
    int nblocchi=(int)floor((double)fh.last_pos/1024);
    while(nblocchi>0){//Ci porta al blocco in base alla posizione del puntatore
        fh.last_pos-=1024;
        fh.ind_blocco=fat[fh.ind_blocco].next;
        nblocchi--;
    }
    char temp[1024];
    if(strncpy(temp,dataBlocks[fh.ind_blocco].data,sizeof(dataBlocks[0].data))==NULL)handle_error("Errore: Copia il blocco da sovrascrivere\n");
    //DEBUG printf("%s\n",temp);
    if(strncpy(dataBlocks[fh.ind_blocco].data+fh.last_pos,buffer,sizeof(dataBlocks[fh.ind_blocco].data)-fh.last_pos)==NULL)handle_error("Errore: Aggiungo la nuova parte al blocco\n");
    //Dopo la sovrascrittura copia la parte dopo del blocco
    if(strlen(buffer)+fh.last_pos>1024){
        if(fat[fh.ind_blocco].next==-1){//nuvo blocco
            if(addBloccoVuoto(fh.ind_blocco,fat,dataBlocks)==-1)return;
        }
        fh.ind_blocco=fat[fh.ind_blocco].next;
        if(strncpy(temp,dataBlocks[fh.ind_blocco].data,sizeof(dataBlocks[0].data))==NULL)handle_error("Errore: Copia il blocco da sovrascrivere2\n");
        if(strncpy(dataBlocks[fh.ind_blocco].data,buffer+(sizeof(dataBlocks[fh.ind_blocco].data)-fh.last_pos),strlen(buffer)-(sizeof(dataBlocks[fh.ind_blocco].data)-fh.last_pos))==NULL)handle_error("Errore: Aggiungo la nuova parte al blocco2\n");
        if(strncpy(dataBlocks[fh.ind_blocco].data+(strlen(buffer)-(sizeof(dataBlocks[fh.ind_blocco].data)-fh.last_pos)),temp+(strlen(buffer)-(sizeof(dataBlocks[fh.ind_blocco].data)-fh.last_pos)),sizeof(dataBlocks[fh.ind_blocco].data)-(strlen(buffer)-(sizeof(dataBlocks[fh.ind_blocco].data)-fh.last_pos)))==NULL)handle_error("Errore: Ricopia la parte dopo2\n");
    }else{
        if(strncpy(dataBlocks[fh.ind_blocco].data+(fh.last_pos+strlen(buffer)),temp+(fh.last_pos+strlen(buffer)),sizeof(dataBlocks[fh.ind_blocco].data)-(fh.last_pos+strlen(buffer)))==NULL)handle_error("Errore: Ricopia la parte dopo\n");
    }
    new_file->ind_puntatore+=strlen(buffer);
    if(fh.size_old+strlen(buffer)>512) new_file->size+=strlen(buffer);
    printf("Operazione conclusa con successo\n");
}

void readFile(char* filename,struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks){
    if(parentDirectory==NULL)handle_error("Errore: ParentDirectory inesistente\n");
    if(fat==NULL)handle_error("Errore: Fat inesistente\n");
    if(dataBlocks==NULL)handle_error("Errore: DataBlocks inesistente\n");
    if(containsForbiddenChars(filename)==1){//Scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    struct FileData* new_file=searchFile(filename,parentDirectory);
    if(new_file==NULL){
        printf("\tNon esiste il file %s nella directory %s\n",filename,parentDirectory->directoryname);
        return;
    }
    //File esiste quindi leggo
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
    new_file->ind_puntatore=fh.size_old;
    printf("Operazione conclusa con successo\n");
}

void readFileFrom(char* filename,struct DirectoryData* parentDirectory,struct FATEntry* fat,struct DataBlock* dataBlocks){
    if(parentDirectory==NULL)handle_error("Errore: ParentDirectory inesistente\n");
    if(fat==NULL)handle_error("Errore: Fat inesistente\n");
    if(dataBlocks==NULL)handle_error("Errore: DataBlocks inesistente\n");
    if(containsForbiddenChars(filename)==1){//Scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    struct FileData* new_file=searchFile(filename,parentDirectory);
    if(new_file==NULL){
        printf("\tNon esiste il file %s nella directory %s\n",filename,parentDirectory->directoryname);
        return;
    }
    //File esiste quindi leggo
    FileHandle fh=openFile(new_file,0,fat,dataBlocks);
    if(fh.ind_blocco==-1){
        printf("Errore: Non riesce ad aprire il file\n");
        return;
    }
    printf("Lettura del file %s\n",filename);
    int nblocchi=(int)floor((double)fh.last_pos/1024);
    while(nblocchi>0){//Ti porta al blocco dove incominciare a leggere
        fh.last_pos-=1024;
        fh.ind_blocco=fat[fh.ind_blocco].next;
        nblocchi--;
    }
    for(int i=fh.last_pos;i<1024;i++){//Legge il primo blocco dal puntatore
        if(dataBlocks[fh.ind_blocco].data[i]==0)break;
        printf("%c",dataBlocks[fh.ind_blocco].data[i]);
    }
    //I prossimi blocchi li legge tutti
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
    if(parentDirectory==NULL)handle_error("Errore: ParentDirectory inesistente\n");
    if(fat==NULL)handle_error("Errore: Fat inesistente\n");
    if(dataBlocks==NULL)handle_error("Errore: DataBlocks inesistente\n");
    if(containsForbiddenChars(filename)==1){//Scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    struct FileData* new_file=searchFile(filename,parentDirectory);
    if(new_file==NULL){
        printf("\tNon esiste il file %s nella directory %s\n",filename,parentDirectory->directoryname);
        return;
    }
    //File esiste quindi scrivo
    FileHandle fh=openFile(new_file,1,fat,dataBlocks);
    if(fh.ind_blocco==-1){
        printf("Errore: Non riesce ad aprire il file\n");
        return;
    }
    printf("Seek del file %s, posizione del puntatore ora %d\n",filename,fh.last_pos);
    int d=-1;
    char buffer[10];
    while(d<0 || d>fh.size_old){//Cicla fino a un inserimento corretto
        printf("\tInserisci un numero compreso tra 0-%d: ",fh.size_old);
        if(fgets(buffer, sizeof(buffer), stdin) != (char*)buffer){
            fprintf(stderr, "Error while reading from stdin, exiting...\n");
            exit(EXIT_FAILURE);
        }
        buffer[strlen(buffer)-1]=0;
        d=atoi(buffer);
    }
    new_file->ind_puntatore=d;
    printf("Puntatore del file %s spostato nel punto %d\n",filename,new_file->ind_puntatore);
}

void createDir(char* directoryname,struct DirectoryData* parentDirectory,struct DirectoryData* directory){
    if(parentDirectory==NULL)handle_error("Errore: ParentDirectory inesistente\n");
    if(directory==NULL)handle_error("Errore: Directory inesistente\n");
    if(containsForbiddenChars(directoryname)==1){//Scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    //Verifica esistenza del nome
    if(strlen(directoryname)<sizeof(parentDirectory->sub_directories[0].directoryname)){
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
    //Cerca uno spazio libero nella directory
    int k=-1;
    for(int i=0;i<MAX_SUB;i++){
        //DEBUG printf("numero sub[%d]",i);
        if (parentDirectory->sub_directories[i].directoryname[0]=='\0'){
            k=i;
            break;
        }
    }
    if(k==-1){
        printf("Errore: Spazio insufficiente nella directory per una nuova directory\n");
        return;
    }
    //Cerca uno spazio libero nella struttura directory
    int j=-1;
    for(int i=0;i<MAXENTRY;i++){
        //DEBUG printf("numero dir %d",i);
        if (directory[i].directoryname[0]=='\0'){
            j=i;
            break;
        }
    }
    if(j==-1){
        printf("Errore: Spazio insufficiente nella struct directory per una nuova directory\n");
        return;
    }
    //Crea subdirectory
    if(strncpy(directory[j].directoryname,directoryname,sizeof(directory[j].directoryname))==NULL)handle_error("Errore: Copia stringa per nome\n");
    
    //Inizializza
    for(int i=0;i<MAX_FILE;i++){
        directory[j].files[i].size=0;//per controlli futuri
    }
    for(int i=0; i<MAX_SUB;i++){
        directory[j].sub_directories[i].directoryname[0]='\0';//per controlli futuri
    }
    directory[j].par_directory=parentDirectory;
    parentDirectory->sub_directories[k]=directory[j];
    printf("Creazione della subdirectory %s con successo\n",directoryname);
}

void eraseDir(char* directoryname,struct DirectoryData* parentDirectory,struct DirectoryData* directory,struct FATEntry* fat,struct DataBlock* dataBlocks){
    if(parentDirectory==NULL)handle_error("Errore: ParentDirectory inesistente\n");
    if(fat==NULL)handle_error("Errore: Fat inesistente\n");
    if(dataBlocks==NULL)handle_error("Errore: DataBlocks inesistente\n");
    if(directory==NULL)handle_error("Errore: Directory inesistente\n");
    if(containsForbiddenChars(directoryname)==1){//Scritto male 
        printf("Errore: chiusura operazione per errore... \n");
        return;
    }
    //Cerco dir
    struct DirectoryData* discard_dir=NULL;
    for(int s=0;s<MAX_SUB;s++){//trovo la subdirectory
        if(parentDirectory->sub_directories[s].directoryname[0]!='\0'){
            if(strcmp(directoryname,parentDirectory->sub_directories[s].directoryname)==0){
                discard_dir=dir_corrente(&parentDirectory->sub_directories[s]);
                if(discard_dir==NULL)handle_error("Errore: Copia della directory da eliminare\n");
                break;
            }
        }
    }
    eraseDirAux(discard_dir,directory,fat,dataBlocks);
    printf("La subdirectory %s e' stata eliminata\n",directoryname);
}

void eraseDirAux(struct DirectoryData* dir,struct DirectoryData* directory,struct FATEntry* fat,struct DataBlock* dataBlocks){
    if(dir==NULL)handle_error("Errore: Dir inesistente\n");
    if(fat==NULL)handle_error("Errore: Fat inesistente\n");
    if(dataBlocks==NULL)handle_error("Errore: DataBlocks inesistente\n");
    if(directory==NULL)handle_error("Errore: Directory inesistente\n");

    for(int f=0;f<MAX_FILE;f++){//Elimina file
        if(dir->files[f].size!=0){
            eraseFile(dir->files[f].filename, dir,fat,dataBlocks);
        }
    }       
    for(int s=0;s<MAX_SUB;s++){//Elimina sub_dir
        if (dir->sub_directories[s].directoryname[0]!='\0'){
            eraseDirAux(&dir->sub_directories[s],directory,fat,dataBlocks);
        }
    }
    for(int i=0;i<MAXENTRY;i++){//Pulisce la struttura della directory
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
    if(dir==NULL)handle_error("Errore: Dir inesistente\n");
    struct DirectoryData* new_dir=NULL;
    if(containsForbiddenChars(dirname)==1){//Scritto male 
        printf("Chiusura operazione per errore... \n");
        return dir;
    }
    if(strcmp(dirname,"PREC")==0){//Torna indietro
        if(dir->par_directory==NULL){
            printf("Ti trovi nella root, non puoi andare indietro\n");
            return dir;
        }
        new_dir=dir_corrente(dir->par_directory);
        if(new_dir==NULL)handle_error("Errore nel cambio della nuova directory\n");
        printf("Ora ti trovi nella directory %s\n",new_dir->directoryname);
        return new_dir;
    }else{
        for(int i=0;i<MAX_SUB;i++){//Cerca nella subdirectory
            if(strcmp(dirname,dir->sub_directories[i].directoryname)==0){
                new_dir=dir_corrente(&dir->sub_directories[i]);
                if(new_dir==NULL)handle_error("Errore nel cambio della nuova directory\n");
                printf("Ora ti trovi nella directory %s\n",new_dir->directoryname);
                return new_dir;
            }
        }
        printf("Non esiste la directory %s, rimarrai nella directory %s\n",dirname,dir->directoryname);
        return dir;
    }
}

void listDir(char* dirname,struct DirectoryData* dir){
    if(dir==NULL)handle_error("Errore: Dir inesistente\n");
    if(containsForbiddenChars(dirname)==1){//Scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    if(strcmp(dirname,"CORR")==0){//Corrente
        printf("Oggetti nella directory corrente %s:\n",dir->directoryname);
        printf("Subdirectory presenti:\n");
        int count=1;
        for(int i=0;i<MAX_SUB;i++){
            if(dir->sub_directories[i].directoryname[0]!='\0'){
                printf("[%d]\t%s\n",count,dir->sub_directories[i].directoryname);
                count++;
            }
        }
        if(count==1)printf("\tNon esistono\n");
        count=1;
        printf("File presenti in:\n");
        for(int i=0;i<MAX_FILE;i++){
            if(dir->files[i].size!=0){
                printf("[%d]\t%s\n",count,dir->files[i].filename);
                count++;
            }
        }
        if(count==1)printf("\tNon esistono\n");
        return;
    }else{
    //Cerca la dir nella directory corrente
        struct DirectoryData* copy=NULL;
        for(int i=0;i<MAX_SUB;i++){
            if(dir->sub_directories[i].directoryname[0]!='\0'){
                if(strcmp(dirname,dir->sub_directories[i].directoryname)==0){//Trovata e stampa
                    copy=copy_dir(&dir->sub_directories[i]);
                    printf("Oggetti nella subdirectory %s:\n",copy->directoryname);
                    printf("Subdirectory presenti in:\n");
                    int count=1;
                    for(int i=0;i<MAX_SUB;i++){
                        if(copy->sub_directories[i].directoryname[0]!='\0'){
                            printf("[%d]\t%s\n",count,copy->sub_directories[i].directoryname);
                            count++;
                        }
                    }
                    if(count==1)printf("\tNon esistono\n");
                    count=1;
                    printf("File presenti in:\n");
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
    }
    printf("Non esiste la subdirectory %s, verifica di aver inserito il nome corretto\n",dirname);
}

struct DirectoryData* copy_dir(struct DirectoryData* dir){
    //Senza modifiche all'originale
    if(dir==NULL)return NULL;
    //Crea una copia
    struct DirectoryData* dir_corr= (struct DirectoryData*)malloc(sizeof(struct DirectoryData));
    if(dir_corr==NULL)handle_error("Errore: Allocazione copia della struttura\n");
    // Copia i dati dal nodo originale al nuovo nodo
    if(strncpy(dir_corr->directoryname,dir->directoryname,sizeof(dir_corr->directoryname))==NULL)handle_error("Errore strncpy copy_dir\n");
    dir_corr->files= dir->files;
    dir_corr->sub_directories= dir->sub_directories;
    dir_corr->par_directory=dir->par_directory;
    return dir_corr;
}

struct DirectoryData* dir_corrente(struct DirectoryData* dir){
    //Modifiche all'originale
    if(dir==NULL)return NULL;
    return dir;
}