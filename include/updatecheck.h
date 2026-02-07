/*
 *  updatecheck.h is part of the HB-RF-ETH firmware v2.0
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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sysinfo.h"
#include "led.h"
#include "settings.h"

class UpdateCheck
{
private:
    SysInfo* _sysInfo;
    LED *_statusLED;
    Settings* _settings;
    TaskHandle_t _tHandle = NULL;   
    void _updateLatestVersion();
    char _latestVersion[33] = "n/a";

public:
    UpdateCheck(Settings* settings, SysInfo* sysInfo, LED *statusLED);
    void start();
    void stop();
    void performOnlineUpdate();

    const char* getLatestVersion();

    void _taskFunc();
};