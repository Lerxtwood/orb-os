"""Identify post-login cloud failures without exposing tokens or API bodies."""
import shutil
from pathlib import Path


def adapt(destination, replace):
    header = destination / 'main/include/printsphere/bambu_cloud_client.hpp'
    client = destination / 'main/src/bambu_cloud_client.cpp'
    shutil.copy2(Path(__file__).parent / 'printsphere/cloud_http_error.hpp',
                 destination / 'main/include/printsphere/cloud_http_error.hpp')
    replace(client, '#include "printsphere/bambu_cloud_client.hpp"',
            '#include "printsphere/bambu_cloud_client.hpp"\n#include "printsphere/cloud_http_error.hpp"')
    replace(header, '  void apply_cloud_token_expired_state();',
            '''  void apply_cloud_token_expired_state(int status_code, const char* operation,
                                       const std::string& response_body);
  bool cloud_auth_rejected_ = false;''')
    replace(client, '''  access_token_.clear();
  token_expiry_us_ = 0;
  if (config_store_ != nullptr) {''', '''  access_token_.clear();
  token_expiry_us_ = 0;
  cloud_auth_rejected_ = false;
  if (config_store_ != nullptr) {''')
    replace(client, '''void BambuCloudClient::apply_cloud_token_expired_state() {
  apply_cloud_session_state(true, false, false, false, "Bambu Cloud token expired", false, true);
}''', '''void BambuCloudClient::apply_cloud_token_expired_state(int status_code,
                                                         const char* operation,
                                                         const std::string& response_body) {
  const std::string detail = std::string("Bambu Cloud ") + operation +
      (status_code == 403 ? " access denied (" : " authentication rejected (") +
      cloud_http_error_summary(status_code, response_body) + ")";
  ESP_LOGW(kTag, "%s", detail.c_str());
  cloud_auth_rejected_ = true;
  apply_cloud_session_state(true, false, false, false, detail, false, true);
}''')
    replace(client, '''    if (access_token_.empty() && !credentials_.can_password_login()) {''',
            '''    // Keep the first post-login rejection visible until the user
    // explicitly reconnects. Automatic password login would hide it with a
    // fresh code prompt and can repeatedly request verification messages.
    if (cloud_auth_rejected_) {
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
      continue;
    }

    if (access_token_.empty() && !credentials_.can_password_login()) {''')
    replace(client, '''      if (waiting_for_user_code()) {
        apply_cloud_session_state(true, false, true, auth_mode() == AuthMode::kTfaCode,
                                  auth_mode() == AuthMode::kTfaCode
                                      ? "Bambu Cloud requires 2FA code"
                                      : "Bambu Cloud verification code required",
                                  false, true);
      } else {''', '''      if (waiting_for_user_code()) {
        // The login response already published the code prompt or error.
        // Preserve that detail until another code is submitted.
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        continue;
      } else {''')
    # Replace only inside the relevant function, so upstream call-site changes
    # cannot silently assign the wrong request name to an error.
    text = client.read_text(encoding='utf-8')
    for function, operation in (
        ('ensure_cloud_mqtt_identity', 'MQTT identity lookup'),
        ('fetch_bindings', 'printer binding'),
        ('fetch_latest_preview', 'print history'),
    ):
        start = text.index('bool BambuCloudClient::' + function + '(')
        end = text.index('\n}\n', start) + 3
        body = text[start:end]
        old = 'apply_cloud_token_expired_state();'
        if body.count(old) != 1:
            raise RuntimeError('Expected one cloud auth failure in ' + function)
        body = body.replace(old, f'apply_cloud_token_expired_state(status_code, "{operation}", response_body);')
        indent = '      ' if function == 'ensure_cloud_mqtt_identity' else '    '
        invalidation = '\n'.join(indent + line for line in (
            'clear_persisted_access_token();', 'access_token_.clear();', 'token_expiry_us_ = 0;'))
        if body.count(invalidation) != 1:
            raise RuntimeError('Expected one token invalidation in ' + function)
        body = body.replace(invalidation, indent + 'if (status_code == 401) {\n' +
                            '\n'.join('  ' + line for line in invalidation.splitlines()) +
                            '\n' + indent + '}')
        text = text[:start] + body + text[end:]
    client.write_text(text, encoding='utf-8')
    replace(client, '''      normalized.find("REJECTED") != std::string::npos ||''',
            '''      normalized.find("REJECTED") != std::string::npos ||
      normalized.find("ACCESS DENIED") != std::string::npos ||''')

    # Identity lookup and binding can invalidate the token. Stop this iteration
    # before later requests send an empty bearer and overwrite the first error.
    replace(client, '''    const TickType_t now_tick = xTaskGetTickCount();
    const bool low_power = low_power_mode_.load();''', '''    if (access_token_.empty()) {
      continue;
    }

    const TickType_t now_tick = xTaskGetTickCount();
    const bool low_power = low_power_mode_.load();''')
    replace(client, '''      if (preview_fetch_enabled && preview_due && !waiting_for_live_payload) {''',
            '''      if (!access_token_.empty() && preview_fetch_enabled && preview_due &&
          !waiting_for_live_payload) {''')
