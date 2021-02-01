#include "json.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#define OOM_EXIT() err_exit("Out of Memory in %s",__func__)

static void json_cleanup_element(void * p);

typedef struct JSON_Object_Table_Elem {
    uint32_t size;
    uint32_t alloc;
    void * arr;
} JSON_Object_Table_Elem;

typedef struct JSON_Object_Table {
    uint32_t num_buckets;
    uint32_t item_size;
    JSON_Object_Table_Elem buckets[];
} JSON_Object_Table;

typedef JSON_Object_Table table;
typedef JSON_Object_Table_Elem table_elem;

static table * alloc_table(size_t num_buckets,size_t item_size){
    table * tbl=calloc(1,sizeof(table)+
                         (num_buckets*sizeof(table_elem)));
    tbl->num_buckets=num_buckets;
    tbl->item_size=item_size;
    return tbl;
}

static void table_cleanup(table * tbl,void (*cleanup)(void*)){
    uint32_t isz=tbl->item_size;
    for(uint32_t i=0;i<tbl->num_buckets;i++){
        if(tbl->buckets[i].arr){
            uint32_t sz=tbl->buckets[i].size*isz;
            for(uint32_t j=0;j<sz;j+=isz){
                if(cleanup) cleanup(&((uint8_t *)tbl->buckets[i].arr)[j]);
            }
            free(tbl->buckets[i].arr);
        }
    }
    free(tbl);
}

static void table_add_item(table * tbl,void * item,uint32_t (*hash)(void*)){
    table_elem * e=&tbl->buckets[hash(item)%tbl->num_buckets];
    if(e->arr){
        if(e->alloc==e->size){
            uint32_t new_alloc=e->alloc*2;//growth factor 2
            e->arr=realloc(e->arr,new_alloc*tbl->item_size);
            if(!e->arr){
                OOM_EXIT();
            }
            e->alloc=new_alloc;
        }
    }else{
        e->arr=calloc(4,tbl->item_size);
        e->alloc=4;
    }
    memcpy((uint8_t*)e->arr+((e->size++)*tbl->item_size),item,tbl->item_size);
}

static void * table_find_item(table * tbl,void * key,uint32_t (*hash)(void*),int (*compare)(void*,void*)){
    table_elem * e=&tbl->buckets[hash(key)%tbl->num_buckets];
    if(!(e->arr&&e->size)) return NULL;
    uint8_t * arr=e->arr;
    uint32_t isz=tbl->item_size;
    uint32_t sz=e->size*isz;
    for(uint32_t i=0;i<sz;i+=isz){
        if(compare(&arr[i],key)){
            return &arr[i];
        }
    }
    return NULL;
}

typedef struct JSON_ObjectEntry {
    char * key;
    JSON_Element elem;
} JSON_ObjectEntry;

JSON_Object * json_make_object(){
    JSON_Object * obj=calloc(1,sizeof(JSON_Element));
    obj->type=JSON_OBJECT;
    obj->tbl=alloc_table(32,sizeof(JSON_ObjectEntry));
    return obj;
}

[[maybe_unused]]
static int json_object_find_compare_keys(void * item,void * key){
    return strcmp(((JSON_ObjectEntry*)item)->key,(const char *)key)==0;
}

[[maybe_unused]]
static uint32_t json_object_item_hash(void * item){
    return str_hash(((JSON_ObjectEntry*)item)->key);
}

[[maybe_unused]]
static uint32_t json_object_key_hash(void * key){
    return str_hash((const char *)key);
}

[[maybe_unused]]
static void json_object_item_cleanup(void * item){
    free(((JSON_ObjectEntry*)item)->key);
    json_cleanup_element(&((JSON_ObjectEntry*)item)->elem);
}

JSON_Element * json_object_get_n(JSON_Object * obj,const char * key,size_t n){
    JSON_ObjectEntry * entry=table_find_item(obj->tbl,(void*)key,json_object_key_hash,json_object_find_compare_keys);
    if(entry){
        return &entry->elem;
    }else{
        return NULL;
    }
}

JSON_Element * json_object_get(JSON_Object * obj,const char * key){
    return json_object_get_n(obj,key,strlen(key));
}

