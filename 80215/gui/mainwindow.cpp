#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "bth.h"

#include <thread>
#include <chrono>

#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow)
{
  this->ui->setupUi(this);
  connect(this->ui->ScanButton, SIGNAL(released()),this,SLOT(scan()));
  connect(this->ui->PairButton, SIGNAL(released()),this,SLOT(pairDevice()));
  connect(this->ui->PickFileButton, SIGNAL(released()),this,SLOT(pickFile()));
  connect(this->ui->SendButton, SIGNAL(released()),this,SLOT(sendFile()));
  connect(this->ui->DeviceList, SIGNAL(itemClicked(QListWidgetItem*)),this,SLOT(deviceSelected(QListWidgetItem *)));
}

MainWindow::~MainWindow()
{
  this->device.disconnect();
  delete ui;
}

void MainWindow::scan() noexcept
{
  this->disableButtons();
  this->setWindowTitle("Scanning . . .");

  this->ui->DeviceList->clear();
  this->devInfo.clear();

  for (const auto &dev : scanDevices()) {
    auto devName = QString::fromWCharArray(dev.szName);
    this->devInfo[devName] = dev;
    this->ui->DeviceList->addItem(devName);
  }

  this->setWindowTitle("Done.");
  this->enableButtons();
}

void MainWindow::pairDevice() noexcept
{
  const auto& devName = this->ui->DevNameF->text();
  if (devName == "") {
    this->setWindowTitle("Select device from list first.");
    return;
  }

  auto& dev = this->devInfo[devName];
  this->setWindowTitle("Pairing with device . . .");
  if(::pairDevice(dev.nativeInfo)) {
     this->setWindowTitle("Failed to pair with device.");
     return;
  }

  this->setWindowTitle("Paired.");
}

void MainWindow::pickFile() noexcept
{
  this->filename = QFileDialog::getOpenFileName(this);
  this->setWindowTitle(QString("Selected file %1").arg(filename));
}

void MainWindow::sendFile() noexcept
{
  const auto& devName = this->ui->DevNameF->text();
  if (devName == "") {
    this->setWindowTitle("Select device from list first.");
    return;
  }

  auto& dev = this->devInfo[devName];

  Obex obex(bth_connect(dev.nativeInfo));
  obex.connect();
  obex.put_file(this->filename.toStdString());
}

void MainWindow::deviceSelected(QListWidgetItem *item) const noexcept
{
  const auto& dev = const_cast<MainWindow*>(this)->devInfo[item->text()];

  this->ui->DevNameF->setText(item->text());
  this->ui->DevAddrF->setText(dev.addr);
  this->ui->DevPairF->setText(dev.nativeInfo.fAuthenticated ? "True" : "False");
}

void MainWindow::changeButtonsState(bool state) noexcept
{
  this->ui->PairButton->setEnabled(state);
  this->ui->ScanButton->setEnabled(state);
  this->ui->PickFileButton->setEnabled(state);
  this->ui->SendButton->setEnabled(state);
}

