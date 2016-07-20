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
 
#include <string>
#include <iostream>

/**
 * What is this about? For performance reasons, we want to (at least for testing)
 * switch to rapidjson.  nlohmann::json is awesome but the speed difference is 
 * non-trivial, especially for large block outputs.
 *
 * But: standard R problem, lots of defines and templating make R and rapidjson 
 * essentially incompatible.  So this wrapper aims to encapsulate RJ and avoid 
 * header conflicts.  Don't do more work than necessary.
 *
 * This came out ugly.  FIXME: elegant interface.
 * 
 * FIXME: merge "object" document, array
 * FIXME: test type before object/array operations? how expensive?
 *
 */

// fwd
class JSONDocument;

class JSONValue {
    friend class JSONDocument;

public:
    JSONValue() : p(0) {}
    JSONValue( const JSONValue &rhs ){ this->p = rhs.p; }
    ~JSONValue(){}

public:

    // serialize
    std::string toString();

    // tests
    bool is_array();
    bool is_object();
    bool is_string();
    bool is_number();
    bool is_bool();
    bool is_null();

    // has key?
    bool has(const char *key);

    // accessors by type
    operator std::string();
    operator bool();
    operator double();

    // special
    void set_null();
    void set_object();

    // get member by name
    JSONValue operator [](const char *key);

    // ? should go in array, or merge?
    size_t length();
    JSONValue operator [](size_t index);

    // steal rhs' value using RJ move semantics.
    void take( JSONValue &v );

protected:
    void *p;

};

/* document root type */
class JSONDocument : public JSONValue {
public:
    JSONDocument( const char *json = 0 );
    ~JSONDocument();

public:

    void set_array( int alloc = 0 );

    // object methods (require allocator)
    void add( const char *key, const char *value );
    void add( const char *key, bool value );
    void add( const char *key, int value );
    void add( const char *key, double value );

    JSONValue add( const char *key );
    void add( const char *key, JSONValue &obj );

    // array methods (require allocator)
    void push( bool val );
    void push( int val );
    void push( double val );
    void push( std::string &val );    
    void push( JSONValue &val );
    void push( JSONDocument &val );

public:
    void parse( const char *json );

};

