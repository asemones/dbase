#include <stdio.h>
#include <string.h>
#include "../../ds/hashtbl.h"
#include "../../ds/byte_buffer.h"
#include "../../util/alloc_util.h"
#ifndef JSON_H
#define JSON_H
//why did i do this
enum json_types{
    STRING,
    NUMBER,
    OBJECT,
    ARRAY,
    BOOL,
};

void seralize_field(const char* key, const char* value, byte_buffer * buffer, const enum json_types type){
    write_buffer(buffer, "\"", 1);
    int length = strlen(key);
    write_buffer(buffer, (char*)key, length);
    write_buffer(buffer, "\":", 2);
    if (type == STRING){
        write_buffer(buffer, "\"", 1);
        length = strlen(value);
        write_buffer(buffer, (char*)value, length);
        write_buffer(buffer, "\"", 1);
    }
    else if (type == NUMBER){
        length = sizeof(size_t); 
        write_buffer(buffer, (char*)value, length);
    }
    write_buffer(buffer, ",", 1);
    return;

}
int seralize_dict(dict * table, byte_buffer * buffer, enum json_types type){
    if (table->size == 0) return -1;
    write_buffer(buffer, "{", 1);
    for (int i = 0; i < table->capacity; i++){
        if (((void**)table->keys)[i]== NULL || ((void**)table->values)[i]==NULL) continue;
        seralize_field((char*)((void**)table->keys)[i], (char*)((void**)table->values)[i], buffer,type);
    }
    buffer->buffy[buffer->curr_bytes-1] = '}';
    return 0;
}
int findJsonType(const char value){
    switch (value)
    {
        case '\"':
            return STRING;
            break;
        case '{':
            return OBJECT;
            break;
        case '[':  
            return ARRAY;
            break;
        case 't':
            return BOOL;
            break;
        case 'f':
            return BOOL;
            break;
        default:
            return NUMBER;
    }
}
size_t deseralize_one(void (*insert_func)(void *, void *, void *), byte_buffer * buffer, byte_buffer * key_values, void * data_struct){
    char * ret_check = NULL;

    if ((ret_check = go_nearest_v(buffer, '\"')) == NULL) return -1;
    char *key_start = NULL, *key_end = NULL, *value_start = NULL, *value_end = NULL, *real_key_start = NULL , *real_value_start = NULL;
    key_start = get_next(buffer);
    if ((ret_check = go_nearest_v(buffer, '\"')) == NULL) return -1;
    key_end = get_curr(buffer);


    size_t key_length = key_end - key_start;
    real_key_start = &key_values->buffy[key_values->curr_bytes];
    write_buffer(key_values, key_start, key_length);
    write_buffer(key_values, "\0", 1);

    if ((ret_check = go_nearest_v(buffer, ':')) == NULL) return -1;

    if ((ret_check = go_nearest_v(buffer, '\"')) == NULL) return -1;
    value_start = get_curr(buffer);
    int type = findJsonType(*value_start);

    switch (type) {
        case STRING:
                    value_start = get_next(buffer);
                    real_value_start =  &key_values->buffy[key_values->curr_bytes];
                    while(1){
                        value_end = go_nearest_v(buffer, '\"');
                        if (value_end != NULL && *(value_end - 1) != '\\') break; 
                        if (value_end == NULL) return -1;           
                    }
                    if (value_end == NULL) return -1;
            
            break;
        case NUMBER:
            real_value_start = &key_values->buffy[key_values->curr_bytes];    
            value_end = &buffer->buffy[buffer->read_pointer + sizeof(size_t)];
            buffer->read_pointer += sizeof(size_t);
            break;
        case OBJECT:
           
            break;
        case ARRAY:
           
            break;
        case BOOL:
           
            break;
        default:
            break;
    }
    size_t value_length = value_end - value_start;
    write_buffer(key_values, value_start, value_length);
    write_buffer(key_values, "\0", 1);
    (insert_func(real_key_start, real_value_start, data_struct));
    if ((ret_check = go_nearest_v(buffer, ',')) == NULL) return -1;
    return 0;

}
size_t deseralize_into_structure(void (*insert_func)(void *, void *, void *),void * structure, byte_buffer * buffer, byte_buffer * key_values) {
    size_t ret = 0;
    while(buffer->read_pointer < buffer->curr_bytes && ret != -1 ){
        ret = deseralize_one(insert_func,buffer, key_values, structure);
    }
    return 0;
}
size_t back_track(byte_buffer * buffer, const char targ, size_t curr_pos, size_t max_len){
    
    while(curr_pos >0){
        const char c = buffer->buffy[curr_pos];
        if (c == targ && curr_pos < max_len) return curr_pos;
        curr_pos--;
    }
    return curr_pos;
}
int get_next_key(byte_buffer * buffer, char * store){
    char * ret_check = NULL;
    if ((ret_check = go_nearest_v(buffer, '\"')) == NULL) return -1;
    char *key_start = NULL, *key_end = NULL;
    key_start = get_next(buffer);
    if ((ret_check = go_nearest_v(buffer, '\"')) == NULL)return -1;
    key_end = get_curr(buffer);
    size_t key_length = key_end - key_start;
    memcpy(store, key_start, key_length);
    store[key_length] = '\0';
    go_nearest_v(buffer, ':');
    get_next(buffer);
    go_nearest_v(buffer, '\"');
    get_next(buffer);
    go_nearest_v(buffer, '\"');
    get_next(buffer);

    return key_length;
}
#endif