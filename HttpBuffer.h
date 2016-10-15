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
	~HttpBuffer();

	void get(const char *uri);

	const char* getResponseBody() const {
		return response.memory;
	}

	const size_t getResponseLength() const {
		return response.size;
	}

private:
	struct MemoryStruct response;
};