void json_object_set_n(JSON_Object * obj,const char * key,size_t n,JSON_Element * elem){
    JSON_ObjectEntry * entry=table_find_item(obj->tbl,(void*)key,json_object_key_hash,json_object_find_compare_keys);
    if(entry){
        json_cleanup_element(&entry->elem);
        memcpy(&entry->elem,elem,sizeof(JSON_Element));
    }else{
        JSON_ObjectEntry new_entry = {
            .key=calloc(n+1,sizeof(char)),
            .elem={{0}},
        };
        memcpy(new_entry.key,key,n);
        memcpy(&new_entry.elem,elem,sizeof(JSON_Element));
        table_add_item(obj->tbl,&new_entry,json_object_item_hash);
    }
    free(elem);
}

void json_object_set(JSON_Object * obj,const char * key,JSON_Element * elem){
    json_object_set_n(obj,key,strlen(key),elem);
}

static void json_cleanup_object(JSON_Object * obj){
    if(!obj)return;
    table_cleanup(obj->tbl,json_cleanup_element);
}

void json_free_object(JSON_Object * obj){
    if(!obj)return;
    json_cleanup_object(obj);
    free(obj);
}

JSON_Array * json_make_array(){
    JSON_Array * arr=calloc(1,sizeof(JSON_Element));
    arr->type=JSON_ARRAY;
    return arr;
}

static void json_cleanup_array(JSON_Array * arr){
    if(!arr)return;
    if(arr->arr){
        size_t sz=arr->size;
        for(size_t i=0;i<sz;i++){
            json_cleanup_element(&arr->arr[i]);
        }
        free(arr->arr);
    }
}

void json_free_array(JSON_Array * arr){
    if(!arr)return;
    json_cleanup_array(arr);
    free(arr);
}

JSON_Element * json_array_get(JSON_Array * arr,size_t index){
    if(index>arr->size){
        return NULL;
    }
    return arr->arr+index;
}

void json_array_set(JSON_Array * arr,JSON_Element * elem,size_t index){
    if(index>arr->size)return;
    json_cleanup_element(arr->arr+index);
    memcpy(arr->arr+index,elem,sizeof(JSON_Element));
    free(elem);
}

static void json_array_grow_by(JSON_Array * arr,size_t by){
    if(arr->alloc>=(arr->size+by))return;
    if(arr->arr){
        arr->arr=realloc(arr->arr,(arr->size+by)*sizeof(JSON_Element));
    }else{
        arr->arr=calloc(by,sizeof(JSON_Element));
    }
    if(!arr->arr){
        OOM_EXIT();
    }
}

void json_array_push(JSON_Array * arr,JSON_Element * elem){
    json_array_grow_by(arr,1);
    memcpy(arr->arr+arr->size,elem,sizeof(JSON_Element));
    ++arr->size;
    free(elem);
}

int json_array_insert(JSON_Array * arr,JSON_Element * elem,size_t index){
    if(arr->size>index){
        json_array_grow_by(arr,1);
        memmove(arr->arr+index+1,arr->arr+index,((arr->size-index)-1)*sizeof(JSON_Element));
        ++arr->size;
        memcpy(arr->arr+index,elem,sizeof(JSON_Element));
        free(elem);
    }else if(arr->size==index){
        json_array_grow_by(arr,1);
        ++arr->size;
        memcpy(arr->arr+index,elem,sizeof(JSON_Element));
        free(elem);
    }else{
        //COULD NOT ADD, INVALID INDEX
        return 1;
    }
    return 0;
}

void json_array_remove(JSON_Array * arr,size_t index){
    if(arr->size>index){
        json_cleanup_element(arr->arr+index);
        --arr->size;
        if(arr->size>index) memmove(arr->arr+index,arr->arr+index+1,(arr->size-index)*sizeof(JSON_Element));
    }
}

JSON_String * json_make_string_n(const char * s,size_t n){
    JSON_String * str=calloc(1,sizeof(JSON_Element));
    str->type=JSON_STRING;
    str->str=calloc(n+1,sizeof(char));
    memcpy(str->str,s,n);
    str->str[n]=0;
    str->len=n;
    return str;
}

