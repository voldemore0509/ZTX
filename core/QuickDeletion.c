#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

struct stat info;

int verif_size_of_file(struct dirent *scannedFileInfo , int *size_max){     //verification size of the file
    printf("Taille du ficher : %i\n", sizeof(scannedFileInfo->d_name));
    if(sizeof(scannedFileInfo->d_name) > size_max){
        printf("Taille du ficher supérieur a la taille requise");
        return 1; // ficher détécter 
    }
    else{
        printf("Taille du ficher dans les normes");
        return 0; //aucun problème
    }
}

int cst(char *path){
    int i = 0;
    while(path[i] != '\0'){
        i ++;
    }
    return i;
}

int dpbotss(char *path , int *size){  //Deletion process based on the specified size
    DIR *d = opendir(path);
    if(d == NULL){  //verifie si le dossier s'ouvre
        perror("opendir");
        return 1;  //erreur
    }
    struct dirent *scannedFileInfo;     //Informations sur le fichier numérisé (ou scanné)
    int return_vsof ;
    while((scannedFileInfo = readdir(d)) != NULL){
        printf("Trouve : %s\n", scannedFileInfo->d_name);
        return_vsof = verif_size_of_file(scannedFileInfo , size);
    }
    closedir(d);
    return 0;
}

int modularity(char *path,char **out){  //allocation de la mémoire pour un ficher
    char *writingpath = malloc(sizeof(char)*100);
    int result_cst = cst(path);
    char *rbt = realloc(writingpath,result_cst+1);
    if(rbt != NULL){
        writingpath = rbt;
        strcpy(writingpath, path);  
        *out = writingpath;
        return 0;
    }
    free(writingpath);
    return 2;
}

int clear_memory(char *path){
    free(path);
    return 3;
}

int deletion_process(char *path , int size, char **out){
    int result_allocation_memory = modularity(path ,out);
    if(result_allocation_memory != 0){
        return 2;
    }
    int result = stat(path,&info);
    if(result != 0){
        return 1;
    }
    clear_memory(*out);
    return 0;
}

int main(void){
    int result = dpbotss("C:/c.ai.prompt/pgs",100);
    printf("%i",result);
    return 0;
}
