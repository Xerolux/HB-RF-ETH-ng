#include "secure_utils.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

static char *exact_string(const char *value)
{
    const size_t size = std::strlen(value) + 1;
    char *copy = static_cast<char *>(std::malloc(size));
    assert(copy != nullptr);
    std::memcpy(copy, value, size);
    return copy;
}

int main()
{
    char *short_value = exact_string("x");
    char *long_value = exact_string("this-is-a-longer-credential");
    char *long_copy = exact_string("this-is-a-longer-credential");

    assert(secure_strcmp(short_value, long_value) != 0);
    assert(secure_strcmp(long_value, short_value) != 0);
    assert(secure_strcmp(long_value, long_copy) == 0);
    assert(secure_strcmp("same", "same") == 0);
    assert(secure_strcmp("same", "samf") != 0);
    assert(secure_strcmp(nullptr, "value") != 0);

    std::free(short_value);
    std::free(long_value);
    std::free(long_copy);
    return 0;
}
