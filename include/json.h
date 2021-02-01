#pragma once

#include "utils.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct JSON_Element JSON_Element;

typedef enum JSON_Element_Type {
    JSON_NULL,
    JSON_ARRAY,
    JSON_OBJECT,
    JSON_INTEGER,
    JSON_DOUBLE,
    JSON_STRING,
    JSON_TRUE,
    JSON_FALSE,
    JSON_PARSE_ERROR,//JSON_PARSE_ERROR is a JSON_STRING that describes the error that happened during json_parse
} JSON_Element_Type;

typedef struct JSON_Object_Table JSON_Object_Table;

typedef struct JSON_Object {
    JSON_Element_Type type;
    JSON_Object_Table * tbl;
} JSON_Object;

JSON_Object * json_make_object();

JSON_Element * json_object_get(JSON_Object *,const char * key);//pointers returned from this are 'fragile' they may break when modifying the object
JSON_Element * json_object_get_n(JSON_Object *,const char * key,size_t n);//pointers returned from this are 'fragile' they may break when modifying the object

void json_object_set(JSON_Object *,const char * key,JSON_Element * elem);//elem pointer is invalidated
void json_object_set_n(JSON_Object *,const char * key,size_t n,JSON_Element * elem);//elem pointer is invalidated

void json_free_object(JSON_Object *);

typedef struct JSON_Array {
    JSON_Element_Type type;
    size_t size;
    size_t alloc;
    JSON_Element * arr;
} JSON_Array;

JSON_Array * json_make_array();

void json_free_array(JSON_Array *);

JSON_Element * json_array_get(JSON_Array * arr,size_t index);//pointers returned from this are 'fragile' they may break when modifying the array

void json_array_set(JSON_Array * arr,JSON_Element * elem,size_t index);//elem pointer is invalidated

void json_array_push(JSON_Array * arr,JSON_Element * elem);//elem pointer is invalidated

int json_array_insert(JSON_Array * arr,JSON_Element * elem,size_t index);//if returns 1, insertion failed and passed elem pointer is still valid, otherwise elem pointer is invalidated

void json_array_remove(JSON_Array * arr,size_t index);

typedef struct JSON_String {
    JSON_Element_Type type;
    size_t len;
    char * str;
} JSON_String;

JSON_String * json_make_string(const char * s);

JSON_String * json_make_string_n(const char * s,size_t n);

void json_set_string(JSON_String * str,const char * s);

void json_set_string_n(JSON_String * str,const char * s,size_t n);

void json_free_string(JSON_String *);

typedef struct JSON_Integer {
    JSON_Element_Type type;
    int64_t i;
} JSON_Integer;

JSON_Integer * json_make_integer(int64_t i);

typedef struct JSON_Double {
    JSON_Element_Type type;
    double d;
} JSON_Double;

JSON_Double * json_make_double(double d);

struct JSON_Element {
    union {
        JSON_Element_Type type;
        JSON_Array _arr;
        JSON_Object _obj;
        JSON_String _str;
        JSON_Integer _int;
        JSON_Double _double;
    };
};

void json_free_element(JSON_Element *);

JSON_Element * json_parse_n(const char * s,size_t n);

JSON_Element * json_parse(const char * s);

void json_print_element(JSON_Element *,size_t indentation);
void json_print_object(JSON_Object *,size_t indentation);
void json_print_array(JSON_Array *,size_t indentation);
void json_print_string(JSON_String *,size_t indentation);

#ifdef __cplusplus
}
#endif // __cplusplus

