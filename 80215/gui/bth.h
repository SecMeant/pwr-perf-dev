#pragma once

std::vector<BLUETOOTH_DEVICE_INFO> scanDevices();
int initWINAPI();

class Obex
{
  struct ConnInfo
  {
    uint16_t maxPacketSize;
  };

  SOCKET sock;
  ConnInfo connInfo;
  std::vector<char> sendBuffer;

public:
  Obex() noexcept : sock(INVALID_SOCKET) {}

  Obex(const Obex &other) = delete;
  Obex& operator=(const Obex &other) = delete;

  Obex(Obex &&other)
  :sock(other.sock), connInfo(other.connInfo),
  sendBuffer(std::move(other.sendBuffer))
  {
    other.sock = INVALID_SOCKET;
  }

  Obex& operator=(Obex &&other)
  {
    if (this == &other)
      return *this;

    sock = other.sock;
    connInfo = other.connInfo;
    sendBuffer = std::move(other.sendBuffer);

    other.sock = INVALID_SOCKET;

    return *this;
  }

  Obex(SOCKET s) noexcept : sock(s) {}

  ~Obex() { this->disconnect(); }

  int
  connect() noexcept;

  void
  disconnect() noexcept;

  int
  put_file(std::string_view filename);
};