JSON_String * json_make_string(const char * s){
    return json_make_string_n(s,strlen(s));
}

void json_set_string_n(JSON_String * str,const char * s,size_t n){
    free(str->str);
    str->str=calloc(n+1,sizeof(char));
    memcpy(str->str,s,n);
    str->str[n]=0;
    str->len=n;
}

void json_set_string(JSON_String * str,const char * s){
    json_set_string_n(str,s,strlen(s));
}

void json_cleanup_string(JSON_String * str){
    if(!str)return;
    free(str->str);
}

void json_free_string(JSON_String * str){
    if(!str)return;
    free(str->str);
    free(str);
}


JSON_Integer * json_make_integer(int64_t i){
    JSON_Integer * ie=calloc(1,sizeof(JSON_Element));
    ie->type=JSON_INTEGER;
    ie->i=i;
    return ie;
}

JSON_Double * json_make_double(double d){
    JSON_Double * de=calloc(1,sizeof(JSON_Element));
    de->type=JSON_DOUBLE;
    de->d=d;
    return de;
}

static void json_cleanup_element(void * p){
    if(!p)return;
    JSON_Element * elem=p;
    switch(elem->type){
    case JSON_ARRAY:
        json_cleanup_array(p);
        break;
    case JSON_OBJECT:
        json_cleanup_object(p);
        break;
    case JSON_PARSE_ERROR:
    case JSON_STRING:
        json_cleanup_string(p);
        break;
    default:
        break;
    }
}

void json_free_element(JSON_Element * elem){
    if(!elem)return;
    json_cleanup_element(elem);
    free(elem);
}

typedef struct parse_data {
    size_t i;
    size_t n;
    const char * s;
} parse_data;

JSON_Element * parse_error(const char * fmt,...){
    JSON_String * str=calloc(1,sizeof(JSON_Element));
    va_list arg1,arg2;
    va_start(arg1,fmt);
    va_copy(arg2,arg1);
    size_t n=vsnprintf(NULL,0,fmt,arg2);
    va_end(arg2);
    str->type=JSON_PARSE_ERROR;
    str->str=calloc(n+1,sizeof(char));
    vsnprintf(str->str,n+1,fmt,arg1);
    va_end(arg1);
    str->str[n]=0;
    str->len=n;
    return (JSON_Element*)str;
}

static bool is_whitespace(char c){
    return c==' '||c=='\t'||c=='\n'||c=='\r';
}

void skip_whitespace(parse_data * p){
    while(p->i<p->n){
        if(is_whitespace(p->s[p->i])){
            ++p->i;
        }else if(p->s[p->i]=='/'&&(p->i+1<p->n)&&((p->s[p->i+1]=='/')||(p->s[p->i+1]=='*'))){
            if(p->s[p->i+1]=='/'){
                p->i+=2;
                while(p->i<p->n&&p->s[p->i]!='\n')++p->i;
                if(p->i<p->n)++p->i;
            }else{
                
                p->i+=2;
                if(p->i<p->n)++p->i;
                while(p->i<p->n&&p->s[p->i-1]!='*'&&p->s[p->i]!='/')++p->i;
                if(p->i<p->n)++p->i;
            }
        }else{
            break;
        }
    }
}

JSON_Element * json_parse(const char * s){
    return json_parse_n(s,strlen(s));
}

static char unescape(char c){
    switch(c) {
    case 'a':
        return '\a';
    case 'b':
        return '\b';
    case 't':
        return '\t';
    case 'n':
        return '\n';
    case 'v':
        return '\v';
    case 'f':
        return '\f';
    case 'r':
        return '\r';
    default:
        return c;
    }
}

