"""Exercise the prepared firmware's identity lookup with simulated cloud replies."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / '.pio/companion/cloud-identity-tests'

STUBS = r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include "cJSON.h"
#include "printsphere/cloud_http_error.hpp"
#define ESP_LOGW(...) ((void)0)
std::string cloud_api_url(int, const char* path) { return path; }
std::string decode_username_from_access_token(const std::string& token) {
  return token == "synthetic-jwt" ? "u_123" : "";
}
const cJSON* child_object(const cJSON* root, const char* key) {
  const cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
  return cJSON_IsObject(item) ? item : nullptr;
}
struct BambuCloudClient {
  struct { int region = 0; } credentials_;
  std::string access_token_ = "synthetic-opaque", mqtt_username_;
  int64_t token_expiry_us_ = 100;
  int response_status = 200, calls = 0, rejected_status = 0;
  bool transport_ok = true, persisted = true;
  std::string response = R"({"uid":123})";
  bool ensure_cloud_mqtt_identity();
  bool perform_json_request(const std::string& url, const char* method,
                            const std::string& body, const std::string& token,
                            int* status, std::string* result) {
    ++calls;
    assert(url == "/v1/user-service/my/profile");
    assert(std::string(method) == "GET" && body.empty());
    assert(token == access_token_ && !token.empty());
    *status = response_status; *result = response;
    return transport_ok;
  }
  void clear_persisted_access_token() { persisted = false; }
  void stop_mqtt_client() {}
  void apply_cloud_token_expired_state(int status, const char*, const std::string&) { rejected_status = status; }
};
'''

CHECKS = r'''
int main() {
  using printsphere::cloud_http_error_summary;
  assert(cloud_http_error_summary(403, "<html>Cloudflare cf-chl-test</html>") ==
         "HTTP 403; Cloudflare block/challenge");
  assert(cloud_http_error_summary(403, R"({"code":8,"error":"The resource is forbidden"})") ==
         "HTTP 403; API code 8; resource forbidden");
  assert(cloud_http_error_summary(401, R"({"data":{"code":4},"error":"Please login."})") ==
         "HTTP 401; API code 4; login required");
  assert(cloud_http_error_summary(403, R"({"error":"sensitive-token-do-not-echo"})") ==
         "HTTP 403; JSON denial");
  assert(cloud_http_error_summary(403, "<HTML>secret</HTML>") == "HTTP 403; HTML denial");
  assert(cloud_http_error_summary(403, "secret") == "HTTP 403; non-JSON denial");
  for (const char* reply : {R"({"uid":123})", R"({"uidStr":"123"})",
                            R"({"data":{"uidStr":"123"}})",
                            R"({"uid":999,"uidStr":"123"})"}) {
    BambuCloudClient client;
    client.response = reply;
    assert(client.ensure_cloud_mqtt_identity());
    assert(client.mqtt_username_ == "u_123" && client.calls == 1);
    assert(client.ensure_cloud_mqtt_identity() && client.calls == 1);
  }
  BambuCloudClient jwt;
  jwt.access_token_ = "synthetic-jwt";
  assert(jwt.ensure_cloud_mqtt_identity() && jwt.calls == 0);
  BambuCloudClient forbidden;
  forbidden.response_status = 403;
  assert(!forbidden.ensure_cloud_mqtt_identity());
  assert(forbidden.persisted && !forbidden.access_token_.empty());
  assert(forbidden.rejected_status == 403 && forbidden.mqtt_username_.empty());
  BambuCloudClient unauthorized;
  unauthorized.response_status = 401;
  assert(!unauthorized.ensure_cloud_mqtt_identity());
  assert(!unauthorized.persisted && unauthorized.access_token_.empty());
  assert(unauthorized.rejected_status == 401);
  for (const char* reply : {"invalid-json", "{}", R"({"uid":0})"}) {
    BambuCloudClient missing;
    missing.response = reply;
    assert(!missing.ensure_cloud_mqtt_identity() && missing.mqtt_username_.empty());
    assert(missing.persisted && !missing.access_token_.empty());
  }
  BambuCloudClient offline;
  offline.transport_ok = false;
  assert(!offline.ensure_cloud_mqtt_identity() && offline.persisted);
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / '.pio/companion/PrintSphere')
    parser.add_argument('--compiler', default=str(ROOT / '.pio/build-venv/Scripts/python-zig.exe'))
    parser.add_argument('--cjson', type=Path, help='Host cJSON source/include directory')
    args = parser.parse_args()
    source = (args.source / 'main/src/bambu_cloud_client.cpp').read_text(encoding='utf-8')
    start = source.index('bool BambuCloudClient::ensure_cloud_mqtt_identity() {')
    function = source[start:source.index('\n}\n', start) + 3]
    start = source.index('constexpr char kProfilePath[]')
    constant = source[start:source.index(';', start) + 1]
    WORK.mkdir(parents=True, exist_ok=True)
    harness = WORK / 'identity.cpp'
    harness.write_text(STUBS + constant + '\n' + function + CHECKS, encoding='utf-8')
    cjson = args.cjson or args.source / 'managed_components/espressif__cjson/cJSON'
    zig = 'zig' in Path(args.compiler).stem
    env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(ROOT / '.pio/zig-global-cache'),
               ZIG_LOCAL_CACHE_DIR=str(ROOT / '.pio/zig-cache'))
    obj = WORK / 'cjson.o'
    subprocess.run([args.compiler] + (['cc'] if zig else []) +
                   ['-I' + str(cjson), '-c', str(cjson / 'cJSON.c'), '-o', str(obj)],
                   check=True, env=env)
    exe = WORK / 'identity.exe'
    subprocess.run([args.compiler] + (['c++'] if zig else []) +
                   ['-std=c++17', '-UNDEBUG', '-I' + str(cjson),
                    '-I' + str(args.source / 'main/include'), str(harness), str(obj), '-o', str(exe)],
                   check=True, env=env)
    subprocess.run([str(exe)], check=True)
    print('Cloud identity regression checks passed')


if __name__ == '__main__':
    main()
