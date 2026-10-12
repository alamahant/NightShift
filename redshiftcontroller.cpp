#include "redshiftcontroller.h"
#include <QStandardPaths>
#include <QFileInfo>
#include<QDir>
#include<QTimer>
#include"configmanager.h"

RedshiftController::RedshiftController(QObject *parent)
    : QObject(parent) {}

RedshiftController::~RedshiftController()
{
    killDaemon();
}

/*
QString RedshiftController::findRedshift() const
{
    const QString name = m_waylandMode ? "gammastep" : "redshift";

    QString fromPath = QStandardPaths::findExecutable(name);
    if (!fromPath.isEmpty())
        return fromPath;

    const QStringList candidates = {
        "/usr/bin/" + name,
        "/usr/local/bin/" + name,
        "/bin/" + name,
        "/opt/" + name + "/bin/" + name,
    };

    for (const QString &p : candidates) {
        if (QFileInfo::exists(p) && QFileInfo(p).isExecutable())
            return p;
    }

    //QString homeBin = QDir::homePath() + "/.local/bin/redshift";
    QString homeBin = QDir::homePath() + "/.local/bin/" + name;
    if (QFileInfo::exists(homeBin) && QFileInfo(homeBin).isExecutable())
        return homeBin;

    return QString();
}
*/

QString RedshiftController::findRedshift() const
{
    const QString name = m_waylandMode ? "gammastep" : "redshift";

    QString fromPath = QStandardPaths::findExecutable(name);
    if (!fromPath.isEmpty())
        return fromPath;

    const QStringList candidates = {
        "/usr/bin/" + name,
        "/usr/local/bin/" + name,
        "/bin/" + name,
        "/opt/" + name + "/bin/" + name,
    };
    for (const QString &p : candidates) {
        if (QFileInfo::exists(p) && QFileInfo(p).isExecutable())
            return p;
    }

    QString homeBin = QDir::homePath() + "/.local/bin/" + name;
    if (QFileInfo::exists(homeBin) && QFileInfo(homeBin).isExecutable())
        return homeBin;

    return QString();
}

void RedshiftController::killDaemon()
{
    if (m_daemon) {
        m_daemon->disconnect(this);   // no error signals on intentional kill
        m_daemon->terminate();
        if (!m_daemon->waitForFinished(2000)) {
            m_daemon->kill();
            m_daemon->waitForFinished(1000);
        }
        m_daemon->deleteLater();
        m_daemon = nullptr;
    }
    QProcess::execute("pkill", {"-x", "redshift"});
}

void RedshiftController::stopAll()
{
    killDaemon();
    m_mode = Stopped;
    emit modeChanged(m_mode);
}



void RedshiftController::startAuto()
{
    killDaemon();       // disconnect happens inside
    resetGamma();

    QString bin = findRedshift();
    if (bin.isEmpty()) {
        emit error("redshift not found in PATH.");
        return;
    }
    if (!QFileInfo::exists(m_configPath)) {
        QString err;
        QString section = m_waylandMode ? "general" : "redshift";
        //if (!ConfigManager::createDefault(m_configPath, &err)) {
        if (!ConfigManager::createDefault(m_configPath, section, &err)) {
            emit error("Could not create default config: " + err);
            return;
        }
        emit info("Created default config: " + m_configPath);
    }
    m_daemon = new QProcess(this);
    m_startingUp = true;

    connect(m_daemon, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (m_startingUp) {
            return;
        }
        QString msg;
        switch (err) {
        case QProcess::FailedToStart: msg = "redshift failed to start"; break;
        case QProcess::Crashed:       msg = "redshift crashed";         break;
        case QProcess::Timedout:      msg = "redshift timed out";       break;
        case QProcess::WriteError:
        case QProcess::ReadError:     msg = "redshift I/O error";       break;
        default:                      msg = "redshift process error";   break;
        }
        emit error(msg);
    });

    m_daemon->setProcessChannelMode(QProcess::MergedChannels);
    m_daemon->start(bin, {"-c", m_configPath});

    if (!m_daemon->waitForStarted(3000)) {
        m_startingUp = false;
        emit error("redshift did not start in time: " + m_daemon->errorString());
        killDaemon();
        return;
    }

    QTimer::singleShot(4000, this, [this]() {
        m_startingUp = false;
        if (m_daemon)
            m_daemon->disconnect(this);

        m_mode = Auto;
        emit modeChanged(m_mode);

        QProcess check;
        check.start("pgrep", {"-x", "redshift"});
        check.waitForFinished(1000);
        QString pids = QString::fromLocal8Bit(check.readAllStandardOutput()).trimmed();

        emit info(pids.isEmpty()
                  ? "Auto mode started"
                  : "Auto mode started (PID: " + pids + ")");
    });
}


void RedshiftController::restartAuto()
{
    if (m_mode == Auto)
        startAuto();
}


void RedshiftController::applyManual(int temperatureK, int brightness)
{
    if (m_daemon) {
        killDaemon();
    }

    QString bin = findRedshift();
    if (bin.isEmpty()) {
        emit error("redshift not found in PATH.");
        return;
    }

    double b = brightness / 100.0;

    QProcess::startDetached(bin, {
        "-O", QString::number(temperatureK),
        "-b", QString::number(b, 'f', 2),
        "-P"
    });

    m_mode = Manual;
    emit modeChanged(m_mode);
    emit info(QString("Manual: %1K, brightness %2")
                  .arg(temperatureK)
                  .arg(b, 0, 'f', 2));
}

void RedshiftController::resetGamma()
{
    QString bin = findRedshift();
    if (bin.isEmpty()) return;

    QProcess::execute(bin, {"-x"});
}


void RedshiftController::setWaylandMode(bool wayland)
{
    m_waylandMode = wayland;
}
