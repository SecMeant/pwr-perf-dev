#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "bth.h"

#include <thread>
#include <chrono>

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow)
{
  this->ui->setupUi(this);
  connect(this->ui->ScanButton, SIGNAL(released()),this,SLOT(scan()));
  connect(this->ui->DeviceList, SIGNAL(itemClicked(QListWidgetItem*)),this,SLOT(deviceSelected(QListWidgetItem *)));
}

MainWindow::~MainWindow()
{
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

void MainWindow::deviceSelected(QListWidgetItem *item) const noexcept
{
  const auto& dev = const_cast<MainWindow*>(this)->devInfo[item->text()];

  this->ui->DevNameF->setText(item->text());
  this->ui->DevAddrF->setText(dev.addr);
  this->ui->DevPairF->setText(dev.nativeInfo.fConnected ? "True" : "False");
}

void MainWindow::changeButtonsState(bool state) noexcept
{
  this->ui->PairButton->setEnabled(state);
  this->ui->ScanButton->setEnabled(state);
  this->ui->PickFileButton->setEnabled(state);
  this->ui->SendButton->setEnabled(state);
}

