#include "net.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <atomic>
#include "config.h"
#include "controller_html.h"
#include "input.h"

namespace {
  AsyncWebServer server(80);
  AsyncWebSocket ws("/ws");
  std::atomic<uint8_t> clientCount{0};
  bool up = false;
  char ipStr[16] = "-";

  void onWsEvent(AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type,
                 void* arg, uint8_t* data, size_t len) {
    switch (type) {
      case WS_EVT_CONNECT:
        clientCount++;
        client->setCloseClientOnQueueFull(false);
        // Ping every 2 s so a phone that vanishes (screen lock, out of range)
        // times out and its buttons get released.
        client->keepAlivePeriod(2);
        Serial.printf("[net] pad #%u connected from %s\n", client->id(),
                      client->remoteIP().toString().c_str());
        break;
      case WS_EVT_DISCONNECT:
        clientCount--;
        Input::setSource(Input::SRC_WEB, 0);
        Serial.printf("[net] pad #%u disconnected, buttons released\n", client->id());
        break;
      case WS_EVT_DATA: {
        auto* info = static_cast<AwsFrameInfo*>(arg);
        // Message: [button mask] or [button mask, signed look delta].
        if (info->final && info->index == 0 && info->len == len && len >= 1) {
          Input::setSource(Input::SRC_WEB, data[0]);
          if (len >= 2) Input::addLook((int8_t)data[1]);
        }
        break;
      }
      default:
        break;
    }
  }
}

namespace Net {

void begin() {
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);  // power save adds tens of ms of input latency
  up = WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CONN);
  strlcpy(ipStr, WiFi.softAPIP().toString().c_str(), sizeof(ipStr));
  Serial.printf("[net] AP '%s' %s, http://%s/\n", AP_SSID, up ? "up" : "FAILED", ipStr);

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse* res =
        req->beginResponse(200, "text/html", CONTROLLER_HTML_GZ, CONTROLLER_HTML_GZ_LEN);
    res->addHeader("Content-Encoding", "gzip");
    req->send(res);
  });
  server.onNotFound([](AsyncWebServerRequest* req) { req->redirect("/"); });
  server.begin();
}

bool apUp() { return up; }
uint8_t clients() { return clientCount; }
const char* ip() { return ipStr; }

}  // namespace Net
