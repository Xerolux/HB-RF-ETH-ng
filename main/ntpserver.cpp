/*
 *  ntpserver.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "ntpserver.h"
#include <string.h>
#include "esp_log.h"
#include "udphelper.h"

static const char *TAG = "NtpServer";

void _ntp_udpQueueHandlerTask(void *parameter)
{
    ((NtpServer *)parameter)->_udpQueueHandler();
}

void _ntp_udpReceivePaket(void *arg, udp_pcb *pcb, pbuf *pb, const ip_addr_t *addr, uint16_t port)
{
    if (pb != NULL && !((NtpServer *)arg)->_udpReceivePacket(pb, addr, port))
    {
        pbuf_free(pb);
    }
}

NtpServer::NtpServer(SystemClock *clk) : _clk(clk), _pcb(NULL), _udp_queue(NULL), _tHandle(NULL)
{
}

inline tstamp convertToNtp(struct timeval *tv)
{
    tv->tv_sec = htonl(tv->tv_sec + 2208988800L);
    tv->tv_usec = htonl(tv->tv_usec / 1e6 * 4294967296.);

    return (unsigned long long)tv->tv_sec + (((unsigned long long)tv->tv_usec) << 32);
}

void NtpServer::handlePacket(pbuf *pb, ip4_addr_t addr, uint16_t port)
{
    struct timeval tv = _clk->getTime();
    tstamp recv = convertToNtp(&tv);

    struct timeval lastSync = _clk->getLastSyncTime();
    if (lastSync.tv_sec < 1577836800l) // 2020-01-01 00:00:00 GMT
    {
        ESP_LOGE(TAG, "Ignoring ntp request because local time is not set");
        return;
    }

    if (pb->tot_len != sizeof(ntp_packet_t))
    {
        ESP_LOGE(TAG, "Ignoring packet with bad length %d (should be %d)", pb->tot_len, sizeof(ntp_packet_t));
        return;
    }

    ntp_packet_t ntp;
    if (pbuf_copy_partial(pb, &ntp, sizeof(ntp), 0) != sizeof(ntp)) {
        ESP_LOGE(TAG, "Could not linearize NTP request");
        return;
    }

    pbuf *resp_pb = pbuf_alloc_reference(&ntp, sizeof(ntp_packet_t), PBUF_REF);
    if (!resp_pb) {
        ESP_LOGE(TAG, "Failed to allocate response pbuf");
        return;
    }

    ip_addr_t resp_addr;
    resp_addr.type = IPADDR_TYPE_V4;
    resp_addr.u_addr.ip4 = addr;

    ntp.flags = 4 << 3 | 4; // version 4, mode server
    ntp.stratum = 15;       // Prefer other ntp server over us
    ntp.poll = 8;
    ntp.precision = 0;               // precision 2^0=1sec because of RTC values;
    ntp.delay = htonl(1 << 16);      // 1sec
    ntp.dispersion = htonl(1 << 16); // 1sec
    memset(ntp.ref_id, 0, sizeof(ntp.ref_id));
    ntp.orig_time = ntp.trns_time;
    ntp.recv_time = recv;
    ntp.ref_time = convertToNtp(&lastSync);
    tv = _clk->getTime();
    ntp.trns_time = convertToNtp(&tv);

    _udp_sendto(_pcb, resp_pb, &resp_addr, port);
    pbuf_free(resp_pb);
}

void NtpServer::start()
{
    if (_tHandle || _pcb || _udp_queue) return;

    _udp_queue = xQueueCreate(32, sizeof(udp_event_t *));
    if (!_udp_queue) {
        ESP_LOGE(TAG, "Failed to create NTP queue");
        return;
    }
    _pcb = _udp_new();
    if (!_pcb || _udp_bind(_pcb, IP4_ADDR_ANY, 123) != ERR_OK) {
        ESP_LOGE(TAG, "Failed to create/bind NTP server on port 123");
        _udp_remove(_pcb);
        _pcb = NULL;
        vQueueDelete(_udp_queue);
        _udp_queue = NULL;
        return;
    }
    _udp_recv(_pcb, &_ntp_udpReceivePaket, (void *)this);

    if (xTaskCreate(_ntp_udpQueueHandlerTask, "NTPServer_UDP_QueueHandler",
                    4096, this, 10, &_tHandle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create NTP server task");
        _udp_recv(_pcb, NULL, NULL);
        _udp_remove(_pcb);
        _pcb = NULL;
        vQueueDelete(_udp_queue);
        _udp_queue = NULL;
    }
}

void NtpServer::stop()
{
    if (_pcb) {
        _udp_recv(_pcb, NULL, NULL);
        _udp_disconnect(_pcb);
        _udp_remove(_pcb);
        _pcb = NULL;
    }
    if (_tHandle) {
        vTaskDelete(_tHandle);
        _tHandle = NULL;
    }
    if (_udp_queue) {
        udp_event_t *event = NULL;
        while (xQueueReceive(_udp_queue, &event, 0) == pdTRUE) {
            pbuf_free(event->pb);
            free(event);
        }
        vQueueDelete(_udp_queue);
        _udp_queue = NULL;
    }
}

void NtpServer::_udpQueueHandler()
{
    udp_event_t *event = NULL;

    for (;;)
    {
        if (xQueueReceive(_udp_queue, &event, portMAX_DELAY) == pdTRUE)
        {
            handlePacket(event->pb, event->addr, event->port);
            pbuf_free(event->pb);
            free(event);
        }
    }
}

bool NtpServer::_udpReceivePacket(pbuf *pb, const ip_addr_t *addr, uint16_t port)
{
    udp_event_t *e = (udp_event_t *)malloc(sizeof(udp_event_t));
    if (!e)
    {
        return false;
    }

    e->pb = pb;

    // Use the source address and port provided directly by lwIP in the callback
    // instead of reading raw pbuf header memory (which is unsafe and fragile).
    e->addr.addr = addr->u_addr.ip4.addr;
    e->port = port;

    // Runs in the lwIP tcpip thread - never block here. If the queue is full
    // (e.g. NTP request flood), drop the packet instead of freezing the whole
    // network stack including the latency-critical raw-uart UDP bridge.
    if (xQueueSend(_udp_queue, &e, 0) != pdPASS)
    {
        free((void *)(e));
        return false;
    }
    return true;
}
