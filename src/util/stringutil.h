#include <string.h>
#include <stdlib.h>
#include "../ds/byte_buffer.h"
#include "../ds/arena.h"
#include "alloc_util.h"

#ifndef STRINGUTIL_H
#define STRINGUTIL_H

/*TODO
    1: string struct
*/
typedef struct string {
    char *data;
    size_t length;
} string;


/** Function to find all occurrences of a target string within a source string
* @param source_string the source string to search within
* @param target_string the target string to search for
* @param num_occurrences an integer for the desired (maxiumum) number of occurrences
* @return an array of pointers to the occurrences of the target string within the source string
*/
char** find_all(char *source_string, char *target_string, int num_occurrences);

string *alloc_string(const char *data, arena *alloc) {
    string * s =  NULL;
    if (alloc != NULL){
        s = (string *)arena_alloc(alloc, sizeof(string));
        if (!s) return NULL;

        s->length = strlen(data);
        s->data = (char *)arena_alloc(alloc, s->length + 1);
        if (!s->data) {
            return NULL;
        }
        return s;
    }
    else{
        s = (string *)wrapper_alloc((sizeof(string)), NULL,NULL);
        if (!s) return NULL;

        s->length = strlen(data);
        s->data = (char *)wrapper_alloc((s->length + 1), NULL,NULL);
        if (!s->data) {
            free(s);
            return NULL;
        }
    }
    strncpy(s->data, data, s->length+1);
    return s;
}
string stack_str(const char *data) {
    string s;
    if (data == NULL) {
        s.length = 0;
        s.data = NULL;
        return s;
    }
    s.length = strlen(data);
    s.data = (char*)data;
    return s;
}
int string_cmp(string s1, string s2) {
    size_t i_1 =0;
    size_t i_2 = 0;
    while (i_1 < s1.length && i_2 < s2.length) {
        if (s1.data[i_1] != s2.data[i_2]) return s1.data[i_1] - s2.data[i_2];
        i_1++;
        i_2++;
    }
    return 0;
}
string * copy(string * dest,string * src, int index){
    if (dest == NULL || src == NULL || index < 0 || index >= src->length) return NULL;
    dest->length = src->length - index;
    dest->data = (char*)wrapper_alloc((dest->length + 1), NULL,NULL);
    strncpy(dest->data, &src->data[index], dest->length);
    return dest;
}
void free_string(string * s, arena * alloc) {
    if (alloc != NULL || s == NULL) return;
    free(s->data);
    free(s); 
}

char ** find_all(char * source_string, char * target_string, int num_occurrences){
    if (source_string == NULL || target_string == NULL || strlen(source_string) < strlen(target_string) || num_occurrences < 0 ){
        return NULL;
    }
    char* pos = source_string;
    char** occurences = (char**)wrapper_alloc((sizeof (char*)) * num_occurrences, NULL,NULL);
    int i = 0;
    // pointer to each string so we can access nearby elements
    //use strstr on each occurence
    while ((pos = strstr (pos, target_string))!= NULL && i < num_occurrences){
        occurences[i] = pos;
        pos+=1;
        i++;
    }
    while (i < num_occurrences) {
        occurences[i] = NULL;
        i++;
    } 
    return occurences;
}
size_t find_loc(const char *source, char target, size_t max_dist, size_t start_dist) {
    if (source == NULL) return -1; 
    char * loc  = memchr(source, target, max_dist);
    if (loc == NULL) return -1;
    return (size_t)(loc - source + start_dist);
}
int extract_bytes(char start_sep, char end_sep, byte_buffer *source, byte_buffer *dest) {
    if (source == NULL || dest == NULL) return -1;
    
    size_t max_dist = source->curr_bytes - source->read_pointer;

    size_t start_pos = find_loc(&source->buffy[source->read_pointer], start_sep, max_dist, source->read_pointer);
    if (start_pos >= max_dist) return -1;

    size_t end_pos = find_loc(&source->buffy[start_pos + 1], end_sep, max_dist - (start_pos - source->read_pointer) - 1, start_pos + 1);
    if (end_pos >= max_dist) return -1;

    size_t length = end_pos - (start_pos + 1);
    write_buffer(dest, &source->buffy[start_pos + 1], length);

    source->read_pointer = end_pos + 2;

    return 0;
}

void gen_sst_fname(size_t id, size_t level, char * buffer){
    snprintf(buffer, 100, "sst_%zu_%zu", id, level);
}

void grab_time_char(const char * buffer){
    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo = localtime(&rawtime);
    strftime((char*)buffer, 20,"%Y-%m-%d %H:%M:%S", timeinfo);
}
void grab_time(byte_buffer * buf){
    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo = localtime(&rawtime);
    char buff[128];

    strftime(buff, 128,"%Y-%m-%d %H:%M:%S", timeinfo);
    write_buffer(buf, buff, strlen(buff));
    write_buffer(buf, ",",1);
    
}
bool C_strn_between(const char * s, const char * max, const char * min){
    int max_ccmp_char = strlen(s);

    return strncmp(s, min,max_ccmp_char) >=0 && strncmp(s,max, max_ccmp_char) <=0;
}



#endif