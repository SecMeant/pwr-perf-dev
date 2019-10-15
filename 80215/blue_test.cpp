#include "bthdef.h"
#include <BluetoothAPIs.h>
#include <Winsock2.h>
#include <Ws2bth.h>
#include <array>
#include <initguid.h>
#include <intsafe.h>
#include <iostream>
#include <limits>
#include <stdio.h>
#include <string>
#include <strsafe.h>
#include <tchar.h>
#include <vector>
#include <winsock2.h>
#include <ws2bth.h>

using namespace std;
std::array<char> OBEX_CONNECT_PAYLOAD{ 0x80, 0x0,  0x15, 0x10, 0x0,
                                       0x02, 0x0,  0x46, 0x0,  0x0e,
                                       0x53, 0x59, 0x4e, 0x43, 0x4d,
                                       0x4c, 0x2d, 0x44, 0x4d };

std::array<char> OBEX_CONNECT_FTP_PAYLOAD { 0xF9, 0xEC, 0x7B, 0xC4, 0x95, 0x3C,
                                   0x11, 0xD2, 0x98, 0x4E, 0x52, 0x54,
                                   0x00, 0xDC, 0x9E, 0x09 };

std::array<char> OBEX_PUT_PAYLOAD{ 0x82, 0x0, 0x7, 0x10, 0x0, 0x0, 0x0 };

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
  constexpr int min_port = 0;
  constexpr int max_port = 30;
  auto port = min_port;
  while (port <= max_port) {
    printf(
      "Conneecting to device : %llx:%i\n", SockAddrBthServer.btAddr, port);

    SockAddrBthServer.port = port;

    if (::connect(s,
                  (SOCKADDR *)&SockAddrBthServer,
                  sizeof(SOCKADDR_BTH)) != SOCKET_ERROR)
      break;

    ++port;
  }

  if (port > max_port) {
    printf("connect() failed with error code %d\n", WSAGetLastError());
    ::closesocket(s);
    return INVALID_SOCKET;
  }

  return s;
}

int
obex_connect(SOCKET s)
{
  printf("Sending msg\n");
  int send_len =
    send(s, OBEX_CONNECT_FTP_PAYLOAD, OBEX_CONNECT_FTP_PAYLOAD.size(), 0);
  if (send_len == SOCKET_ERROR) {
    printf("send failed with error: %d\n", WSAGetLastError());
    closesocket(s);
    return 1;
  }

  // printf("Sending msg\n");
  // send_len = send( s, OBEX_PUT_PAYLOAD, sizeof(OBEX_CONNECT_PAYLOAD), 0
  // ); if ( send_len == SOCKET_ERROR) {
  //     printf("send failed with error: %d\n", WSAGetLastError());
  //     closesocket(s);
  //     return 1;
  // }

  printf("send len %i\n", send_len);
  printf("recv\n");
  char buff[6];
  int rec_len = recv(s, buff, 6, MSG_WAITALL);
  if (rec_len == SOCKET_ERROR) {
    printf("recv failed with error: %d\n", WSAGetLastError());
    closesocket(s);
    return 1;
  }

  printf("recv  data %i \n", rec_len);
  for (int i = 0; i < rec_len; i++) {
    printf("%hhx ", buff[i]);
  }

  return 0;
}

int
pairDevice(BLUETOOTH_DEVICE_INFO &device)
{

  HBLUETOOTH_AUTHENTICATION_REGISTRATION hCallbackHandle = 0;
  DWORD result = BluetoothRegisterForAuthenticationEx(
    &device,
    &hCallbackHandle,
    (PFN_AUTHENTICATION_CALLBACK_EX)&bluetoothAuthCallback,
    NULL);
  if (result != ERROR_SUCCESS) {
    puts("Failed to register callback");
    return;
  }
  result = BluetoothAuthenticateDeviceEx(
    NULL, NULL, &device, NULL, MITMProtectionNotRequired);

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
    goto failed;
  }

  auto pd = devices[devid];
  cout << "Device pairing...\n";
  if (pairDevice(pd))
    goto failed;

  auto sock = bth_connect(device);
  obex_connect(sock);
  return 0;

failed:
  WSACleanup();
  return 1;
}
