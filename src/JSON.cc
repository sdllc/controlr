/*
 * controlR
 * Copyright (C) 2016 Structured Data, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included 
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
 
#include "JSON.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include <iostream>

using namespace rapidjson;
using namespace std;

JSONDocument::JSONDocument( const char *json ){
    Document *d = new Document();
    if( json ) d->Parse( json );
    else d->SetNull();
    this->p = (void*)d;
}

JSONDocument:: ~JSONDocument(){
    if( this->p ){
        Value *v = (Value*)p;
        delete v;
    }
    this->p = 0;
}

void JSONDocument::parse( const char *json ){
    Document *d = (Document*)p;
    d->Parse( json );
}

void JSONDocument::add( const char *key, const char *value ){
    Document *d = (Document*)p;
    if( !d->IsObject()) d->SetObject();
    Value str( value, d->GetAllocator());
    Value skey( key, d->GetAllocator());
    d->AddMember( skey, str, d->GetAllocator());
}

void JSONDocument::add( const char *key, bool value ){
    Document *d = (Document*)p;
    if( !d->IsObject()) d->SetObject();
    Value skey( key, d->GetAllocator());
    d->AddMember( skey, value, d->GetAllocator());
}

void JSONDocument::add( const char *key, int value ){
    Document *d = (Document*)p;
    if( !d->IsObject()) d->SetObject();
    Value skey( key, d->GetAllocator());
    d->AddMember( skey, value, d->GetAllocator());
}

void JSONDocument::add( const char *key, double value ){
    Document *d = (Document*)p;
    if( !d->IsObject()) d->SetObject();
    Value skey( key, d->GetAllocator());
    d->AddMember( skey, value, d->GetAllocator());
}

std::string JSONValue::toString(){
    Value *val = (Value*)p;
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
//    writer.SetMaxDecimalPlaces(10); // test
    val->Accept(writer);
    std::string str = buffer.GetString();
    return str;
}

void JSONDocument::set_array( int alloc ){
    Document *d = (Document*)p;
    ((Value*)p)->SetArray();
    if( alloc ) d->Reserve( alloc, d->GetAllocator());
}

void JSONValue::set_object(){
    ((Value*)p)->SetObject();
}

void JSONValue::set_null(){
    ((Value*)p)->SetNull();
}

bool JSONValue::is_null(){
    return ((Value*)p)->IsNull();
}

bool JSONValue::is_array(){
    return ((Value*)p)->IsArray();
}

bool JSONValue::is_number(){
    return ((Value*)p)->IsNumber();
}

size_t JSONValue::length(){
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

JSONValue JSONValue :: operator [](const char *key){
    Value *v = (Value*)p;
    JSONValue jv;
    Value &target = (*v)[key];
    jv.p = (void*)&target;
    return jv;
}

JSONValue JSONValue :: operator [](size_t index){
    Value *v = (Value*)p;
    JSONValue jv;
    Value &target = (*v)[index];
    jv.p = (void*)&target;
    return jv;
}

void JSONDocument::add( const char *key, JSONValue &obj ){
    Document *d = (Document*)p;
    if( !d->IsObject()) d->SetObject();
    Value *json = (Value*)(obj.p);
    Value skey( key, d->GetAllocator());
    d->AddMember( skey, *json, d->GetAllocator());
}

JSONValue JSONDocument::add( const char *key ){
    Document *d = (Document*)p;
    if( !d->IsObject()) d->SetObject();
    Value obj;
    obj.SetObject();
    Value skey( key, d->GetAllocator());
    d->AddMember( skey, obj, d->GetAllocator());
    JSONValue jv;
    jv.p = (void*)&obj;
    return jv;    
}

void JSONDocument::push( bool val ){
    Document *d = (Document*)p;
    Value *v = (Value*)p;
    v->PushBack( val, d->GetAllocator() );
}

void JSONDocument::push( int val ){
    Document *d = (Document*)p;
    Value *v = (Value*)p;
    v->PushBack( val, d->GetAllocator() );
}

void JSONDocument::push( double val ){
    Document *d = (Document*)p;
    Value *v = (Value*)p;
    v->PushBack( val, d->GetAllocator() );
}

void JSONDocument::push( JSONValue &val ){
    Document *d = (Document*)p;
    Value *v = (Value*)p;
    Value *target = (Value*)(val.p);
    v->PushBack( *target, d->GetAllocator());   
}

void JSONDocument::push( JSONDocument &val ){
    Document *d = (Document*)p;
    Value *v = (Value*)p;
    Value *target = (Value*)(val.p);
    v->PushBack( *target, d->GetAllocator());   
}

void JSONDocument::push( std::string &val ){
    Document *d = (Document*)p;
    Value *v = (Value*)p;
    Value str( val.c_str(), d->GetAllocator());
    v->PushBack( str, d->GetAllocator() );
} 

void JSONValue::take( JSONValue &v ){
    Value *me = (Value*)p;
    Value *you = (Value*)v.p;

    // rapidjson's move semantics: move data, 
    // not copy, and set src to null (empty)

    (*me) = (*you); 
}

