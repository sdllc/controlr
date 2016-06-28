
#include "JSON.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include <iostream>

using namespace rapidjson;
using namespace std;

JSONDocument::JSONDocument( const char *json ){
    //cout << "JSONDocument()" << endl;
    Document *d = new Document();
    if( json ) d->Parse( json );
    else d->SetObject();
    this->p = (void*)d;
}

JSONDocument:: ~JSONDocument(){
    //cout << "~JSONDocument()" << endl;
    if( this->p ) delete this->p;
    this->p = 0;
}

void JSONDocument::parse( const char *json ){
    Document *d = (Document*)p;
    d->Parse( json );
}

void JSONDocument::add( const char *key, const char *value ){
    Document *d = (Document*)p;
    Value str( value, d->GetAllocator());
    Value skey( key, d->GetAllocator());
    d->AddMember( skey, str, d->GetAllocator());
}

void JSONDocument::add( const char *key, bool value ){
    Document *d = (Document*)p;
    Value skey( key, d->GetAllocator());
    d->AddMember( skey, value, d->GetAllocator());
}

void JSONDocument::add( const char *key, int value ){
    Document *d = (Document*)p;
    Value skey( key, d->GetAllocator());
    d->AddMember( skey, value, d->GetAllocator());
}

void JSONDocument::add( const char *key, double value ){
    Document *d = (Document*)p;
    Value skey( key, d->GetAllocator());
    d->AddMember( skey, value, d->GetAllocator());
}

std::string JSONValue::toString(){
    Value *val = (Value*)p;
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    val->Accept(writer);
    std::string str = buffer.GetString();
    return str;
}

bool JSONValue::is_array(){
    return ((Value*)p)->IsArray();
}

bool JSONValue::is_number(){
    return ((Value*)p)->IsNumber();
}

unsigned int JSONValue::arrayLength(){
    return ((Value*)p)->Size();
}

bool JSONValue::is_object(){
    return ((Value*)p)->IsObject();
}

bool JSONValue::is_bool(){
    return ((Value*)p)->IsBool();
}

bool JSONValue::is_string(){
    return ((Value*)p)->IsString();
}

bool JSONValue::has(const char *key){
    Value *v = (Value*)p;
    if( !v->IsObject()) return false;
    return ( v->FindMember(key) != v->MemberEnd());
}

/*
std::string JSONValue::getString(const char *key){
    Value *v = (Value*)p;
    if( v->IsObject() && has( key )){
        return std::string((*v)[key].GetString());
    }
    return "";
}
*/

JSONValue::operator double(){
    Value *v = (Value*)p;
    return v->GetDouble();
}

JSONValue::operator bool(){
    Value *v = (Value*)p;
    return v->GetBool();
}

JSONValue::operator std::string(){
    Value *v = (Value*)p;
    return std::string( v->GetString());
}

/*
std::string JSONValue::stringValue(){
    Value *v = (Value*)p;
    return std::string( v->GetString());
}

double JSONValue::doubleValue(){
    Value *v = (Value*)p;
    return v->GetDouble();
}

bool JSONValue::boolValue(){
    Value *v = (Value*)p;
    return v->GetBool();
}
*/

JSONValue JSONValue :: operator [](const char *key){
    Value *v = (Value*)p;
    JSONValue jv;
    Value &target = (*v)[key];
    jv.p = (void*)&target;
    return jv;
}

/*
JSONValue JSONValue::get(const char *key){
    Value *v = (Value*)p;
    JSONValue jv;
    Value &target = (*v)[key];
    jv.p = (void*)&target;
    return jv;
}
*/

JSONValue JSONValue::arrayValue(unsigned int index){
    Value *v = (Value*)p;
    JSONValue jv;
    Value &target = (*v)[index];
    jv.p = (void*)&target;
    return jv;
}

void JSONDocument::add( const char *key, JSONValue &obj ){
    Document *d = (Document*)p;
    Value *json = (Value*)(obj.p);
    Value skey( key, d->GetAllocator());
    d->AddMember( skey, *json, d->GetAllocator());
}

JSONValue JSONDocument::add( const char *key ){
    Document *d = (Document*)p;
    Value obj;
    obj.SetObject();
    Value skey( key, d->GetAllocator());
    d->AddMember( skey, obj, d->GetAllocator());
    JSONValue jv;
    jv.p = (void*)&obj;
    return jv;    
}

void JSONDocument::clear(){
    Document *d = (Document*)p;
    d->Clear();
}

// -----------------------------------

JSONArray::JSONArray( int alloc ){
    Document *d = new Document();
    d->SetArray();
    if( alloc ) d->Reserve( alloc, d->GetAllocator());
    this->p = (void*)d;
}

JSONArray::~JSONArray(){
    if( this->p ) delete this->p;
    this->p = 0;
}

void JSONArray::push( bool val ){
    Document *d = (Document*)p;
    Value *v = (Value*)p;
    v->PushBack( val, d->GetAllocator() );
}

void JSONArray::push( int val ){
    Document *d = (Document*)p;
    Value *v = (Value*)p;
    v->PushBack( val, d->GetAllocator() );
}

void JSONArray::push( double val ){
    Document *d = (Document*)p;
    Value *v = (Value*)p;
    v->PushBack( val, d->GetAllocator() );
}

void JSONArray::push( JSONValue &val ){
    Document *d = (Document*)p;
    Value *v = (Value*)p;
    Value *target = (Value*)(val.p);
    v->PushBack( *target, d->GetAllocator());   
}

void JSONArray::push( JSONDocument &val ){
    Document *d = (Document*)p;
    Value *v = (Value*)p;
    Value *target = (Value*)(val.p);
    v->PushBack( *target, d->GetAllocator());   
}

void JSONArray::push( std::string &val ){
    Document *d = (Document*)p;
    Value *v = (Value*)p;
    Value str( val.c_str(), d->GetAllocator());
    v->PushBack( str, d->GetAllocator() );
} 

void JSONDocument::take( JSONValue &v ){

    Document *d = (Document*)p;
    Value *me = (Value*)p;
    Value *you = (Value*)v.p;

    (*me) = (*you);
//    v.p = 0;
//    (*me) = 909;
//    me->CopyFrom( *you, d->GetAllocator());

}

