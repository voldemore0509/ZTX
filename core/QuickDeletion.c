#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

struct stat info;

int cst(char *path){
    int i = 0;
    while(path[i] != '\0'){
        i ++;
    }
    return i;
}

int dpbotss(char *path , int size){  //Deletion process based on the specified size
    DIR *d = opendir(path);
    if(d == NULL){  //verifie si le dossier s'ouvre
        return 1;  //erreur
    }
    int i = 0;
    DIR *return_reddir = readdir(d);
    while( != NULL){
        if(i > size){
        }
        i ++;
    }
    closedir(d);
    return 0;
}

int modularity(char *path,char **out){
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
    return 0;
}
