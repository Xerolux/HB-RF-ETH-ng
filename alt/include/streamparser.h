/*
 *  streamparser.h is part of the HB-RF-ETH firmware v2.0
 *
 *  Original work Copyright 2022 Alexander Reinert
 *  https://github.com/alexreinert/HB-RF-ETH
 *
 *  Modified work Copyright 2025 Xerolux
 *  Modernized fork - Updated to ESP-IDF 5.1 and modern toolchains
 *
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 *
 *  You should have received a copy of the license along with this
 *  work.  If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#pragma once

#include <stdint.h>
#include <functional>

// Performance optimization: Place frequently accessed code in IRAM
#ifndef IRAM_ATTR
#define IRAM_ATTR __attribute__((section(".iram1")))
#endif

// Compiler hints for hot path optimization
#ifndef HOT_FUNCTION
#define HOT_FUNCTION __attribute__((hot))
#endif

typedef enum
{
    NO_DATA,
    RECEIVE_LENGTH_HIGH_BYTE,
    RECEIVE_LENGTH_LOW_BYTE,
    RECEIVE_FRAME_DATA,
    FRAME_COMPLETE
} state_t;

class StreamParser
{
private:
    // Increased buffer size for larger frames and burst handling
    unsigned char _buffer[4096];
    uint16_t _bufferPos;
    uint16_t _framePos;
    uint16_t _frameLength;
    state_t _state;
    bool _isEscaped;
    bool _decodeEscaped;
    std::function<void(unsigned char *buffer, uint16_t len)> _processor;

public:
    StreamParser(bool decodeEscaped, std::function<void(unsigned char *buffer, uint16_t len)> processor);

    // HOT_FUNCTION hint for compiler optimization on frequently called methods
    HOT_FUNCTION void append(unsigned char chr);
    HOT_FUNCTION void append(unsigned char *buffer, uint16_t len);
    void flush();

    bool getDecodeEscaped();
    void setDecodeEscaped(bool decodeEscaped);
};
