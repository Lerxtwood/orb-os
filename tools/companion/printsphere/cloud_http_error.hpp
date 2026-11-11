#pragma once

#include <string>
#include "cJSON.h"

namespace printsphere {
// Return only fixed labels and a numeric API code. Never expose response text,
// which can contain account data, bearer tokens, or reflected request headers.
inline std::string cloud_http_error_summary(int status, const std::string& response) {
  std::string result = "HTTP " + std::to_string(status);
  std::string lower = response.substr(0, 8192);
  for (char& ch : lower) {
    if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
  }
  if (lower.find("cloudflare") != std::string::npos ||
      lower.find("cf-chl-") != std::string::npos ||
      lower.find("challenge-platform") != std::string::npos) {
    return result + "; Cloudflare block/challenge";
  }
  cJSON* root = cJSON_Parse(response.c_str());
  if (root == nullptr) {
    return result + (lower.find("<html") != std::string::npos ||
                             lower.find("<!doctype html") != std::string::npos
                         ? "; HTML denial" : "; non-JSON denial");
  }
  const cJSON* code = cJSON_GetObjectItemCaseSensitive(root, "code");
  if (!cJSON_IsNumber(code)) {
    code = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root, "data"), "code");
  }
  result += cJSON_IsNumber(code) ? "; API code " + std::to_string(code->valueint) : "; JSON denial";
  cJSON_Delete(root);
  if (lower.find("please login") != std::string::npos) result += "; login required";
  else if (lower.find("forbidden") != std::string::npos) result += "; resource forbidden";
  else if (lower.find("access rights") != std::string::npos) result += "; access rights denied";
  else if (lower.find("captcha") != std::string::npos) result += "; CAPTCHA required";
  return result;
}
}  // namespace printsphere
