#include "bthdef.h"
#include <BluetoothAPIs.h>
#include <Winsock2.h>
#include <Ws2bth.h>
#include <initguid.h>
#include <intsafe.h>
#include <strsafe.h>
#include <tchar.h>
#include <winsock2.h>
#include <ws2bth.h>

#include <array>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include <byteswap.h>

#define OBEX_CONV_SIZE(size) __bswap_16(size)
#define OBEX_SIZE_HIGH(size) (size >> 4)
#define OBEX_SIZE_LOW(size) (size & 0xff)
constexpr uint16_t OBEX_MAX_PACKET_SIZE = OBEX_CONV_SIZE(0x00ff);
constexpr uint8_t OBEX_ERROR_RESP = 0xFF;

using namespace std;
template<typename T, typename... Ts>
auto
make_array(Ts &&... args)
{
  return std::array<T, sizeof...(args)>({ static_cast<T>(args)... });
}

auto OBEX_CONNECT_PAYLOAD = make_array<char>(
  0x80, 0x00, 0x07, 0x10, 0x00, OBEX_SIZE_HIGH(OBEX_MAX_PACKET_SIZE),
  OBEX_SIZE_LOW(OBEX_MAX_PACKET_SIZE));
auto OBEX_DISCONNECT_PAYLOAD = make_array<char>(0x81, 0x00, 0x03);

auto OBEX_CONNECT_FTP_PAYLOAD =
  make_array<char>(0xF9, 0xEC, 0x7B, 0xC4, 0x95, 0x3C, 0x11, 0xD2, 0x98,
                   0x4E, 0x52, 0x54, 0x00, 0xDC, 0x9E, 0x09);

auto OBEX_PUT_PAYLOAD = make_array<char>(0x02);

auto RFCOMM_UUID =
  make_array<char>(0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80,
                   0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB);

struct ObexConnResp
{
  uint8_t code;
  uint8_t lenH;
  uint8_t lenL;
  uint8_t ver;
  uint8_t flags;
  uint8_t maxSizeH;
  uint8_t maxSizeL;

  void
  debug_show() noexcept
  {
    printf("Obex resp: %hhx %hhx %hhx %hhx %hhx %hhx %hhx\n", code, lenH,
           lenL, ver, flags, maxSizeH, maxSizeL);
  }
};

ObexConnResp
obex_fetch_resp(SOCKET s)
{
  ObexConnResp ret;
  char buff[OBEX_MAX_PACKET_SIZE];
  int rec_len = recv(s, buff, OBEX_MAX_PACKET_SIZE, 0);
  if (rec_len == SOCKET_ERROR || rec_len < 7) {
    printf("recv failed with error: %d\n", WSAGetLastError());
    ret.code = OBEX_ERROR_RESP;
  }

  char *retp = (char *)&ret;
  for (size_t i = 0; i < 7; ++i)
    retp[i] = buff[i];

  return ret;
}

int
obex_connect(SOCKET s, ObexConnResp &resp)
{
  printf("Sending msg\n");
  int send_len = SEND_ARRAY(s, OBEX_CONNECT_PAYLOAD);
  if (send_len == SOCKET_ERROR) {
    printf("send failed with error: %d\n", WSAGetLastError());
    closesocket(s);
    return 1;
  }

  printf("send len %i\n", send_len);

  resp = obex_fetch_resp(s);
  if (resp.code == OBEX_ERROR_RESP) {
    printf("recv failed with error: %d\n", WSAGetLastError());
    closesocket(s);
    return 1;
  }

  resp.debug_show();
  return 0;
}

void
obex_disconnect(SOCKET s)
{
  printf("Sending disconnect\n");
  int send_len = SEND_ARRAY(s, OBEX_DISCONNECT_PAYLOAD);
  if (send_len == SOCKET_ERROR) {
    printf("send failed with error: %d\n", WSAGetLastError());
    closesocket(s);
    return;
  }

  printf("send len %i\n", send_len);

  auto resp = obex_fetch_resp(s);
  if (resp.code == OBEX_ERROR_RESP) {
    printf("recv failed with error: %d\n", WSAGetLastError());
    closesocket(s);
    return 1;
  }

  resp.debug_show();
  closesocket(s);
}

class Obex
{
  struct ConnInfo
  {
    uint32_t maxPacketSize;
  };

  SOCKET sock;
  ConnInfo connInfo;

public:
  Obex() noexcept : s(INVALID_SOCKET) {}

  Obex(SOCKET s) noexcept : s(INVALID_SOCKET) {}

  ~Obex() { this->disconnect(); }

  int
  connect() noexcept
  {
    int ret = obex_connect(this->sock, this->connInfo);

    if (ret)
      this->sock = INVALID_SOCKET;

    return ret;
  }

  void
  disconnect() noexcept
  {
    if (this->sock == INVALID_SOCKET)
      returm;

    obex_disconnect(this->sock);
  }
};

