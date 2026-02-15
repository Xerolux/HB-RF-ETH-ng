# IP Validation Bug Fix Verification

## Issue Description
The IP address validation function was incorrectly extracting the first octet from IP addresses stored in network byte order (big-endian). This caused valid IP addresses to be rejected as "Class E addresses" when their fourth octet was >= 240.

## Root Cause
In `src/validation.cpp:89`, the code was using:
```cpp
uint8_t firstOctet = (ip & 0xFF);
```

This extracts the **least significant byte**, which in network byte order is the **fourth octet** of the IP address, not the first.

## Example
For IP address `192.168.1.240`:
- Network byte order: `0xC0A801F0`
- Old code: `(ip & 0xFF)` = `0xF0` = 240 → **FALSE POSITIVE** (rejected as Class E)
- Fixed code: `(ip >> 24) & 0xFF` = `0xC0` = 192 → **CORRECT** (valid Class C)

## Fix
Changed line 89 in `src/validation.cpp` to:
```cpp
uint8_t firstOctet = (ip >> 24) & 0xFF;
```

This correctly extracts the **most significant byte**, which is the first octet in network byte order.

## Test Cases
| IP Address | First Octet | Old Code Result | Fixed Code Result |
|------------|-------------|-----------------|-------------------|
| 192.168.1.1 | 192 | VALID | VALID |
| 192.168.1.240 | 192 | **INVALID (BUG)** | VALID |
| 10.0.0.1 | 10 | VALID | VALID |
| 240.0.0.1 | 240 | INVALID | INVALID (correctly rejected) |
| 255.255.255.255 | 255 | VALID | VALID (broadcast exception) |

## Impact
This fix resolves issue #215 where users could not save network settings in firmware v2.1.6, particularly when using IP addresses with a fourth octet >= 240.
