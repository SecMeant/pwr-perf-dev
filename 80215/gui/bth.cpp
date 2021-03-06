#include <tchar.h>
#include <Winsock2.h>
#include <Ws2bth.h>
#include <BluetoothAPIs.h>
#include "bthdef.h"
#include <initguid.h>
#include <winsock2.h>
#include <ws2bth.h>
#include <strsafe.h>
#include <intsafe.h>

#include <array>
#include <cstdio>
#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <string_view>
#include <iterator>
#include <algorithm>

#include "bth.h"

#define OBEX_SIZE_HIGH(size) (size >> 4)
#define OBEX_SIZE_LOW(size) (size & 0xff)

#define SEND_ARRAY(sock, arr) send(sock, arr.data(), arr.size(), 0)

using uchar = unsigned char;

template<typename IntegralType>
constexpr auto OBEX_CONV_SIZE(IntegralType i)
{
  if constexpr(sizeof(IntegralType) == 2)
    return __builtin_bswap16(i);
  else if constexpr (sizeof(IntegralType) == 4)
    return __builtin_bswap32(i);
  else if constexpr (sizeof(IntegralType) == 8)
    return __builtin_bswap64(i);
  else
    static_assert(!sizeof(IntegralType));
}

constexpr uint8_t OBEX_PAYLOAD_PUT_CODE = 0x02;
constexpr uint16_t OBEX_MAX_PACKET_SIZE = OBEX_CONV_SIZE(static_cast<uint16_t>(0x00ff));
constexpr uint8_t OBEX_ERROR_RESP = 0xFF;

template<typename IntegralType>
auto
serialize(IntegralType i)
{
  i = OBEX_CONV_SIZE(i);
  return std::string_view((char*)&i, sizeof(IntegralType));
}

constexpr uint16_t
OBEX_PACK_SIZE(uint8_t sh, uint8_t sl)
{
  return (static_cast<uint16_t>(sh) << 4) | sl;
}

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

auto OBEX_END_OF_PUT_PAYLOAD =
  make_array<char>(0x82, 0x04, 0x06, 0x49, 0x04, 0x03);

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
  char buff[255];
  int rec_len = recv(s, buff, 255, 0);
  if (rec_len == SOCKET_ERROR || rec_len < 3) {
    printf("recv failed with error: %d\n", WSAGetLastError());
    ret.code = OBEX_ERROR_RESP;
  }

  char *retp = (char *)&ret;
  for (int i = 0; i < std::min(7, rec_len); ++i)
    retp[i] = buff[i];

  for (int i = 0; i < rec_len; ++i) {
    printf("%hhx ", buff[i]);
  }
  puts("");

  return ret;
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
    return;
  }

  resp.debug_show();
  closesocket(s);
}


// TODO reject packets that declare max_packet_size < 3
int
obex_connect(SOCKET s, ObexConnResp &resp)
{
  printf("Sending msg %lu\n", OBEX_CONNECT_PAYLOAD.size());
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

  uint16_t max_packet_size = ( static_cast<uint16_t>(resp.lenH) << 8 ) | static_cast<uint16_t>(resp.lenL);

  if (max_packet_size <= 3) {
    printf("Client declared max packet size less than 3. Aborting.");
    obex_disconnect(s);
    return 1;
  }

  resp.debug_show();
  return 0;
}

int
obex_end_of_put(SOCKET s)
{
  int send_len = SEND_ARRAY(s, OBEX_END_OF_PUT_PAYLOAD);
  if (send_len == SOCKET_ERROR) {
    printf("send failed with error: %d\n", WSAGetLastError());
    return -1;
  }

  return 0;
}


int
Obex::connect() noexcept
{
  ObexConnResp resp;
  int ret = obex_connect(this->sock, resp);

  if (ret)
    this->sock = INVALID_SOCKET;

  this->connInfo.maxPacketSize = OBEX_PACK_SIZE(resp.maxSizeH, resp.maxSizeL);
  sendBuffer.resize(this->connInfo.maxPacketSize);
  printf("Got max packet size: %hu\n", this->connInfo.maxPacketSize);
  return 0;
}

