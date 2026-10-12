#include "mainwindow.h"

#include <QApplication>
#include<QMessageBox>
#include<QLocalServer>
#include<QLocalSocket>
#include<QProcess>
#include<QThread>
#include<QSettings>



int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName("NightShift");
    QCoreApplication::setOrganizationName("Alamahant");
    QCoreApplication::setApplicationVersion("1.0.0");


    QApplication a(argc, argv);

    a.setQuitOnLastWindowClosed(false);



    const QString serverName = "NightShift-SingleInstanceServer";
    QLocalSocket socket;
    socket.connectToServer(serverName);

    if (socket.waitForConnected(500)) {
        QMessageBox::critical(nullptr, "Application Already Running",
                              "Another instance of NightShift is already running.\n"
                              "Only one instance of the application is allowed.");
        return 1;
    }

    QLocalServer server;
    QLocalServer::removeServer(serverName);
    if (!server.listen(serverName)) {
        QMessageBox::critical(nullptr, "Error",
                              "Could not create server for single instance check: " +
                                  server.errorString());
        return 1;
    }

    for (int i = 0; i < 3; ++i) {
        QProcess::execute("pkill", {"-x", "redshift"});
        QThread::msleep(200);
    }



    bool startMin = false;
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--start-minimized") {
            startMin = true;
            break;
        }
    }
    if (!startMin) {
        QSettings s;
        startMin = s.value("mainwindow/startMinimized", false).toBool();
    }


    MainWindow w(startMin);
    if (!startMin) w.show();
    return a.exec();
}
