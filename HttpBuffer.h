/*
 * HttpBuffer.h
 *
 *  Created on: Oct 15, 2016
 *      Author: mark
 */

#pragma once

struct MemoryStruct {
  char *memory;
  size_t size;
};

class HttpBuffer {
public:
	HttpBuffer();
	virtual ~HttpBuffer();

	void get(const char *uri);

private:
	struct MemoryStruct response;
};
