#ifndef REDSHIFTCONTROLLER_H
#define REDSHIFTCONTROLLER_H

#include <QObject>
#include <QProcess>

class RedshiftController : public QObject
{
    Q_OBJECT
public:
    enum Mode { Auto, Manual, Stopped };

    explicit RedshiftController(QObject *parent = nullptr);
    ~RedshiftController();

    Mode mode() const { return m_mode; }
    void setConfigPath(const QString &path) { m_configPath = path; }
    QString configPath() const { return m_configPath; }

public slots:
    void startAuto();
    void stopAll();
    void applyManual(int temperatureK, int brightness);
    void restartAuto();

signals:
    void modeChanged(Mode m);
    void error(const QString &message);
    void info(const QString &message);

private:
    QProcess *m_daemon = nullptr;
    Mode m_mode = Stopped;
    QString m_configPath;

    QString findRedshift() const;
    void killDaemon();
    bool m_startingUp = false;
    void resetGamma();

public:
    void setWaylandMode(bool wayland);
    bool isWaylandMode() const { return m_waylandMode; }

private:
    bool m_waylandMode = false;

};

#endif // REDSHIFTCONTROLLER_H
