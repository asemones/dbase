#include <stdlib.h>
#include <stdio.h>
#include "../../util/io.h"
#include <string.h>
#include <unistd.h>
#include "../../util/alloc_util.h"
#ifndef STORE_H
#define STORE_H

int save_file(char * data, char * file_name){
    if (data == NULL || file_name == NULL){
        fprintf(stdout,"no\n");
        return -1;
    }
    char path [50]= "/src/db/storage/";
    char fullpath[2048];
    getcwd(fullpath, sizeof(fullpath));
    strcat(fullpath, path);
    strncat(fullpath,file_name, strlen(file_name)+1);
    fprintf(stdout,"%s", fullpath);
    int l = write_file(data,fullpath, "wb", 1, strlen(data)+ 1);
    if (l <= 0){
        printf("%d", l);
        exit(0);
        return -1;
    }
    return 0;
}
int load_file(char * buffer, char * file_name){
    if (buffer == NULL || file_name == NULL){
        return -1;
    }
    char path [50]= "/src/db/storage/";
    char fullpath[2048];
    getcwd(fullpath, sizeof(fullpath));
    strcat(fullpath, path);
    strncat(fullpath,file_name, strlen(file_name)+1);
    fprintf(stdout,"%s", fullpath);
    int l  = read_file(buffer, fullpath, "rb",1,0);
    if (l <= 0){
        return -1;
    }
    return 0;
}
#endif

