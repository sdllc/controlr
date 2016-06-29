
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