static JSON_Element * json_parse_string(parse_data * p){
    skip_whitespace(p);
    bool singlequote=false;
    if(p->i>=p->n){
        return parse_error("Expected '\"', got EOF");
    }else if(p->s[p->i]=='\''){
        singlequote=true;
    }else if(p->s[p->i]!='"'){
        return parse_error("Expected '\"', got %c",p->s[p->i]);
    }
    ++p->i;
    bool reading_escape=false;
    size_t n=0,i=p->i;
    for(;i<p->n;++i){
        if(reading_escape){
            n++;
            reading_escape=false;
        }else if(p->s[i]=='\\'){
            reading_escape=true;
        }else if(p->s[i]==(singlequote?'\'':'"')){
            break;
        }else{
            n++;
        }
    }
    if(i>=p->n){
        return parse_error(reading_escape?"Expected '\"', got EOF":"Expected char got EOF");
    }
    JSON_String * str=calloc(1,sizeof(JSON_Element));
    str->type=JSON_STRING;
    str->str=calloc(n+1,sizeof(char));
    str->str[n]=0;
    str->len=n;
    reading_escape=false;
    n=0;
    for(;p->i<p->n;++p->i){
        if(reading_escape){
            str->str[n++]=unescape(p->s[p->i]);
            reading_escape=false;
        }else if(p->s[p->i]=='\\'){
            reading_escape=true;
        }else if(p->s[p->i]==(singlequote?'\'':'"')){
            break;
        }else{
            str->str[n++]=p->s[p->i];
        }
    }
    ++p->i;
    return (JSON_Element *)str;
}

JSON_Element * json_parse_element(parse_data * p);

JSON_Element * json_parse_object(parse_data * p){
    skip_whitespace(p);
    if(p->i>=p->n){
        return parse_error("Expected '{', got EOF");
    }else if(p->s[p->i]!='{'){
        return parse_error("Expected '{', got %c",p->s[p->i]);
    }
    ++p->i;
    JSON_Object * obj=json_make_object();
    while(true){
        JSON_String * key=(JSON_String *)json_parse_string(p);
        if(key->type==JSON_PARSE_ERROR){
            json_free_object(obj);
            return (JSON_Element*)key;
        }
        skip_whitespace(p);
        if(p->i>=p->n){
            json_free_string(key);
            json_free_object(obj);
            return parse_error("Expected ':', got EOF");
        }else if(p->s[p->i]!=':'){
            json_free_string(key);
            json_free_object(obj);
            return parse_error("Expected ':', got %c",p->s[p->i]);
        }
        ++p->i;
        JSON_Element * e=json_parse_element(p);
        if(e->type==JSON_PARSE_ERROR){
            json_free_string(key);
            json_free_object(obj);
            return e;
        }
        json_object_set_n(obj,key->str,key->len,e);
        json_free_string(key);
        skip_whitespace(p);
        if(p->i>=p->n){
            json_free_object(obj);
            return parse_error("Expected '}', got EOF");
        }else if(p->s[p->i]==','){
            ++p->i;
            skip_whitespace(p);
            if(p->s[p->i]=='}'){
                ++p->i;
                return (JSON_Element*)obj;
            }
        }else if(p->s[p->i]=='}'){
            ++p->i;
            return (JSON_Element*)obj;
        }else{
            json_free_object(obj);
            return parse_error("Expected '}', got %c",p->s[p->i]);
        }
    }
    json_free_object(obj);
    return parse_error("Expected '}', got EOF");
}

JSON_Element * json_parse_array(parse_data * p){
    skip_whitespace(p);
    if(p->i>=p->n){
        return parse_error("Expected '[', got EOF");
    }else if(p->s[p->i]!='['){
        return parse_error("Expected '[', got %c",p->s[p->i]);
    }
    ++p->i;
    JSON_Array * arr=json_make_array();
    while(true){
        JSON_Element * e=json_parse_element(p);
        if(e->type==JSON_PARSE_ERROR){
            json_free_array(arr);
            return e;
        }
        json_array_push(arr,e);
        skip_whitespace(p);
        if(p->i>=p->n){
            json_free_array(arr);
            return parse_error("Expected ']', got EOF");
        }else if(p->s[p->i]==','){
            ++p->i;
            skip_whitespace(p);
            if(p->s[p->i]==']'){
                ++p->i;
                return (JSON_Element*)arr;
            }
        }else if(p->s[p->i]==']'){
            ++p->i;
            return (JSON_Element*)arr;
        }else{
            json_free_array(arr);
            return parse_error("Expected ']', got %c",p->s[p->i]);
        }
    }
    json_free_array(arr);
    return parse_error("Expected ']', got EOF");
}

