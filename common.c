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

void createFile(char* filename, int size, struct DirectoryData parentDirectory, void *mmapped_buffer){
    if(&parentDirectory==NULL)handle_error("Errore: ParentDirectory inesistente\n");
    if(mmapped_buffer==NULL)handle_error("Errore: mmapped_buffer inesistente\n");
    if(containsForbiddenChars(filename)==1){//Scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    //Verifica esistenza del nome
    if(strlen(filename)<sizeof(parentDirectory.files[0].filename)){
        for(int i=0; i<MAX_FILE;i++){
            if(parentDirectory.files[i].size!=0 && strcmp(parentDirectory.files[i].filename,filename)==0){
                printf("Errore: Cambia il nome del file, gia' esiste\n");
                return;
            }
        }
    }else{
        printf("Errore: Nome del file troppo lungo\n");
        return;
    }
    //Cerca uno spazio libero nella directory
    int k=-1;
    for (int i=0; i<MAX_FILE;i++){
        if(parentDirectory.files[i].size==0){//implica vuoto
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
    struct FATEntry fat_temp[FATSIZE];
    memcpy(&fat_temp, mmapped_buffer+LENDIR, LENFAT);
    for (int i=0;i<FATSIZE;i++){
        if(fat_temp[i].used==0){
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
    fat_temp[bstart].used=1;
    fat_temp[bstart].next=bsecond;
    fat_temp[bsecond].used=1;
    fat_temp[bsecond].next=-1;
    memcpy(mmapped_buffer+LENDIR, &fat_temp, LENFAT);
    //Assegna valori al file 
    if(strncpy(parentDirectory.files[k].filename,filename,sizeof(parentDirectory.files[k].filename))==NULL)handle_error("Errore: Copia nome file durante creazione\n");
    parentDirectory.files[k].size=size;
    parentDirectory.files[k].start_block=bstart;
    //DEBUG printf("IND: %d\n",new_file.start_block);
    parentDirectory.files[k].ind_puntatore=0;
    parentDirectory.files[k].par_directory=&parentDirectory;
    memcpy(mmapped_buffer+(parentDirectory.dir_indice*sizeof(struct DirectoryData)), &parentDirectory, sizeof(struct DirectoryData));
    //aggiorno la directory
    printf("Creazione del file %s con successo\n",filename);
}

int searchFile(char* filename,struct DirectoryData parentDirectory){
    //Se esce un numero tra 0 a 9 l'ha trovato
    if(&parentDirectory==NULL)handle_error("Errore: parentDirectory inesistente\n");
    for(int i=0;i<MAX_FILE;i++){//cerca nella directory
        if(parentDirectory.files[i].size!=0 && strcmp(parentDirectory.files[i].filename,filename)==0){
            return i;
        }
    }
    return -1;
}

void searchFileAll(char* filename, void *mmapped_buffer){
    if(mmapped_buffer==NULL)handle_error("Errore: mmapped_buffer inesistente\n");
    printf("Inizio la ricerca del file con nome %s nel sistema\n",filename);
    struct DirectoryData dir=puntatore_dir(0,mmapped_buffer);
    int ret=searchFileAllAux(filename,dir,"",mmapped_buffer);
    if(ret==0)printf("\tNon ho trovato nessun file\n");
    printf("Ricerca finita\n");
}

int searchFileAllAux(char* filename,struct DirectoryData dir,char* per,void *mmapped_buffer){
    if(&dir==NULL)handle_error("Errore: Dir inesistente\n");
    int ret=0;
    //Costruisce il percorso del file
    char percorso[1024];
    if(strcpy(percorso,per)==NULL)handle_error("Errore: Copia percorso durante ricerca\n");
    if(strcat(percorso,"/")==NULL)handle_error("Errore: Concatenazione percorso durante ricerca\n");
    if(strncat(percorso,dir.directoryname,strlen(dir.directoryname))==NULL)handle_error("Errore: Concatenazione percorso durante ricerca\n");
    int ind_file=searchFile(filename,dir);
    if(ind_file==-1){
        for(int i=0; i<MAX_SUB;i++){//cerca nelle subdirectory di root
            if(dir.sub_directories[i]->directoryname[0]!='\0'){
                struct DirectoryData temp_dir=puntatore_dir(dir.sub_directories[i]->dir_indice,mmapped_buffer);
                ret+=searchFileAllAux(filename, temp_dir ,percorso, mmapped_buffer);
            }
        }
    }else{
        printf("\tEsiste il file %s nella directory %s\n",filename,percorso);
        ret++;
    }
    return ret;
}

FileHandle openFile(struct FileData new_file,int mode,void *mmapped_buffer){
    if(&new_file==NULL)handle_error("Errore: new_file inesistente\n");
    if(mmapped_buffer==NULL)handle_error("Errore: mmapped_buffer inesistente\n");
    FileHandle fh;
    fh.ind_blocco=new_file.start_block;//primo blocco di dati
    fh.size_old=0;
    int count=new_file.start_block;
    while(count!=-1){
        int len=0;
        //Calcola lunghezza file
        struct DataBlock temp_db;
        memcpy(&temp_db, mmapped_buffer+(LENDIR+LENFAT+(count*sizeof(struct DataBlock))) ,sizeof(struct DataBlock));
        while(len<1024 && temp_db.data[len]!='\0'){
            len++;
        }
        fh.size_old+=len;
        memcpy(&count, mmapped_buffer+(LENDIR+(count*sizeof(struct FATEntry))+sizeof(int)) ,sizeof(int));
    }
    fh.last_pos=new_file.ind_puntatore;
    if(mode==0){//Lettura
        fh.current_pos=0;
    }else{//Scrittura   
        fh.current_pos=fh.size_old;//punto dopo l'utltimo carattere
    }
    return fh;
}

void eraseFile(char* filename, struct DirectoryData parentDirectory, void *mmapped_buffer){
    if(&parentDirectory==NULL)handle_error("Errore: ParentDirectory inesistente\n");
    if(mmapped_buffer==NULL)handle_error("Errore: mmapped_buffer inesistente\n");
    if(containsForbiddenChars(filename)==1){//Scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    int ret=searchFile(filename,parentDirectory);
    if(ret==-1){
        printf("\tNon esiste il file %s nella directory %s\n",filename,parentDirectory.directoryname);
        return;
    }
    int ind=-1;
    ind=parentDirectory.files[ret].start_block;
    printf("ind: %d\n",ind);
    struct FATEntry fat_temp[FATSIZE];
    memcpy(&fat_temp, mmapped_buffer+LENDIR, LENFAT);
    while(ind!=-1){
        memset(mmapped_buffer+LENDIR+LENFAT+(ind*sizeof(struct DataBlock)), 0, sizeof(struct DataBlock));
        fat_temp[ind].used=0;
        int temp=fat_temp[ind].next;
        fat_temp[ind].next=-1;    
        ind=temp;
    }
    memcpy(mmapped_buffer+LENDIR, &fat_temp, LENFAT);
    int size=(parentDirectory.dir_indice*sizeof(struct DirectoryData))+((2*sizeof(int))+sizeof(parentDirectory.directoryname)+(ret*sizeof(struct FileData)));
    memset(mmapped_buffer+size, 0, sizeof(struct FileData));
    //new_file->size=0;//per controlli*/
    printf("\tFile %s eliminato\n",filename);
}

int addBloccoVuoto(int ind_now,void *mmapped_buffer){
    if(mmapped_buffer==NULL)handle_error("Errore: mmapped_buffer inesistente\n");
    //Cerca nella fat
    struct FATEntry fat_temp[FATSIZE];
    memcpy(&fat_temp, mmapped_buffer+LENDIR, LENFAT);
    int newblock=-1;
    for (int i=0;i<FATSIZE;i++){
        if(fat_temp[i].used==0){
            newblock=i; 
            break;
        }
    }
    //Controllo pre-assegnazione
    if(newblock==-1){
        printf("Errore: Spazio insufficiente nella fat per aumentare lo spazio del file... Operazione annullata\n");
        return -1;
    }
    fat_temp[ind_now].next=newblock;
    fat_temp[newblock].used=1;
    fat_temp[newblock].next=-1;
    memcpy(mmapped_buffer+LENDIR, &fat_temp, LENFAT);
    return 0;
}

void writeFile(char* filename,struct DirectoryData parentDirectory, void *mmapped_buffer){
    if(&parentDirectory==NULL)handle_error("Errore: ParentDirectory inesistente\n");
    if(mmapped_buffer==NULL)handle_error("Errore: mmapped_buffer inesistente\n");
    if(containsForbiddenChars(filename)==1){//scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    int ret=searchFile(filename,parentDirectory);
    if(ret==-1){
        printf("\tNon esiste il file %s nella directory %s\n",filename,parentDirectory.directoryname);
        return;
    }
    //File esiste quindi scrivo
    FileHandle fh=openFile(parentDirectory.files[ret],1,mmapped_buffer);
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
        memcpy(mmapped_buffer+LENDIR+LENFAT+(fh.ind_blocco*sizeof(struct DataBlock)), buffer, strlen(buffer));
        printf("File inizializzato correttamente... ");
    }else{
        int blocchi_pieni=(int)floor((double)fh.size_old/1024);
        int ret=0;
        while(blocchi_pieni>0){//Ci porta la primo blocco dove posso scrivere
            fh.current_pos-=1024;
            memcpy(&fh.ind_blocco, mmapped_buffer+LENDIR+(fh.ind_blocco*sizeof(struct FATEntry)+sizeof(int)), sizeof(int));
            ret=1;
            blocchi_pieni--;
        }
        if(ret==0){//NON ha blocchi pieni==>ha un blocco vuoto(perche minimo 2) 
            memcpy(mmapped_buffer+LENDIR+LENFAT+(fh.ind_blocco*sizeof(struct DataBlock)+fh.current_pos), buffer, 1024-fh.current_pos);
            if(strlen(buffer)+fh.current_pos>1024){
                memcpy(&fh.ind_blocco, mmapped_buffer+LENDIR+(fh.ind_blocco*sizeof(struct FATEntry)+sizeof(int)), sizeof(int));
                memcpy(mmapped_buffer+LENDIR+LENFAT+(fh.ind_blocco*sizeof(struct DataBlock)), buffer+(1024-fh.current_pos), strlen(buffer)-(1024-fh.current_pos));
            }
        }else if(ret==1){//Ha almeno un blocco pieno==>no blocco vuoto
            memcpy(mmapped_buffer+LENDIR+LENFAT+(fh.ind_blocco*sizeof(struct DataBlock)+fh.current_pos), buffer, 1024-fh.current_pos);
            if(strlen(buffer)+fh.current_pos>1024){
                if(addBloccoVuoto(fh.ind_blocco,mmapped_buffer)==-1)return;//esce e non continua a scrivere
                memcpy(&fh.ind_blocco, mmapped_buffer+LENDIR+(fh.ind_blocco*sizeof(struct FATEntry)+sizeof(int)), sizeof(int));
                memcpy(mmapped_buffer+LENDIR+LENFAT+(fh.ind_blocco*sizeof(struct DataBlock)), buffer+(1024-fh.current_pos), strlen(buffer)-(1024-fh.current_pos));
            }else if(strlen(buffer)+fh.current_pos==1024){
                memcpy(&fh.ind_blocco, mmapped_buffer+LENDIR+(fh.ind_blocco*sizeof(struct FATEntry)+sizeof(int)), sizeof(int));
                //Se riempe il blocco di dati, aumento lo spazio in anticipo
                if(fh.ind_blocco==-1){
                    if(addBloccoVuoto(fh.ind_blocco,mmapped_buffer)==-1)return;
                }
            }
        }
    }
    parentDirectory.files[ret].ind_puntatore+=strlen(buffer);
    if(fh.size_old+strlen(buffer)>512) parentDirectory.files[ret].size+=strlen(buffer);
    memcpy(mmapped_buffer+(parentDirectory.dir_indice*sizeof(struct DirectoryData)), &parentDirectory, sizeof(struct DirectoryData));
    printf("Operazione conclusa con successo\n");
}

void writeFileForm(char* filename,struct DirectoryData parentDirectory, void *mmapped_buffer){
    if(&parentDirectory==NULL)handle_error("Errore: ParentDirectory inesistente\n");
    if(mmapped_buffer==NULL)handle_error("Errore: mmapped_buffer inesistente\n");
    if(containsForbiddenChars(filename)==1){//Scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    int ret=searchFile(filename,parentDirectory);
    if(ret==-1){
        printf("\tNon esiste il file %s nella directory %s\n",filename,parentDirectory.directoryname);
        return;
    }
    //File esiste quindi scrivo
    FileHandle fh=openFile(parentDirectory.files[ret],1,mmapped_buffer);
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
        memcpy(&fh.ind_blocco, mmapped_buffer+LENDIR+(fh.ind_blocco*sizeof(struct FATEntry)+sizeof(int)), sizeof(int));
        nblocchi--;
    }
    char temp[1024];
    memcpy(temp, mmapped_buffer+LENDIR+LENFAT+(fh.ind_blocco*sizeof(struct DataBlock)), 1024);
    memcpy(mmapped_buffer+LENDIR+LENFAT+(fh.ind_blocco*sizeof(struct DataBlock)+fh.last_pos), buffer, 1024-fh.last_pos);
    if(strlen(buffer)+fh.last_pos>1024){
        int temp_ind;
        memcpy(&temp_ind, mmapped_buffer+LENDIR+(fh.ind_blocco*sizeof(struct FATEntry)+sizeof(int)), sizeof(int));
        if(temp_ind==-1){//nuvo blocco
            if(addBloccoVuoto(fh.ind_blocco,mmapped_buffer)==-1)return;
        }
        memcpy(temp, mmapped_buffer+LENDIR+LENFAT+(fh.ind_blocco*sizeof(struct DataBlock)), 1024);
        memcpy(mmapped_buffer+LENDIR+LENFAT+(fh.ind_blocco*sizeof(struct DataBlock)), buffer+(1024-fh.last_pos) ,strlen(buffer)-(1024-fh.last_pos));
        memcpy(mmapped_buffer+LENDIR+LENFAT+(fh.ind_blocco*sizeof(struct DataBlock))+(strlen(buffer)-(1024-fh.last_pos)), temp+(strlen(buffer)-(1024-fh.last_pos)), 1024-(strlen(buffer)-(1024-fh.last_pos)));
    }else{
        memcpy(mmapped_buffer+LENDIR+LENFAT+(fh.ind_blocco*sizeof(struct DataBlock)+fh.last_pos+strlen(buffer)), temp+(fh.last_pos+strlen(buffer)), 1024-(fh.last_pos+strlen(buffer)));
    }
    parentDirectory.files[ret].ind_puntatore+=strlen(buffer);
    if(fh.size_old+strlen(buffer)>512) parentDirectory.files[ret].size+=strlen(buffer);
    memcpy(mmapped_buffer+(parentDirectory.dir_indice*sizeof(struct DirectoryData)), &parentDirectory, sizeof(struct DirectoryData));
    printf("Operazione conclusa con successo\n");
}

void readFile(char* filename,struct DirectoryData parentDirectory, void *mmapped_buffer){
    if(&parentDirectory==NULL)handle_error("Errore: ParentDirectory inesistente\n");
    if(mmapped_buffer==NULL)handle_error("Errore: mmapped_buffer inesistente\n");
    if(containsForbiddenChars(filename)==1){//Scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    int ret=searchFile(filename,parentDirectory);
    if(ret==-1){
        printf("\tNon esiste il file %s nella directory %s\n",filename,parentDirectory.directoryname);
        return;
    }
    //File esiste quindi leggo
    FileHandle fh=openFile(parentDirectory.files[ret],0,mmapped_buffer);
    if(fh.ind_blocco==-1){
        printf("Errore: Non riesce ad aprire il file\n");
        return;
    }
    printf("Lettura del file %s\n",filename);
    while(fh.ind_blocco!=-1){
        struct DataBlock temp_db;
        memcpy(&temp_db, mmapped_buffer+LENDIR+LENFAT+(fh.ind_blocco*sizeof(struct DataBlock)), sizeof(struct DataBlock));
        for(int i=0;i<FATSIZE;i++){
            if(temp_db.data[i]==0)break;
            printf("%c",temp_db.data[i]);
        }
        memcpy(&fh.ind_blocco, mmapped_buffer+LENDIR+(fh.ind_blocco*sizeof(struct FATEntry)+sizeof(int)), sizeof(int));
    }
    parentDirectory.files[ret].ind_puntatore=fh.size_old;
    memcpy(mmapped_buffer+(parentDirectory.dir_indice*sizeof(struct DirectoryData)), &parentDirectory, sizeof(struct DirectoryData));
    printf("Operazione conclusa con successo\n");
}

void readFileFrom(char* filename,struct DirectoryData parentDirectory, void *mmapped_buffer){
    if(&parentDirectory==NULL)handle_error("Errore: ParentDirectory inesistente\n");
    if(mmapped_buffer==NULL)handle_error("Errore: mmapped_buffer inesistente\n");
    if(containsForbiddenChars(filename)==1){//Scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    int ret=searchFile(filename,parentDirectory);
    if(ret==-1){
        printf("\tNon esiste il file %s nella directory %s\n",filename,parentDirectory.directoryname);
        return;
    }
    //File esiste quindi leggo
    FileHandle fh=openFile(parentDirectory.files[ret],0,mmapped_buffer);
    if(fh.ind_blocco==-1){
        printf("Errore: Non riesce ad aprire il file\n");
        return;
    }
    printf("Lettura del file %s\n",filename);
    int nblocchi=(int)floor((double)fh.last_pos/1024);
    while(nblocchi>0){//Ti porta al blocco dove incominciare a leggere
        fh.last_pos-=1024;
        memcpy(&fh.ind_blocco, mmapped_buffer+LENDIR+(fh.ind_blocco*sizeof(struct FATEntry)+sizeof(int)), sizeof(int));
        nblocchi--;
    }
    struct DataBlock temp_db;
    memcpy(&temp_db, mmapped_buffer+LENDIR+LENFAT+(fh.ind_blocco*sizeof(struct DataBlock)), sizeof(struct DataBlock));
    for(int i=fh.last_pos;i<FATSIZE;i++){//Legge il primo blocco dal puntatore
        if(temp_db.data[i]==0)break;
        printf("%c",temp_db.data[i]);
    }
    //I prossimi blocchi li legge tutti
    memcpy(&fh.ind_blocco, mmapped_buffer+LENDIR+(fh.ind_blocco*sizeof(struct FATEntry)+sizeof(int)), sizeof(int));
    while(fh.ind_blocco!=-1){
        memcpy(&temp_db, mmapped_buffer+LENDIR+LENFAT+(fh.ind_blocco*sizeof(struct DataBlock)), sizeof(struct DataBlock));
        for(int i=0;i<FATSIZE;i++){
            if(temp_db.data[i]==0)break;
            printf("%c",temp_db.data[i]);
        }
        memcpy(&fh.ind_blocco, mmapped_buffer+LENDIR+(fh.ind_blocco*sizeof(struct FATEntry)+sizeof(int)), sizeof(int));
    }
    parentDirectory.files[ret].ind_puntatore=fh.size_old;
    memcpy(mmapped_buffer+(parentDirectory.dir_indice*sizeof(struct DirectoryData)), &parentDirectory, sizeof(struct DirectoryData));
    printf("Operazione conclusa con successo\n");
}

void seekFile(char* filename,struct DirectoryData parentDirectory, void *mmapped_buffer){
    if(&parentDirectory==NULL)handle_error("Errore: ParentDirectory inesistente\n");
    if(mmapped_buffer==NULL)handle_error("Errore: mmapped_buffer inesistente\n");
    if(containsForbiddenChars(filename)==1){//Scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    int ret=searchFile(filename,parentDirectory);
    if(ret==-1){
        printf("\tNon esiste il file %s nella directory %s\n",filename,parentDirectory.directoryname);
        return;
    }
    //File esiste quindi sposto
    FileHandle fh=openFile(parentDirectory.files[ret],1,mmapped_buffer);
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
    parentDirectory.files[ret].ind_puntatore=d;
    memcpy(mmapped_buffer+(parentDirectory.dir_indice*sizeof(struct DirectoryData)), &parentDirectory, sizeof(struct DirectoryData));
    printf("Operazione conclusa con successo\n");
}

void createDir(char* directoryname,struct DirectoryData parentDirectory,void *mmapped_buffer){
    if(&parentDirectory==NULL)handle_error("Errore: ParentDirectory inesistente\n");
    if(mmapped_buffer==NULL)handle_error("Errore: mmapped_buffer inesistente\n");
    if(containsForbiddenChars(directoryname)==1){//Scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    //Verifica esistenza del nome
    if(strlen(directoryname)<sizeof(parentDirectory.sub_directories[0]->directoryname)){
        for(int i=0; i<MAX_SUB;i++){
            if(parentDirectory.sub_directories[i]->directoryname[0]!='\0' && strcmp(parentDirectory.sub_directories[i]->directoryname,directoryname)==0){
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
        if (parentDirectory.sub_directories[i]->directoryname[0]=='\0'){
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
    struct DirectoryData temp_dir[MAXENTRY];
    memcpy(&temp_dir, mmapped_buffer, LENDIR);
    for(int i=0;i<MAXENTRY;i++){
        //DEBUG printf("numero dir %d",i);
        if (temp_dir[i].directoryname[0]=='\0'){
            j=i;
            break;
        }
    }
    if(j==-1){
        printf("Errore: Spazio insufficiente nella struct directory per una nuova directory\n");
        return;
    }
    //Crea subdirectory
    if(strncpy(temp_dir[j].directoryname, directoryname, sizeof(temp_dir[j].directoryname))==NULL)handle_error("Errore: Copia stringa per nome\n");
    //Inizializza
    for(int i=0;i<MAX_FILE;i++){
        temp_dir[j].files[i].size=0;//per controlli futuri
    }
    for(int i=0; i<MAX_SUB;i++){
        temp_dir[j].sub_directories[i]=(struct DirectoryData*)malloc(sizeof(struct DirectoryData));
        if(temp_dir[j].sub_directories[i]==NULL)handle_error("Errore: Allocazione struttura sub directory della directory\n");
        temp_dir[j].sub_directories[i]->directoryname[0]='\0';//per controlli futuri
    }
    temp_dir[j].par_directory=&temp_dir[parentDirectory.dir_indice];
    temp_dir[j].dir_indice=j;
    temp_dir[j].parentdir_indice=parentDirectory.dir_indice;
    temp_dir[parentDirectory.dir_indice].sub_directories[k]=&temp_dir[j];
    memcpy(mmapped_buffer, &temp_dir, LENDIR);
    printf("Creazione della subdirectory %s con successo\n",directoryname);
}

int eraseDir(char* directoryname,struct DirectoryData parentDirectory, void *mmapped_buffer){
    if(&parentDirectory==NULL)handle_error("Errore: ParentDirectory inesistente\n");
    if(mmapped_buffer==NULL)handle_error("Errore: mmapped_buffer inesistente\n");
    if(containsForbiddenChars(directoryname)==1){//Scritto male 
        printf("Errore: chiusura operazione per errore... \n");
        return parentDirectory.dir_indice;
    }
    //Cerco dir
    for(int s=0;s<MAX_SUB;s++){//trovo la subdirectory
        if(parentDirectory.sub_directories[s]->directoryname[0]!='\0'){
            if(strcmp(directoryname, parentDirectory.sub_directories[s]->directoryname)==0){
                memset(parentDirectory.sub_directories[s]->directoryname, 0, sizeof(parentDirectory.sub_directories[s]->directoryname));
                eraseDirAux(parentDirectory.sub_directories[s]->dir_indice, mmapped_buffer);
                printf("La subdirectory %s e' stata eliminata\n",directoryname);
                return parentDirectory.dir_indice;
            }
        }
    }
    printf("\tNon esiste la subdirectory %s\n",directoryname);
    return parentDirectory.dir_indice;
} 

void eraseDirAux(int ind,void *mmapped_buffer){
    if(ind==-1)handle_error("Errore: Dir inesistente\n");
    if(mmapped_buffer==NULL)handle_error("Errore: mmapped_buffer inesistente\n");
    struct DirectoryData temp_dir;
    memcpy(&temp_dir, mmapped_buffer+(ind*sizeof(struct DirectoryData)), sizeof(struct DirectoryData));
    for(int f=0;f<MAX_FILE;f++){//Elimina file
        if(temp_dir.files[f].size!=0){
            eraseFile(temp_dir.files[f].filename, temp_dir,mmapped_buffer);
        }
    }
    memcpy(&temp_dir, mmapped_buffer+(ind*sizeof(struct DirectoryData)), sizeof(struct DirectoryData));
    for(int s=0;s<MAX_SUB;s++){//Elimina sub_dir
        if(temp_dir.sub_directories[s]->directoryname[0]!='\0'){
            eraseDirAux(temp_dir.sub_directories[s]->dir_indice,mmapped_buffer);
        }
    }
    memset(mmapped_buffer+(ind*sizeof(struct DirectoryData)), 0, sizeof(struct DirectoryData));
    memcpy(&temp_dir, mmapped_buffer+(ind*sizeof(struct DirectoryData)), sizeof(struct DirectoryData));
    temp_dir.directoryname[0]='\0';//per controlli futuri
    //file
    for(int j=0;j<MAX_FILE;j++){
        temp_dir.files[j].size=0;//per controlli futuri
    }
    //subdirectories
    for(int j=0;j<MAX_SUB;j++){
        temp_dir.sub_directories[j]=(struct DirectoryData*)malloc(sizeof(struct DirectoryData));
        if(temp_dir.sub_directories[j]==NULL)handle_error("Errore: Allocazione struttura sub directory della directory\n");
        temp_dir.sub_directories[j]->directoryname[0]='\0';//per controlli futuri
    }
    memcpy( mmapped_buffer+(ind*sizeof(struct DirectoryData)), &temp_dir, sizeof(struct DirectoryData));
}

struct DirectoryData changeDir(struct DirectoryData dir,char *dirname,void *mmapped_buffer){
    if(&dir==NULL)handle_error("Errore: Dir inesistente\n");
    struct DirectoryData new_dir;
    if(containsForbiddenChars(dirname)==1){//Scritto male 
        printf("Chiusura operazione per errore... \n");
        return dir;
    }
    if(strcmp(dirname,"PREC")==0){//Torna indietro
        if(dir.par_directory==NULL){
            printf("Ti trovi nella root, non puoi andare indietro\n");
            return dir;
        }
        new_dir=puntatore_dir(dir.parentdir_indice,mmapped_buffer);
        if(&new_dir==NULL)handle_error("Errore nel cambio della nuova directory\n");
        printf("Ora ti trovi nella directory %s\n",new_dir.directoryname);
        return  new_dir;
    }else{
        for(int i=0;i<MAX_SUB;i++){//Cerca nella subdirectory
            if(strcmp(dirname,dir.sub_directories[i]->directoryname)==0){
                new_dir=puntatore_dir(dir.sub_directories[i]->dir_indice,mmapped_buffer);
                if(&new_dir==NULL)handle_error("Errore nel cambio della nuova directory\n");
                printf("Ora ti trovi nella directory %s\n",new_dir.directoryname);
                return new_dir;
            }
        }
        printf("Non esiste la directory %s, rimarrai nella directory %s\n",dirname,dir.directoryname);
        return dir;
    }
}

void listDir(char* dirname,struct DirectoryData dir,void *mmapped_buffer){
    if(&dir==NULL)handle_error("Errore: Dir inesistente\n");
    if(containsForbiddenChars(dirname)==1){//Scritto male 
        printf("Chiusura operazione per errore... \n");
        return;
    }
    if(strcmp(dirname,"CORR")==0){//Corrente
        printf("Oggetti nella directory corrente %s:\n",dir.directoryname);
        printf("Subdirectory presenti:\n");
        int count=1;
        for(int i=0;i<MAX_SUB;i++){
            if(dir.sub_directories[i]->directoryname[0]!='\0'){
                printf("[%d]\t%s\n",count,dir.sub_directories[i]->directoryname);
                count++;
            }
        }
        if(count==1)printf("\tNon esistono\n");
        count=1;
        printf("File presenti in:\n");
        for(int i=0;i<MAX_FILE;i++){
            if(dir.files[i].size!=0){
                printf("[%d]\t%s\n",count,dir.files[i].filename);
                count++;
            }
        }
        if(count==1)printf("\tNon esistono\n");
        return;
    }else{
    //Cerca la dir nella directory corrente
        struct DirectoryData copy;
        for(int i=0;i<MAX_SUB;i++){
            if(dir.sub_directories[i]->directoryname[0]!='\0'){
                if(strcmp(dirname,dir.sub_directories[i]->directoryname)==0){//Trovata e stampa
                    copy=puntatore_dir(dir.sub_directories[i]->dir_indice,mmapped_buffer);
                    printf("Oggetti nella subdirectory %s:\n",copy.directoryname);
                    printf("Subdirectory presenti in:\n");
                    int count=1;
                    for(int i=0;i<MAX_SUB;i++){
                        if(copy.sub_directories[i]->directoryname[0]!='\0'){
                            printf("[%d]\t%s\n",count,copy.sub_directories[i]->directoryname);
                            count++;
                        }
                    }
                    if(count==1)printf("\tNon esistono\n");
                    count=1;
                    printf("File presenti in:\n");
                    for(int i=0;i<MAX_FILE;i++){
                        if(copy.files[i].size!=0){
                            printf("[%d]\t%s\n",count,copy.files[i].filename);
                            count++;
                        }
                    }
                    if(count==1)printf("\tNon esistono\n");
                    return;
                }
            }
        }
    }
    printf("Non esiste la subdirectory %s, verifica di aver inserito il nome corretto\n",dirname);
}

struct DirectoryData puntatore_dir(int k,void *mmapped_buffer){
    if(k<0)handle_error("Errore: Indice errato\n");
    struct DirectoryData dir_corr;
    memcpy(&dir_corr, mmapped_buffer+(k*sizeof(struct DirectoryData)), sizeof(struct DirectoryData));
    return dir_corr;
}
