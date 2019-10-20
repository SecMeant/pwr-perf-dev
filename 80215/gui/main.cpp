#include "mainwindow.h"
#include <QApplication>

int initWINAPI();

int main(int argc, char *argv[])
{
  if(initWINAPI())
    return 1;

  QApplication a(argc, argv);
  MainWindow w;
  w.show();

  return a.exec();
}