typedef union numberdata {
    double d;
    int64_t i;
} numberdata;

JSON_Element * json_parse_number(parse_data * p){
    skip_whitespace(p);
    if(p->i>=p->n) return parse_error("Expected JSON Element, got EOF");
    bool is_double=false;
    bool is_negative=false;
    bool is_valid=false;
    size_t double_digit=1;
    numberdata number={0};
    switch(p->s[p->i]){
    case '+':
        is_negative=false;
        ++p->i;
        break;
    case '-':
        is_negative=true;
        ++p->i;
        break;
    case '.':
        is_double=true;
        number.d=0;
        ++p->i;
        break;
    default:
        if(p->s[p->i]<'0'||p->s[p->i]>'9'){
            return parse_error("Expected Number, got %c",p->s[p->i]);
        }
        break;
    }
    for(;p->i<p->n;++p->i){
        char c=p->s[p->i];
        if(c>='0'&&c<='9'){
            is_valid=true;
            if(is_double){
                number.d+=(c-'0')/pow(10,double_digit);
                double_digit++;
            }else{
                number.i*=10;
                number.i+=c-'0';
            }
        }else if(c=='.'){
            if(is_double){
                return parse_error("Expected Number, got %c",c);
            }
            is_double=true;
            number.d=number.i;
        }else if(is_valid){
            if(is_double){
                JSON_Double * d=calloc(1,sizeof(JSON_Element));
                d->type=JSON_DOUBLE;
                d->d=is_negative?-number.d:number.d;
                return (JSON_Element*)d;
            }else{
                JSON_Integer * i=calloc(1,sizeof(JSON_Element));
                i->type=JSON_INTEGER;
                i->i=is_negative?-number.i:number.i;
                return (JSON_Element*)i;
            }
        }else{
            return parse_error("Expected Number, got %c",c);
        }
    }
    if(is_valid){
        if(is_double){
            JSON_Double * d=calloc(1,sizeof(JSON_Element));
            d->type=JSON_DOUBLE;
            d->d=is_negative?-number.d:number.d;
            return (JSON_Element*)d;
        }else{
            JSON_Integer * i=calloc(1,sizeof(JSON_Element));
            i->type=JSON_INTEGER;
            i->i=is_negative?-number.i:number.i;
            return (JSON_Element*)i;
        }
    }
    return parse_error("Expected Number, got EOF");
}

JSON_Element * json_parse_element(parse_data * p){
    skip_whitespace(p);
    if(p->i>=p->n) return parse_error("Expected JSON Element, got EOF");
    char c=p->s[p->i];
    switch(c){
    case '{':
        return json_parse_object(p);
    case '[':
        return json_parse_array(p);
    case '"':
    case '\'':
        return json_parse_string(p);
    default:
        if((c>='0'&&c<='9')||c=='.'||c=='-'||c=='+'){
            return json_parse_number(p);
        }else if((p->i+4)<p->n&&p->s[p->i]=='f'&&p->s[p->i+1]=='a'&&p->s[p->i+2]=='l'&&p->s[p->i+3]=='s'&&p->s[p->i+3]=='e'){
            JSON_Element * e=calloc(1,sizeof(JSON_Element));
            e->type=JSON_FALSE;
            p->i+=5;
            return e;
        }else if((p->i+3)<p->n){
            if(p->s[p->i]=='t'&&p->s[p->i+1]=='r'&&p->s[p->i+2]=='u'&&p->s[p->i+3]=='e'){
                JSON_Element * e=calloc(1,sizeof(JSON_Element));
                e->type=JSON_TRUE;
                p->i+=4;
                return e;
            }else if(p->s[p->i]=='n'&&p->s[p->i+1]=='u'&&p->s[p->i+2]=='l'&&p->s[p->i+3]=='l'){
                JSON_Element * e=calloc(1,sizeof(JSON_Element));
                e->type=JSON_NULL;
                p->i+=4;
                return e;
            }
        }
        return parse_error("Expected JSON Element, got %c",c);
    }
}

