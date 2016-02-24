
#ifndef __CONTROLR_H
#define  __CONTROLR_H

#include <iostream>
#include <sstream>
#include <string.h>
#include <uv.h>

#include <iostream>
#include <string>
#include <vector>

#include "3dparty/json.hpp"

#include "controlr_common.h"
#include "controlr_rinterface.h"

void processCommand( nlohmann::json &j );

class locked_ostringstream : public std::ostringstream {
public:
	locked_ostringstream(){
		uv_mutex_init( &mutex );
	}

	~locked_ostringstream(){
		uv_mutex_destroy( &mutex );
	}

public:
	void lock(){
		uv_mutex_lock( &mutex );
	}

	void unlock(){
		uv_mutex_unlock( &mutex );
	}

	void locked_give( std::string &target ){
		this->lock();
		target = this->str();
		this->str("");
		this->clear();
		this->unlock();
	}

protected:
	uv_mutex_t mutex;
};


template < class T > class locked_vector : public std::vector < T > {
public:
	locked_vector(){
		uv_mutex_init( &mutex );
	}

	~locked_vector(){
		uv_mutex_destroy( &mutex );
	}

public:
	void lock(){
		uv_mutex_lock( &mutex );
	}

	void unlock(){
		uv_mutex_unlock( &mutex );
	}

	void locked_push_back(T elem){
		this->lock();
		this->push_back(elem);
		this->unlock();
	}

	size_t locked_size(){
		size_t count;
		this->lock();
		count = this->size();
		this->unlock();
		return count;
	}

	void locked_give_all( std::vector < T > &target ){
		this->lock();
		target = (std::vector<T>)(*this);
		this->clear();
		this->unlock();	
	}

protected:
	uv_mutex_t mutex;
		
};


#endif // #ifndef __CONTROLR_H

