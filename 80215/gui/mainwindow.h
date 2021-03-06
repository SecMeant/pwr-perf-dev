#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <Winsock2.h>
#include <Ws2bth.h>
#include <BluetoothAPIs.h>
#include "bthdef.h"

#include <QMainWindow>
#include <QListWidgetItem>
#include <QString>
#include <map>

#include "bth.h"

namespace Ui {
  class MainWindow;
}

std::vector<BLUETOOTH_DEVICE_INFO> scanDevices();
int pairDevice(BLUETOOTH_DEVICE_INFO &device);
SOCKET bth_connect(BLUETOOTH_DEVICE_INFO &device);

struct BluetoothDevInfo
{
  BLUETOOTH_DEVICE_INFO nativeInfo;
  QString name;
  QString addr;

  BluetoothDevInfo() = default;

  BluetoothDevInfo(BLUETOOTH_DEVICE_INFO nativeInfo)
    :nativeInfo(nativeInfo), name(QString::fromWCharArray(nativeInfo.szName))
  {
    QString addr = QString::number(nativeInfo.Address.rgBytes[0], 16);
    for(size_t i = 1; i < sizeof(nativeInfo.Address.rgBytes); ++i)
      addr += ":" + QString::number(nativeInfo.Address.rgBytes[i], 16);

    this->addr = addr;
  }
};

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private:
  Ui::MainWindow *ui;
  std::map<QString, BluetoothDevInfo> devInfo;
  QString filename;
  Obex device;

private slots:
  void scan() noexcept;
  void pairDevice() noexcept;
  void pickFile() noexcept;
  void sendFile() noexcept;
  void deviceSelected(QListWidgetItem *item) const noexcept;

private:
  void changeButtonsState(bool state) noexcept;

  inline void disableButtons() noexcept
  {changeButtonsState(false);}

  inline void enableButtons() noexcept
  {changeButtonsState(true);}
};

#endif // MAINWINDOW_H
