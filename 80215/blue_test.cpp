#include <tchar.h>
#include <string>
#include <iostream>
#include <vector>
#include <Winsock2.h>
#include <Ws2bth.h>
#include <BluetoothAPIs.h>
#include "bthdef.h"
#include <stdio.h>
#include <initguid.h>
#include <winsock2.h>
#include <ws2bth.h>
#include <strsafe.h>
#include <intsafe.h>

using namespace std;
const char OBEX_CONNECT_PAYLOAD[] ={ char(0x80), char(0x0), char(0x15), char(0x10), char(0x0), char(0x02), char(0x0), char(0x46), char(0x0), char(0x0e), 0x53, 0x59, 0x4e, 0x43,0x4d, 0x4c, 0x2d, 0x44, 0x4d};
const char OBEX_PUT_PAYLOAD[] ={ char(0x82), char(0x0), char(0x7), char(0x10), char(0x0), char(0x0), char(0x0)};

WSADATA wsaData;
DEFINE_GUID(g_guidServiceClass, 0xb62c4e8d, 0x62cc, 0x404b, 0xbb, 0xbf, 0xbf, 0x3e, 0x3b, 0xbb, 0x13, 0x74);

vector<BLUETOOTH_DEVICE_INFO> scanDevices()
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
    if (hbf)
    {
        do
        {
          res.push_back(bdi);
        } while (BluetoothFindNextDevice(hbf, &bdi));
        // BluetoothFindDeviceClose(hbf);
    }
    return res;
}

BOOL CALLBACK bluetoothAuthCallback(LPVOID param, PBLUETOOTH_AUTHENTICATION_CALLBACK_PARAMS params)
{
    BLUETOOTH_AUTHENTICATE_RESPONSE response;

    ::ZeroMemory(&response,sizeof(BLUETOOTH_AUTHENTICATE_RESPONSE));
    response.authMethod = params->authenticationMethod;
    response.bthAddressRemote = params->deviceInfo.Address;
    response.negativeResponse = FALSE;
    DWORD error=::BluetoothSendAuthenticationResponseEx(nullptr, &response);
    return error == ERROR_SUCCESS;
}

bool bth_connect(BLUETOOTH_DEVICE_INFO &device)
{
  auto s = ::socket (AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
  SOCKADDR_BTH SockAddrBthServer;
  
  SockAddrBthServer.addressFamily = AF_BTH;
  SockAddrBthServer.btAddr = *((BTH_ADDR*)&device.Address);
  SockAddrBthServer.serviceClassId = {};
  if (s == INVALID_SOCKET)
  {
      printf ("Socket creation failed, error %d\n", WSAGetLastError());
      WSACleanup();
      return false;
  }
  else
      printf ("socket() looks fine!\n");

  for(int i =0 ; i< 10000; i++){
    SockAddrBthServer.port = i;
    printf("Conneecting to device : %llx\n", SockAddrBthServer.btAddr );

    if (::connect (s, (SOCKADDR*)&SockAddrBthServer, sizeof(SOCKADDR_BTH)) == SOCKET_ERROR)
    {
      printf("connect() failed with error code %d\n", WSAGetLastError ());
    }else
      break;
  }
  printf("Sending msg\n");
  int send_len = send( s, OBEX_CONNECT_PAYLOAD, sizeof(OBEX_CONNECT_PAYLOAD), 0 );
  if ( send_len == SOCKET_ERROR) {
      printf("send failed with error: %d\n", WSAGetLastError());
      closesocket(s);
      WSACleanup();
      return 1;
  }

  // printf("Sending msg\n");
  // send_len = send( s, OBEX_PUT_PAYLOAD, sizeof(OBEX_CONNECT_PAYLOAD), 0 );
  // if ( send_len == SOCKET_ERROR) {
  //     printf("send failed with error: %d\n", WSAGetLastError());
  //     closesocket(s);
  //     WSACleanup();
  //     return 1;
  // }

  printf("send len %i\n", send_len);
  printf("recv\n");
  char buff[256];
  int rec_len = recv(s, buff, 256, MSG_WAITALL);
  if(  rec_len == SOCKET_ERROR) {
      printf("recv failed with error: %d\n", WSAGetLastError());
      closesocket(s);
      WSACleanup();
      return 1;
  }

  printf("recv  data %i \n", rec_len);
  for(int i =0 ; i < rec_len; i++){
    printf("%hhx ", buff[i]);
  }

  return true;  
}
void obex_connect(){

}

void pairDevice(BLUETOOTH_DEVICE_INFO &device)
{
    
    HBLUETOOTH_AUTHENTICATION_REGISTRATION hCallbackHandle = 0;
    DWORD result = BluetoothRegisterForAuthenticationEx(&device, &hCallbackHandle, (PFN_AUTHENTICATION_CALLBACK_EX)&bluetoothAuthCallback, NULL);
    if (result != ERROR_SUCCESS)
    {
        cout << "Failed to register callback" << endl;
        return;
    }
    result = BluetoothAuthenticateDeviceEx(NULL, NULL, &device, NULL, MITMProtectionNotRequired);
    
    BluetoothUnregisterAuthentication(hCallbackHandle);
    switch (result)
    {
    case ERROR_SUCCESS:
    case ERROR_NO_MORE_ITEMS:
        cout << "pair device success" << endl;
        bth_connect(device);
        break;
    case ERROR_CANCELLED:
        cout << "pair device failed, user cancelled" << endl;
        break;
    case ERROR_INVALID_PARAMETER:
        cout << "pair device failed, invalid parameter" << endl;
        break;
    default:
        cout << "pair device failed, unknown error, code " << (unsigned int)result << endl;
        break;
    }
}

int _tmain(int argc, _TCHAR *argv[])
{
  uint32_t devid;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
  {
    printf("Unable to load Winsock! Error code is %d\n", WSAGetLastError());
    return 1;
  }else{
    printf("WSAStartup() is OK, Winsock lib loaded!\n");
  }

  cout << "Scanning bluetooth devices..." << endl;
  // scan devices
  auto devices = scanDevices();
  cout << "Got " << devices.size() << " devices" << endl;

  int i =0;
  for (const auto & devices : devices)
  {
    wstring ws = devices.szName;
    cout <<(i++) <<". Device: " << string(ws.begin(), ws.end()) << endl;
  }

  cout << "Select device : ";
  cin >> devid;
  if (devid >= devices.size() )
  {
      cout << "Wrong device number\n";
      return 1;
  }

  auto pd = devices[devid];
  wstring ws = pd.szName;
  cout << "Device pairing...\n";
  pairDevice(pd);
  return 0;
}


