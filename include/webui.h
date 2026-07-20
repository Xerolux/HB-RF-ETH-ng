/*
 *  webui.h is part of the HB-RF-ETH firmware v2.0
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
#include <stdint.h>
#include "settings.h"
#include "led.h"
#include "updatecheck.h"
#include "radiomoduleconnector.h"
#include "radiomoduledetector.h"
#include "rawuartudplistener.h"
#include "ethernet.h"
#include "esp_http_server.h"

class WebUI
{
private:
    httpd_handle_t _httpd_handle;

public:
    WebUI(Settings *settings, LED *statusLED, SysInfo *sysInfo, UpdateCheck *updateCheck, Ethernet *ethernet, RawUartUdpListener *rawUartUdpListener, RadioModuleConnector *radioModuleConnector, RadioModuleDetector *radioModuleDetector);
    void start();
    void stop();
};

/**
 * Registration shim used by webui.cpp.
 *
 * It transparently replaces only the static New Design asset handlers with
 * SPIFFS-aware handlers and adds the separate WWW update API. Every other route
 * is forwarded unchanged to ESP-IDF's httpd_register_uri_handler().
 */
esp_err_t hb_webui_register_uri_handler(httpd_handle_t server,
                                        const httpd_uri_t *uri_handler);

// Keep the existing large webui.cpp untouched while allowing the separate WWW
// layer to intercept its route registrations. Other translation units can opt
// out by defining HB_WEBUI_BYPASS_REGISTER_WRAPPER before including this file.
#ifndef HB_WEBUI_BYPASS_REGISTER_WRAPPER
#define httpd_register_uri_handler hb_webui_register_uri_handler
#endif
