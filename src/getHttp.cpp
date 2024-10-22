#include <curl/curl.h>
#include <curl/easy.h>
#include "includes/Regex.hpp"

size_t appendCurlOutputToString(void *ptr, size_t size, size_t nmemb, std::string *woof) {
  // append the output from curl to a string
  woof->append(static_cast<char *>(ptr), size * nmemb);
  return size * nmemb;
}

void getHttp(cmd *inputCmd, std::string url) {
  // initialize curl object and curlcode
  CURL *meow = curl_easy_init();
  CURLcode res;
  std::string woof;

  // set the options
  curl_easy_setopt(meow, CURLOPT_URL, url.c_str());
  curl_easy_setopt(meow, CURLOPT_WRITEFUNCTION, appendCurlOutputToString);
  curl_easy_setopt(meow, CURLOPT_WRITEDATA, &woof);

  // perform the request
  if (curl_easy_perform(meow) != CURLE_OK) {
    throw("failed to perform the request\n");
  }

  inputCmd->source = woof;
  curl_easy_cleanup(meow);
}
