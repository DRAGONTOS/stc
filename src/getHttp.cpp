#include <cstdio>
#include <curl/curl.h>
#include <curl/easy.h>
#include <stdexcept>
#include <string>
#include "includes/Regex.hpp"

size_t writedatatofile(void *ptr, size_t size, size_t nmemb, FILE *data) {
    return fwrite(ptr, size, nmemb, data);
}

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

void downloadFile(const std::string& url, const std::string& outputPath) {
    CURL *meow = curl_easy_init();
    if (!meow) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    FILE* file = fopen(outputPath.c_str(), "wb");
    if (!file) {
        curl_easy_cleanup(meow);
        throw std::runtime_error("Failed to open output file: " + outputPath);
    }

    // Set the options
    curl_easy_setopt(meow, CURLOPT_URL, url.c_str());
    curl_easy_setopt(meow, CURLOPT_WRITEFUNCTION, writedatatofile);
    curl_easy_setopt(meow, CURLOPT_WRITEDATA, file);  // Fixed: pass file directly, not &file

    // Perform the request
    CURLcode res = curl_easy_perform(meow);
    fclose(file);  // Close file regardless of success

    if (res != CURLE_OK) {
        curl_easy_cleanup(meow);
        throw std::runtime_error("Download failed: " + std::string(curl_easy_strerror(res)));
    }

    curl_easy_cleanup(meow);
}

