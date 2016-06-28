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
 
#ifndef __CONTROLR_H
#define  __CONTROLR_H

#include <string.h>
#include <uv.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// #include "3dparty/json.hpp"

#include "controlr_rinterface.h"

/**
 * utility class
 */
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

	void locked_consume( std::string &target ){
		this->lock();
		target = this->str();
		this->str("");
		this->clear();
		this->unlock();
	}

protected:
	uv_mutex_t mutex;
};

/**
 * utility class
 */
template < class T > class deallocate_on_deref : public std::vector < T > {
public:
    deallocate_on_deref(){}
    ~deallocate_on_deref(){
        for( std::vector<T>::iterator iter = begin(); iter != end(); iter++ ){
            T ptr = *iter;
            delete ptr;
        }
        clear();
    }
};

/**
 * utility class
 */
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

	void locked_consume( std::vector < T > &target ){
		this->lock();
		target = (std::vector<T>)(*this);
		this->clear();
		this->unlock();	
	}

protected:
	uv_mutex_t mutex;
		
};

#endif // #ifndef __CONTROLR_H

