/*
 *  sysinfo.h is part of the HB-RF-ETH firmware v2.0
 *
 *  Original work Copyright 2022 Alexander Reinert
 *  https://github.com/alexreinert/HB-RF-ETH
 *
 *  Modified work Copyright 2025 Xerolux
 *  Modernized fork - Updated to ESP-IDF 6.0 and modern toolchains
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

#include <cstddef>
#include <cstdint>
#include <string>

typedef enum
{
    BOARD_TYPE_REV_1_8_PUB = 0,
    BOARD_TYPE_REV_1_8_SK = 1,
    BOARD_TYPE_REV_1_10_PUB = 2,
    BOARD_TYPE_REV_1_10_SK = 3,
    BOARD_TYPE_UNKNOWN = 255
} board_type_t;

class SysInfo
{
public:
    SysInfo();
    double getCpuUsage();
    double getMemoryUsage();
    const char* getCurrentVersion();
    const char *getSerialNumber();
    board_type_t getBoardType();
    // Returns by value (not a shared static buffer): this is called
    // concurrently from the WebUI HTTP task and the MQTT publish task, and a
    // shared static char[] previously let one task's write tear a
    // concurrently-running task's read (data race), corrupting whichever
    // caller lost the race.
    std::string getBoardRevisionString();
    // Raw ADC voltage (in millivolts) from the board-revision sense pin, captured
    // during detectBoard(). Useful for support / diagnostics when the revision
    // resolves to "Unknown" — exposes what the divider actually produced.
    uint32_t getBoardSenseVoltage();
    uint64_t getUptimeSeconds();
    const char* getResetReason();

    // Dynamic, danger-first task diagnostics; concurrent callers own their
    // snapshot. The writer path uses one malloc-checked workspace, no complete
    // output-string or JSON-escape allocation. Sink data is valid only during
    // the call. Returning false from the sink aborts output and frees scratch.
    using TaskStackWriter = bool (*)(void *context, const char *text, size_t length);
    bool writeTaskStackInfo(TaskStackWriter writer, void *context);
    // MQTT compatibility wrapper; HTTP uses the writer directly.
    std::string getTaskStackInfo();
};