vector<BLUETOOTH_DEVICE_INFO>
scanDevices()
{
  vector<BLUETOOTH_DEVICE_INFO> res;

  BLUETOOTH_DEVICE_SEARCH_PARAMS bdsp;
  BLUETOOTH_DEVICE_INFO bdi;
  HBLUETOOTH_DEVICE_FIND hbf;
  ::ZeroMemory(&bdsp, sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS));
  bdsp.dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS);
  bdsp.fReturnAuthenticated = TRUE;
  bdsp.fReturnRemembered = TRUE;
  bdsp.fReturnUnknown = TRUE;
  bdsp.fReturnConnected = TRUE;
  bdsp.fIssueInquiry = TRUE;
  bdsp.cTimeoutMultiplier = 4;
  bdsp.hRadio = NULL;
  bdi.dwSize = sizeof(bdi);

  hbf = BluetoothFindFirstDevice(&bdsp, &bdi);
  if (hbf) {
    do {
      res.push_back(bdi);
    } while (BluetoothFindNextDevice(hbf, &bdi));
    // BluetoothFindDeviceClose(hbf);
  }
  return res;
}

BOOL CALLBACK
bluetoothAuthCallback(LPVOID param,
                      PBLUETOOTH_AUTHENTICATION_CALLBACK_PARAMS params)
{
  BLUETOOTH_AUTHENTICATE_RESPONSE response;

  ::ZeroMemory(&response, sizeof(BLUETOOTH_AUTHENTICATE_RESPONSE));
  response.authMethod = params->authenticationMethod;
  response.bthAddressRemote = params->deviceInfo.Address;
  response.negativeResponse = FALSE;
  DWORD error =
    ::BluetoothSendAuthenticationResponseEx(nullptr, &response);
  return error == ERROR_SUCCESS;
}

SOCKET
bth_connect(BLUETOOTH_DEVICE_INFO &device)
{
  auto s = ::socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
  SOCKADDR_BTH SockAddrBthServer;

  SockAddrBthServer.addressFamily = AF_BTH;
  SockAddrBthServer.btAddr = *((BTH_ADDR *)&device.Address);
  SockAddrBthServer.serviceClassId = {};
  if (s == INVALID_SOCKET) {
    printf("Socket creation failed, error %d\n", WSAGetLastError());
    return s;
  }

  printf("socket() looks fine!\n");

  // Scan all ports -- bthapis feature doesnt seem to work, doing it
  // manually.
  constexpr int min_port = 4;
  constexpr int max_port = 30;
  auto port = 12;
  while (port <= max_port) {
    printf("Conneecting to device : %llx:%i\n", SockAddrBthServer.btAddr,
           port);

    SockAddrBthServer.port = port;

    if (::connect(s, (SOCKADDR *)&SockAddrBthServer,
                  sizeof(SOCKADDR_BTH)) != SOCKET_ERROR)
      break;

    ++port;
    break;
  }

  if (port > max_port) {
    printf("connect() failed with error code %d\n", WSAGetLastError());
    ::closesocket(s);
    return INVALID_SOCKET;
  }

  return s;
}

#define SEND_ARRAY(sock, arr) send(sock, arr.data(), arr.size(), 0)

int
pairDevice(BLUETOOTH_DEVICE_INFO &device)
{

  HBLUETOOTH_AUTHENTICATION_REGISTRATION hCallbackHandle = 0;
  DWORD result = BluetoothRegisterForAuthenticationEx(
    &device, &hCallbackHandle,
    (PFN_AUTHENTICATION_CALLBACK_EX)&bluetoothAuthCallback, NULL);
  if (result != ERROR_SUCCESS) {
    puts("Failed to register callback");
    return 1;
  }
  result = BluetoothAuthenticateDeviceEx(NULL, NULL, &device, NULL,
                                         MITMProtectionNotRequired);

  BluetoothUnregisterAuthentication(hCallbackHandle);
  switch (result) {
    case ERROR_SUCCESS:
    case ERROR_NO_MORE_ITEMS:
      puts("pair device success");
      return 0;
    case ERROR_CANCELLED:
      puts("pair device failed, user cancelled");
      break;
    case ERROR_INVALID_PARAMETER:
      puts("pair device failed, invalid parameter");
      break;
    default:
      printf("pair device failed, unknown error, code %u\n", result);
      break;
  }

  return 1;
}

int
_tmain(int argc, _TCHAR *argv[])
{
  WSADATA wsaData;

  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    printf("Unable to load Winsock! Error code is %d\n",
           WSAGetLastError());
    return 1;
  }

  puts("WSAStartup() is OK, Winsock lib loaded!");

  puts("Scanning bluetooth devices...");
  auto devices = scanDevices();
  printf("Got %llu devices\n", devices.size());

  int i = 0;
  for (const auto &devices : devices) {
    printf("%i: %S\n", i++, devices.szName);
  }

  puts("Delect device: ");
  uint32_t devid;
  if (scanf(" %i", &devid) != 1 || devid >= devices.size()) {
    cout << "Wrong device number\n";
    WSACleanup();
    return 1;
  }

  auto pd = devices[devid];
  cout << "Device pairing...\n";
  if (pairDevice(pd)) {
    WSACleanup();
    return 1;
  }

  auto sock = bth_connect(pd);
  Obex obex(sock);
  obex.connect();
  obex.disconnect();

  return 0;
}
