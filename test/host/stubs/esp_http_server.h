#pragma once

// Host stand-in for the ESP-IDF web server handle. Only what the units under
// test need: the handle is an opaque pointer.
using httpd_handle_t = void *;
