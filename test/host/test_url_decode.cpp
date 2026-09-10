#include "url_decode.h"

#include <cassert>
#include <cstring>

// Regression test for the WebSocket log-stream authentication: the browser
// percent-encodes the base64 admin token, and the firmware must decode it
// before comparing, or every browser connection is rejected with 401.

static void assert_decodes(const char *encoded, const char *expected)
{
    char buffer[128];
    assert(std::strlen(encoded) < sizeof(buffer));
    std::strcpy(buffer, encoded);
    url_decode_in_place(buffer);
    assert(std::strcmp(buffer, expected) == 0);
}

int main()
{
    // encodeURIComponent() escapes every character of a base64 token that is
    // not alphanumeric: '+', '/' and the '=' padding.
    assert_decodes("fZJX9j7DDTn2TV6Z5JX%2BtsvKXAmX1XP%2BSQVhJcZLvUc%3D",
                   "fZJX9j7DDTn2TV6Z5JX+tsvKXAmX1XP+SQVhJcZLvUc=");
    assert_decodes("a%2Fb", "a/b");
    assert_decodes("%3D%3D", "==");

    // Plain values pass through untouched.
    assert_decodes("plain-token-123", "plain-token-123");
    assert_decodes("", "");

    // Invalid or truncated escapes stay verbatim so the token comparison
    // fails closed instead of silently rewriting the value.
    assert_decodes("100%genuine", "100%genuine");
    assert_decodes("trailing%", "trailing%");
    assert_decodes("half%2", "half%2");

    // A literal '+' must survive: the token itself may contain one, and
    // encodeURIComponent() would have encoded a space as %20 instead.
    assert_decodes("a+b", "a+b");

    // Mixed content decodes only the escapes.
    assert_decodes("keep%20this%21", "keep this!");

    // NULL is tolerated.
    url_decode_in_place(nullptr);
    return 0;
}
