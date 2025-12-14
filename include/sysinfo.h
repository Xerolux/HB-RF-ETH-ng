/*
 *  sysinfo.h is part of the HB-RF-ETH firmware v2.0
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

#include <cstdint>

typedef enum {
    BOARD_TYPE_REV_1_8_PUB = 0,
    BOARD_TYPE_REV_1_8_SK = 1,
    BOARD_TYPE_REV_1_10_PUB = 2,
    BOARD_TYPE_REV_1_10_SK = 3,
    BOARD_TYPE_UNKNOWN = 255
} board_type_t;

class SysInfo {
public:
    SysInfo();
    double getCpuUsage();
    double getMemoryUsage();
    const char *getCurrentVersion();
    const char *getSerialNumber();
    board_type_t getBoardType();
    const char *getBoardRevisionString();
    double getSupplyVoltage();
    double getTemperature();
    uint64_t getUptimeSeconds();
    const char *getResetReason();
};