JSON_Element * json_parse_n(const char * data,size_t len){
    parse_data p = {.i=0,.s=data,.n=len};
    return json_parse_element(&p);
}

void print_indent(size_t indentation){
    for(size_t i=0;i<indentation;i++){
        fputs("  ",stdout);
    }
}

void json_print_element(JSON_Element * elem,size_t indentation){
    switch(elem->type){
    case JSON_ARRAY:
        json_print_array(&elem->_arr,indentation);
        break;
    case JSON_OBJECT:
        json_print_object(&elem->_obj,indentation);
        break;
    case JSON_STRING:
        json_print_string(&elem->_str,indentation);
        break;
    case JSON_INTEGER:
        #if ULONG_MAX == 0xFFFFFFFFFFFFFFFFUL
        printf("%ld",elem->_int.i);
        #elif ULONG_LONG_MAX == 0xFFFFFFFFFFFFFFFFUL
        printf("%lld",elem->_int.i);
        #else
            #error "Can't print 64-bit integer"
        #endif
        break;
    case JSON_DOUBLE:
        printf("%f",elem->_double.d);
        break;
    case JSON_TRUE:
        printf("true");
        break;
    case JSON_FALSE:
        printf("false");
        break;
    case JSON_NULL:
        printf("null");
        break;
    case JSON_PARSE_ERROR:
        printf("PARSE ERROR: %s",elem->_str.str);
        break;
    }
}

static bool needs_escape(char c){
    switch(c){
    case '\a':
    case '\b':
    case '\t':
    case '\n':
    case '\v':
    case '\f':
    case '\r':
    case '\\':
    case '\"':
    case '\'':
        return true;
    default:
        return false;
    }
}

static char escape(char c){
    switch(c){
    case '\a':
        return 'a';
    case '\b':
        return 'b';
    case '\t':
        return 't';
    case '\n':
        return 'n';
    case '\v':
        return 'v';
    case '\f':
        return 'f';
    case '\r':
        return 'r';
    default:
        return c;
    }
}

static void print_quoted(const char * str){
    fputc('"',stdout);
    char c;
    while((c=*(str++))){
        if(needs_escape(c)){
            fputc('\\',stdout);
            fputc(escape(c),stdout);
        }else{
            fputc(c,stdout);
        }
    }
    fputc('"',stdout);
}

void json_print_object(JSON_Object * obj,size_t indentation){
    printf("{");
    bool first=true;
    for(uint32_t i=0;i<obj->tbl->num_buckets;i++){
        if(obj->tbl->buckets[i].size&&obj->tbl->buckets[i].arr){
            JSON_ObjectEntry * arr=obj->tbl->buckets[i].arr;
            for(uint32_t j=0;j<obj->tbl->buckets[i].size;j++){
                if(first){
                    first=false;
                    fputc('\n',stdout);
                }else{
                    fputc(',',stdout);
                    fputc('\n',stdout);
                }
                print_indent(indentation+1);
                print_quoted(arr[j].key);
                fputc(':',stdout);
                json_print_element(&arr[j].elem,indentation+1);
            }
        }
    }
    if(!first){
        fputc('\n',stdout);
        print_indent(indentation);
        fputc('}',stdout);
    }else{
        fputc('}',stdout);
    }

}

void json_print_array(JSON_Array * arr,size_t indentation){
    printf("[");
    bool first=true;
    if(arr->size&&arr->arr){
        JSON_Element * a=arr->arr;
        for(uint32_t i=0;i<arr->size;i++){
            if(first){
                first=false;
                fputc('\n',stdout);
            }else{
                fputc(',',stdout);
                fputc('\n',stdout);
            }
            print_indent(indentation+1);
            json_print_element(&a[i],indentation+1);
        }
    }
    if(!first){
        fputc('\n',stdout);
        print_indent(indentation);
        fputc(']',stdout);
    }else{
        fputc(']',stdout);
    }
}

void json_print_string(JSON_String * str,size_t indentation){
    print_quoted(str->str);
}
