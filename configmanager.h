#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QString>
#include <QStringList>

struct RedshiftConfig {
    int tempDay = 5500;
    int tempNight = 3500;
    double lat = 0.0;
    double lon = 0.0;
    QString adjustmentMethod = "randr";
    QString locationProvider = "manual";
    int fade = 1;
    int transition = 1;
    double brightnessDay = 1.0;
    double brightnessNight = 1.0;
    bool hasLatLon() const { return lat != 0.0 || lon != 0.0; }
};

class ConfigManager {
public:
    static QString defaultPath();
    static bool exists(const QString &path);
    static RedshiftConfig load(const QString &path, bool *ok = nullptr);
    //static bool save(const QString &path, const RedshiftConfig &cfg, QString *errorOut = nullptr);
    //static bool createDefault(const QString &path, QString *errorOut = nullptr);

    static bool createDefault(const QString &path, const QString &sectionName,
                              QString *errorOut = nullptr);
    static bool save(const QString &path, const RedshiftConfig &cfg,
                     const QString &sectionName, QString *errorOut = nullptr);

    static void backupIfExists(const QString &path);

private:
    static QString readFile(const QString &path);
    static bool writeFile(const QString &path, const QString &content, QString *errorOut);
};

#endif // CONFIGMANAGER_H
