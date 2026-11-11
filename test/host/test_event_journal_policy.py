"""Exercise the real journal HTTP handler with bounded chunks and hostile text."""
from pathlib import Path
import json
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class EventJournalApiTest(unittest.TestCase):
    @unittest.skipUnless(shutil.which("g++"), "requires host C++ compiler")
    def test_streaming_escaping_disconnect_and_authentication(self):
        source = (ROOT / "main/webui.cpp").read_text(encoding="utf-8")
        begin = source.index("esp_err_t get_events_handler_func(")
        handler = source[begin:source.index("httpd_uri_t get_events_handler", begin)]
        harness = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "event_journal.h"
using esp_err_t = int;
constexpr int ESP_OK = 0, ESP_FAIL = -1;
struct httpd_req_t {
    std::string response;
    bool auth = true, ended = false;
    size_t chunks = 0, max_chunk = 0, fail_at = 0;
};
static std::vector<event_journal_entry_t> entries;
static int reads = 0;
void add_security_headers(httpd_req_t *) {}
int validate_auth(httpd_req_t *req) { return req->auth ? ESP_OK : ESP_FAIL; }
void httpd_resp_set_status(httpd_req_t *, const char *) {}
void httpd_resp_set_type(httpd_req_t *, const char *) {}
void httpd_resp_set_hdr(httpd_req_t *, const char *, const char *) {}
void httpd_resp_sendstr(httpd_req_t *req, const char *s) { req->response = s; }
esp_err_t httpd_resp_send_chunk(httpd_req_t *req, const char *s, size_t n) {
    if (++req->chunks == req->fail_at) return ESP_FAIL;
    if (n > req->max_chunk) req->max_chunk = n;
    if (!s) req->ended = true;
    else req->response.append(s, n);
    return ESP_OK;
}
size_t event_journal_count() { reads++; return entries.size(); }
bool event_journal_copy(size_t i, event_journal_entry_t *entry) {
    if (i >= entries.size()) return false;
    *entry = entries[i]; return true;
}
const char *event_journal_name(uint8_t) { return "net_witness_deferred"; }
'''
        assertions = r'''
int main() {
    httpd_req_t empty;
    assert(get_events_handler_func(&empty) == ESP_OK);
    assert(empty.response == "{\"count\":0,\"events\":[]}"); assert(empty.ended);
    entries.resize(24);
    for (auto &entry : entries) {
        entry.epoch = INT64_MAX;
        for (size_t i = 0; i < sizeof(entry.detail) - 1; i++)
            entry.detail[i] = i % 3 == 0 ? '"' : i % 3 == 1 ? '\\' : '\n';
        entry.detail[95] = '\0';
    }
    httpd_req_t req;
    assert(get_events_handler_func(&req) == ESP_OK); assert(req.ended);
    assert(req.max_chunk <= 320); assert(req.chunks == entries.size() + 3);
    std::cout << req.response;
    httpd_req_t disconnected; disconnected.fail_at = 3;
    assert(get_events_handler_func(&disconnected) == ESP_FAIL); assert(!disconnected.ended);
    httpd_req_t denied; denied.auth = false; reads = 0;
    assert(get_events_handler_func(&denied) == ESP_OK);
    assert(denied.response == "401 Not authorized"); assert(reads == 0);
}
'''
        with tempfile.TemporaryDirectory() as temp:
            cpp = Path(temp) / "journal_api.cpp"
            binary = Path(temp) / "journal_api"
            cpp.write_text(harness + handler + assertions, encoding="utf-8")
            subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I" + str(ROOT / "include"), str(cpp), "-o", str(binary)], check=True, capture_output=True)
            result = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
        payload = json.loads(result.stdout)
        self.assertEqual(payload["count"], 24)
        self.assertEqual(len(payload["events"]), 24)
        self.assertEqual(payload["events"][0]["time"], 2**63 - 1)
        expected = ''.join('"' if i % 3 == 0 else '\\' if i % 3 == 1 else '' for i in range(95))
        self.assertEqual(payload["events"][0]["detail"], expected)


if __name__ == "__main__":
    unittest.main()
