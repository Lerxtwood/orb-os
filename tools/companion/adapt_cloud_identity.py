"""Use account profile lookup and identify PrintSphere's cloud requests."""


def adapt(destination, replace):
    client = destination / 'main/src/bambu_cloud_client.cpp'
    # Bambu Studio retrieves uidStr from the user profile after token login.
    # The MakerWorld preference endpoint can deny access to a valid token.
    replace(client,
            'constexpr char kPreferencePath[] = "/v1/design-user-service/my/preference";',
            'constexpr char kProfilePath[] = "/v1/user-service/my/profile";')
    replace(client, 'cloud_api_url(credentials_.region, kPreferencePath)',
            'cloud_api_url(credentials_.region, kProfilePath)')
    replace(client, 'Cloud MQTT identity lookup via preference API failed',
            'Cloud MQTT identity lookup via account profile API failed')
    replace(client, '''        for (const char* key : {"uid", "uidStr"}) {''',
            '''        for (const char* key : {"uidStr", "uid"}) {''')
    # Both the JSON API helper and the separate TOTP request used the same
    # legacy agent identifier. Direct unauthenticated probes returned a
    # Cloudflare 403 with it, versus the expected API 401 with this identifier.
    text = client.read_text(encoding='utf-8')
    old = 'esp_http_client_set_header(client, "User-Agent", "bambu_network_agent/01.09.05.01");'
    if text.count(old) != 2:
        raise RuntimeError('Expected two legacy cloud User-Agent headers')
    text = text.replace(old, 'esp_http_client_set_header(client, "User-Agent", '
                        '"PrintSphere/2.0 (+https://github.com/Lerxtwood/PrintSphere)");')
    client.write_text(text, encoding='utf-8')
