#include "configmanager.h"
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <QRegularExpression>

QString ConfigManager::defaultPath()
{
    QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return home + "/.config/redshift.conf";
}

bool ConfigManager::exists(const QString &path)
{
    return QFile::exists(path);
}

QString ConfigManager::readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    QTextStream in(&f);
    return in.readAll();
}

bool ConfigManager::writeFile(const QString &path, const QString &content, QString *errorOut)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorOut) *errorOut = f.errorString();
        return false;
    }
    QTextStream out(&f);
    out << content;
    return true;
}

RedshiftConfig ConfigManager::load(const QString &path, bool *ok)
{
    RedshiftConfig cfg;
    QString content = readFile(path);
    if (content.isEmpty()) {
        if (ok) *ok = false;
        return cfg;
    }

    QRegularExpression re(R"(^\s*([a-zA-Z0-9\-]+)\s*=\s*(.*?)\s*$)");
    for (const QString &line : content.split('\n')) {
        if (line.trimmed().startsWith('#') || line.trimmed().isEmpty())
            continue;
        auto m = re.match(line);
        if (!m.hasMatch())
            continue;
        QString key = m.captured(1);
        QString val = m.captured(2);

        if (key == "temp-day")          cfg.tempDay = val.toInt();
        else if (key == "temp-night")   cfg.tempNight = val.toInt();
        else if (key == "lat")          cfg.lat = val.toDouble();
        else if (key == "lon")          cfg.lon = val.toDouble();
        else if (key == "adjustment-method") cfg.adjustmentMethod = val;
        else if (key == "location-provider") cfg.locationProvider = val;
        else if (key == "fade")         cfg.fade = val.toInt();
        else if (key == "transition")   cfg.transition = val.toInt();
        else if (key == "brightness-day")   cfg.brightnessDay = val.toDouble();
        else if (key == "brightness-night") cfg.brightnessNight = val.toDouble();
    }

    if (ok) *ok = true;
    return cfg;
}

bool ConfigManager::save(const QString &path,
                         const RedshiftConfig &cfg,
                         const QString &sectionName,
                         QString *errorOut)
{
    QString method = (sectionName == "general") ? "wayland" : cfg.adjustmentMethod;

    QString content =
        "[" + sectionName + "]\n" +
        "temp-day=" + QString::number(cfg.tempDay) + "\n" +
        "temp-night=" + QString::number(cfg.tempNight) + "\n" +
        "fade=" + QString::number(cfg.fade) + "\n" +
        "transition=" + QString::number(cfg.transition) + "\n" +
        //"adjustment-method=" + cfg.adjustmentMethod + "\n" +
        "adjustment-method=" + method + "\n" +
        "location-provider=" + cfg.locationProvider + "\n" +
        "brightness-day=" + QString::number(cfg.brightnessDay, 'f', 2) + "\n" +
        "brightness-night=" + QString::number(cfg.brightnessNight, 'f', 2) + "\n" +
        "\n" +
        "[manual]\n" +
        "lat=" + QString::number(cfg.lat, 'f', 5) + "\n" +
        "lon=" + QString::number(cfg.lon, 'f', 5) + "\n";

    return writeFile(path, content, errorOut);
}

bool ConfigManager::createDefault(const QString &path, const QString &sectionName, QString *errorOut)
{
    RedshiftConfig defaults;

    return save(path, defaults, sectionName, errorOut);

}

void ConfigManager::backupIfExists(const QString &path)
{
    if (!QFile::exists(path))
        return;

    QString backup = path + ".nightshiftbak";
    if (QFile::exists(backup))
        return;   // already backed up, don't overwrite

    QFile::copy(path, backup);
}
