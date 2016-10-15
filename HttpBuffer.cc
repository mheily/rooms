/*
 * Copyright (C) 2016 Mark Heily <mark@heily.com>
 * Copyright (C) 1998 - 2015, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/

#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <curl/curl.h>

#include "logger.h"
#include "HttpBuffer.h"

// Limit the maximum size of a buffer
const size_t memory_max = 32000000;

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  if (mem->size > memory_max || size > memory_max) {
	  throw std::runtime_error("document too large");
  }

  mem->memory = (char*)realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
	  log_error("not enough memory (realloc returned NULL)");
	  return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

void HttpBuffer::get(const char *uri)
{
  CURL *curl_handle;
  CURLcode res;

  if (response.size > 0) {
	  throw std::logic_error("unsupported reuse of an object");
  }

#ifdef _REENTRANT
#error Not threadsafe; manipulates global state
#else
  curl_global_init(CURL_GLOBAL_ALL);
#endif

  /* init the curl session */
  curl_handle = curl_easy_init();

  /* specify URL to get */
  curl_easy_setopt(curl_handle, CURLOPT_URL, uri);

  /* send all data to this function  */
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

  /* we pass our 'response' struct to the callback function */
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&response);

  /* some servers don't like requests that are made without a user-agent
     field, so we provide one */
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

  /* get it! */
  res = curl_easy_perform(curl_handle);

  /* check for errors */
  if(res != CURLE_OK) {
	  log_error("curl_easy_perform() failed: %s", curl_easy_strerror(res));
	  throw std::runtime_error("curl request failed");
  }

  /* cleanup curl stuff */
  curl_easy_cleanup(curl_handle);

  /* we're done with libcurl, so clean it up */
  curl_global_cleanup();
}


HttpBuffer::HttpBuffer() {
	response.memory = (char*)malloc(1);  /* will be grown as needed by the realloc above */
	response.size = 0;    /* no data at this point */
}

HttpBuffer::~HttpBuffer() {
	free(response.memory);
}
