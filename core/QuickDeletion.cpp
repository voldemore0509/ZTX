#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

class MyStd{
    public:

    int cst(const char *path){      //compte la taille d'une chaine
        int i = 0;
        while(path[i] != '\0'){
            i ++;
        }
        return i;
    }

    int modularity(const char *path, char **out){   //allocation de la memoire pour un chemin
        int result_cst = cst(path);
        char *writingpath = (char*)malloc(result_cst + 1);  //+1 pour le '\0'
        if(writingpath == NULL){
            return 2;
        }
        strcpy(writingpath, path);
        *out = writingpath;
        return 0;
    }

    int clear_memory(char *path){
        free(path);
        return 3;
    }
};

class DeleteProcess{
    public :

    MyStd tools;    //composition : un DeleteProcess UTILISE un MyStd

    int verif_size_of_file(const char *fullpath, long size_max){    //verification size of the file
        struct stat info;
        if(stat(fullpath, &info) != 0){     //interroge le systeme sur ce fichier
            perror("stat");
            return -1;      //erreur
        }
        printf("Taille du fichier : %lld octets\n", (long long)info.st_size);
        if(info.st_size > size_max){
            printf("Taille du fichier superieure a la taille requise\n");
            return 1;   //fichier detecte
        }
        else{
            printf("Taille du fichier dans les normes\n");
            return 0;   //aucun probleme
        }
    }

    int dpbotss(const char *path, long size){   //Deletion process based on the specified size
        DIR *d = opendir(path);
        if(d == NULL){      //verifie si le dossier s'ouvre
            perror("opendir");
            return 1;   //erreur
        }
        struct dirent *scannedFileInfo;     //Informations sur le fichier scanne
        char fullpath[1024];
        int return_vsof;
        while((scannedFileInfo = readdir(d)) != NULL){
            if(strcmp(scannedFileInfo->d_name, ".") == 0 ||
               strcmp(scannedFileInfo->d_name, "..") == 0){
                continue;   //on ignore le dossier courant et le parent
            }
            printf("Trouve : %s\n", scannedFileInfo->d_name);
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, scannedFileInfo->d_name);
            return_vsof = verif_size_of_file(fullpath, size);
        }
        closedir(d);
        return 0;
    }

    int deletion_process(const char *path, long size, char **out){
        int result_allocation_memory = tools.modularity(path, out);
        if(result_allocation_memory != 0){
            return 2;
        }
        struct stat info;
        int result = stat(path, &info);
        if(result != 0){
            tools.clear_memory(*out);   //liberer avant de sortir, sinon fuite memoire
            *out = NULL;
            return 1;
        }
        tools.clear_memory(*out);
        *out = NULL;
        return 0;
    }
};

int main(void){
    DeleteProcess dp;
    int result = dp.dpbotss("C:/c.ai.prompt/pgs", 100);
    printf("%i\n", result);
    return 0;
}
