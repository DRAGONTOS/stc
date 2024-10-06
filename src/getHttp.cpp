#include <curl/curl.h>
#include <curl/easy.h>
#include <fstream>

size_t appendCurlOutputToString(void *ptr, size_t size, size_t nmemb, std::string *woof) {
  // append the output from curl to a string
  woof->append(static_cast<char *>(ptr), size*nmemb);
  return size*nmemb;
}

void writeHtmlFile(std::string* woof, std::string *outputfile) {
  // check for nullptrs
  if (!woof || !outputfile){
    throw("string is null\n");
  } 
  // open file and write to it
  std::ofstream meow{*outputfile};
  meow << *woof;
}

void getHttp(std::string url, std::string *outputfile) {
  //initialize curl object and curlcode
  CURL *meow = curl_easy_init();
  CURLcode res;
  std::string woof;
  // check for nullptr and if nullptr throw an exception
  if (!outputfile){
    curl_easy_cleanup(meow);
    throw("outputfile is null\n");
  }
  // set the options
  curl_easy_setopt(meow, CURLOPT_URL, url.c_str());
  curl_easy_setopt(meow, CURLOPT_WRITEFUNCTION, appendCurlOutputToString);
  curl_easy_setopt(meow, CURLOPT_WRITEDATA, &woof);
  // perform the request
  res = curl_easy_perform(meow);
  if (res != CURLE_OK){
    throw("failed to perform the request\n");
  }
  writeHtmlFile(&woof, outputfile);
  curl_easy_cleanup(meow);
}
