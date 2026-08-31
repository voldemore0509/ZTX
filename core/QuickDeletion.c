#include <stdio.h>
#include <sys/stat.h>

struct state info;

int cst(char *path){
    int i = 0;
    while(path[i] != "\0"){
        i ++;
    }
    return i++;
}

int modularity(char *path){
    char *writingpath = malloc(sizeof(char)*100);
    int result_cst = cst(path);
    char *rbt = realloc(writingpath,result_cst);
    if(rbt != NULL){
        writingpath = rbt;  
        return 0;
    }
    return 2;
}

int clear_memory(char *path){
    free(path);
    return 3;
}

int deletion_process(char *path , int size){
    int result = state(path,&info);
    if(result != 0){
        return 1;
    }
    return 0;
}



int main(void){
    return 0;
}
