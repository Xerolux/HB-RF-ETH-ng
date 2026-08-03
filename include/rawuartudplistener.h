/*
 *  rawuartudplistener.h is part of the HB-RF-ETH firmware v2.0
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

#include "lwip/opt.h"
#include "lwip/inet.h"
#include "lwip/udp.h"
#include "lwip/priv/tcpip_priv.h"
#include "esp_err.h"
#include "freertos/semphr.h"
#include <atomic>
#define _Atomic(X) std::atomic<X>
#include "radiomoduleconnector.h"

class RawUartUdpListener : FrameHandler
{
private:
    RadioModuleConnector *_radioModuleConnector;
    std::atomic<uint> _remoteAddress;
    std::atomic<ushort> _remotePort;
    std::atomic<bool> _connectionStarted;
    std::atomic<int> _counter;
    std::atomic<int> _endpointConnectionIdentifier;
    std::atomic<udp_pcb *> _pcb{NULL};
    std::atomic<QueueHandle_t> _udp_queue{NULL};
    std::atomic<TaskHandle_t> _tHandle{NULL};
    std::atomic<bool> _stopRequested{true};
    // Closes the race between a radio-frame callback which already entered
    // sendMessage() and worker-owned PCB teardown during cooperative stop.
    std::atomic<uint32_t> _activeSenders{0};
    StaticSemaphore_t _lifecycleMutexStorage = {};
    SemaphoreHandle_t _lifecycleMutex = NULL;

    bool handlePacket(pbuf *pb, ip4_addr_t addr, uint16_t port);
    void sendMessage(unsigned char command, unsigned char *buffer, size_t len);

public:
    RawUartUdpListener(RadioModuleConnector *radioModuleConnector);

    void handleFrame(unsigned char *buffer, uint16_t len);
    void handleEvent();

    ip4_addr_t getConnectedRemoteAddress();

    void start();
    esp_err_t stop();

    void _udpQueueHandler();
    bool _udpReceivePacket(pbuf *pb, const ip_addr_t *addr, uint16_t port);
};