void
Obex::disconnect() noexcept
{
  if (this->sock == INVALID_SOCKET)
    return;

  obex_disconnect(this->sock);
  this->sock = INVALID_SOCKET;
}


int
Obex::put_file(std::string_view filename)
{
  namespace fs = std::experimental::filesystem;

  if (this->sock == INVALID_SOCKET) {
    puts("Tried to send file via invalid socket.");
    return 1;
  }

  std::ifstream file(filename.data());
  std::ofstream ofile("outdata.txt");
  ofile << std::hex;

  if (!file.is_open())
    return 1;

  size_t fileSize = fs::file_size(filename);
  size_t packSize = this->connInfo.maxPacketSize;
  uint16_t total_size = 6 + (filename.size()+1)*2 + 1 + 4 + 1 + 2 + fileSize;

  if (total_size > this->connInfo.maxPacketSize) {
    puts("Sending that huge files is not yet supported.\n");
    return 1;
  }

  std::stringstream ss;

  ss << static_cast<uchar>(0x82); // FINAL
  ss << serialize(total_size);
  ss << static_cast<uchar>(0x01);
  uint16_t uniFileNameSize = (filename.size()+1)*2;
  ss << serialize(static_cast<uint16_t>(uniFileNameSize+3));
  for(size_t i = 0; i < filename.size(); ++i)
    ss << static_cast<uchar>(0x00) << filename[i]; // Unicode string 0x00XX
  ss << static_cast<uchar>(0x00); // Unicode null terminator
  ss << static_cast<uchar>(0x00);
  ss << static_cast<uchar>(0xC3);
  ss << serialize(static_cast<uint32_t>(fileSize));
  ss << static_cast<uchar>(0x48);
  ss << serialize(static_cast<uint16_t>(fileSize + 3));

  std::copy(std::istream_iterator<uchar>(file),
            std::istream_iterator<uchar>(),
            std::ostream_iterator<uchar>(ss));


  // DEBUG
  //std::copy(std::istream_iterator<uchar>(ss),
  //          std::istream_iterator<uchar>(),
  //          std::ostream_iterator<uint32_t>(ofile, ", "));

  auto sent_len = SEND_ARRAY(this->sock, ss.str());

  fprintf(stderr, "Sent len: %i\n", sent_len);
  auto resp = obex_fetch_resp(this->sock);
  if (resp.code == OBEX_ERROR_RESP) {
    printf("recv failed with error: %d\n", WSAGetLastError());
    this->sock = INVALID_SOCKET;
    return 1;
  }

  resp.debug_show();
  return 0;
}

std::vector<BLUETOOTH_DEVICE_INFO>
scanDevices()
{
  std::vector<BLUETOOTH_DEVICE_INFO> res;

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

  auto port = 12;
  printf("Conneecting to device : %llx:%i\n", SockAddrBthServer.btAddr,
         port);

  SockAddrBthServer.port = port;

  if (::connect(s, (SOCKADDR *)&SockAddrBthServer,
                sizeof(SOCKADDR_BTH)) == SOCKET_ERROR) {
    printf("connect() failed with error code %d\n", WSAGetLastError());
    ::closesocket(s);
    return INVALID_SOCKET;
  }

  return s;
}

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
      printf("pair device failed, unknown error, code %lu\n", result);
      break;
  }

  return 1;
}

int
initWINAPI()
{
  WSADATA wsaData;

  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    return 1;
  }

  return 0;
}

int
test([[maybe_unsued]] int argc, [[maybe_unused]] _TCHAR *argv[])
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

  puts("Select device: ");
  uint32_t devid;
  if (scanf(" %i", &devid) != 1 || devid >= devices.size()) {
    puts("Wrong device number");
    WSACleanup();
    return 1;
  }

  auto pd = devices[devid];
  puts("Device pairing...\n");
  if (pairDevice(pd)) {
    WSACleanup();
    return 1;
  }

  auto sock = bth_connect(pd);
  Obex obex(sock);
  obex.connect();
  obex.put_file("a.txt");
  obex.disconnect();

  return 0;
}
