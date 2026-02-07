# Stability and Security Improvements

This document describes the stability, security, and memory management improvements
made to the HB-RF-ETH-ng firmware across all build variants.

## Overview of Build Variants

| Variant  | HMLGW | Analyzer | Description                              | Recommended For            |
|----------|-------|----------|------------------------------------------|----------------------------|
| standard | ❌    | ❌       | Basis-Features ohne HMLGW und Analyzer   | Maximale Stabilität        |
| hmlgw    | ✅     | ❌       | Mit HomeMatic LAN Gateway Unterstützung  | CCU/debmatic Integration   |
| analyzer | ❌    | ✅       | Mit Protocol Analyzer für Funkrahmen     | Debugging und Diagnose     |
| full     | ✅     | ✅       | Alle Features aktiviert                  | Volle Funktionalität       |

## Fixed Issues

### 1. Race Conditions in HMLGW (Critical)

**Problem**: Socket file descriptors were accessed from multiple tasks without proper synchronization, causing potential use-after-free and double-close bugs.

**Fixes**:
- Changed socket member variables to `std::atomic<int>` for thread-safe access
- Added TOCTOU (Time-of-check-to-time-of-use) protection using local socket copies
- Implemented `compare_exchange_strong` for safe socket invalidation
- Added socket validity checks before all socket operations

**Files**: `src/hmlgw.cpp`, `include/hmlgw.h`

### 2. Buffer Overflow Protection (High)

**Problem**: Frame size validation was insufficient, allowing potential buffer overflows.

**Fixes**:
- Defined `MAX_FRAME_SIZE` constant (8000 bytes) as strict upper limit
- Added `TEMP_FRAME_BUFFER_SIZE` constant for compile-time size verification
- Enhanced bounds checking in frame processing
- Added safety checks before buffer copy operations

**Files**: `src/hmlgw.cpp`

### 3. Memory Management Improvements (High)

**Problem**: Large frame allocation could exhaust heap under heavy load.

**Fixes**:
- Added heap size check before allocating large frames in Analyzer
- Improved critical threshold from 20KB to 30KB
- Added warning threshold at 50KB for early detection
- Enhanced heap monitoring with largest free block tracking

**Files**: `src/analyzer.cpp`, `src/main.cpp`

### 4. Socket Send Safety (Medium)

**Problem**: `send()` calls could block on dead connections, affecting real-time performance.

**Fixes**:
- Added `MSG_DONTWAIT` flag to all send() calls
- Implemented proper error handling for EAGAIN/EWOULDBLOCK
- Added EPIPE detection for broken connections

**Files**: `src/hmlgw.cpp`

### 5. Mutex Timeout Improvements (Medium)

**Problem**: `portMAX_DELAY` in mutex operations could cause deadlocks.

**Fixes**:
- Changed all `portMAX_DELAY` to 100ms timeout in frame handler management
- Added error logging when mutex acquisition fails
- Prevents indefinite blocking in critical paths

**Files**: `src/radiomoduleconnector.cpp`, `src/analyzer.cpp`

### 6. Atomic Variable Consistency (Low)

**Problem**: Inconsistent use of atomic operations in RawUartUdpListener.

**Fixes**:
- Used explicit memory_order_relaxed for better performance
- Ensured consistent load order for related atomics
- Improved getConnectedRemoteAddress() reliability

**Files**: `src/rawuartudplistener.cpp`

## Memory Usage by Variant

| Variant  | Flash  | RAM (typical) | Heap (free) | Notes                         |
|----------|--------|---------------|-------------|-------------------------------|
| standard | ~1.2MB | ~80KB         | ~220KB      | Maximum resources available    |
| hmlgw    | ~1.3MB | ~95KB         | ~205KB      | +HMLGW ring buffer (8KB)       |
| analyzer | ~1.4MB | ~110KB        | ~190KB      | +Analyzer buffer pool (40KB)   |
| full     | ~1.5MB | ~125KB        | ~175KB      | All features enabled           |

## Testing Recommendations

1. **Stress Testing**: Send >1000 frames/second to verify no memory leaks
2. **Connection Cycling**: Connect/disconnect HMLGW client repeatedly
3. **Long-Running Test**: Run for 24+ hours and monitor heap low water mark
4. **Race Condition Test**: Multiple CCU connections simultaneously

## Monitoring

The firmware includes enhanced heap monitoring:

```
I (12345) [HB-RF-ETH]: Heap status - Free: 180000 B, Min: 165000 B, Largest: 120000 B
I (12346) [HB-RF-ETH]: heap_monitor stack - Min watermark: 1024 words (20.0% used)
```

### Critical Thresholds:
- **30KB**: Critical - may indicate memory leak
- **50KB**: Warning - monitor closely
- **<30% free**: Consider restarting or using standard variant

## Build Configuration

All variants are defined in `platformio.ini`:

```ini
[env:hb-rf-eth-ng-standard]
  -DENABLE_HMLGW=0
  -DENABLE_ANALYZER=0

[env:hb-rf-eth-ng-hmlgw]
  -DENABLE_HMLGW=1
  -DENABLE_ANALYZER=0

[env:hb-rf-eth-ng-analyzer]
  -DENABLE_HMLGW=0
  -DENABLE_ANALYZER=1

[env:hb-rf-eth-ng-full]
  -DENABLE_HMLGW=1
  -DENABLE_ANALYZER=1
```

## Version Information

- Firmware: HB-RF-ETH-ng v2.1.3+
- ESP-IDF: v5.1
- Platform: espressif32@6.12.0
