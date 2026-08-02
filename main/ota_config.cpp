/*
 *  ota_config.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "ota_config.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"

void configure_ota_http_client(esp_http_client_config_t& config, const char* url) {
    config.url = url;

    // Authenticate GitHub and its release-asset redirect hosts. The response
    // buffer is allocated only after the handshake and ESP-IDF's dynamic TLS
    // buffers are enabled, so certificate verification no longer competes with
    // the large JSON buffer for heap. Disabling this check would allow a
    // network attacker to impersonate the configured HTTPS service.
    config.crt_bundle_attach = esp_crt_bundle_attach;

    // Fix for Bug #235: GitHub redirects fail with keep-alive
    config.keep_alive_enable = false;

    // Fix for Bug #235: Default TX buffer (512) is too small for large HTTPS headers
    config.buffer_size_tx = 2048;

    // Enable redirection following (defensive programming)
    config.max_redirection_count = 5;
}
