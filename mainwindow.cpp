#include "mainwindow.h"
#include "configmanager.h"
#include "redshiftcontroller.h"

#include <QApplication>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QRadioButton>
#include <QButtonGroup>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCloseEvent>
#include <QGroupBox>
#include<QSettings>
#include<QFile>
#include<QDir>
#include<QStandardPaths>
#include<QToolButton>

MainWindow::MainWindow(bool startMinimized, QWidget *parent)
    : QMainWindow(parent)
          , m_startMinimized(startMinimized)
{
    setWindowIcon(QIcon(":/files/nightshift.png"));

    QSettings settings;
    isdarktheme = settings.value("ui/darkTheme", false).toBool();
    m_isWayland = settings.value("ui/waylandMode", false).toBool();

    performBackup();

    m_configPath = configPathForMode(m_isWayland);


    if (!ConfigManager::exists(m_configPath)) {
        QString section = m_isWayland ? "general" : "redshift";

        QString err;
        if (!ConfigManager::createDefault(m_configPath, section,  &err)) {
            QMessageBox::warning(this, "NightShift",
                "Could not create default config: " + err);
        } else {
            m_createdDefaultConfig = true;
        }
    }

    m_modeLabel = new QLabel("Mode: Stopped", this);
    statusBar()->addPermanentWidget(m_modeLabel);

    m_redshift = new RedshiftController(this);
    m_redshift->setConfigPath(m_configPath);
    connect(m_redshift, &RedshiftController::error, this, [this](const QString &msg) {
        if (m_tray) {
            m_tray->showMessage("NightShift",
                                msg,
                                QSystemTrayIcon::Warning,
                                10000);
        }
    });
    connect(m_redshift, &RedshiftController::info, this, [this](const QString &msg) {
            statusBar()->showMessage(msg, 10000);
    });

    m_nam = new QNetworkAccessManager(this);

    buildUi();
    buildTray();

    loadConfigIntoUi();

    if (ConfigManager::exists(m_configPath)) {
        m_autoRadio->setChecked(true);
        m_manualRadio->setChecked(false);
        m_redshift->startAuto();
    } else {
        m_manualRadio->setChecked(true);
        m_autoRadio->setChecked(false);
        setManualEnabled(true);
        statusBar()->showMessage(
            "No config file. Fill in values and click Save, or use the manual slider.",
            15000);
    }
    connect(m_redshift, &RedshiftController::modeChanged,
            this, &MainWindow::updateStatusBar);

    m_darkThemeAction->setChecked(isdarktheme);
    applyTheme(isdarktheme);

    updateStatusBar();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    setWindowTitle(QApplication::applicationName());
    resize(520, 420);

    m_tabs = new QTabWidget(this);
    //m_tabs->setStyleSheet("QTabWidget { padding-top: 15px; }");
    setCentralWidget(m_tabs);

    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&Save config", this, &MainWindow::onSaveConfig);
    fileMenu->addSeparator();
    fileMenu->addAction("&Quit", this, &MainWindow::onQuit, QKeySequence::Quit);

    QMenu *modeMenu = menuBar()->addMenu("&Mode");
    m_waylandAction = modeMenu->addAction("Wayland backend");
    m_waylandAction->setCheckable(true);
    m_waylandAction->setChecked(m_isWayland);
    connect(m_waylandAction, &QAction::toggled, this, &MainWindow::onWaylandToggled);

    modeMenu->addSeparator();
    modeMenu->addAction("Auto", this, [this] {
        m_autoRadio->setChecked(true);
        onModeChanged();
    });
    modeMenu->addAction("Manual", this, [this] {
        m_manualRadio->setChecked(true);
        onModeChanged();
    });

    QMenu *settingsMenu = menuBar()->addMenu("&Settings");

    m_startMinimizedAction = new QAction("Start &minimized to tray", this);
    m_startMinimizedAction->setCheckable(true);

    m_autostartAction = new QAction("Start on &login", this);
    m_autostartAction->setCheckable(true);

    settingsMenu->addAction(m_startMinimizedAction);
    settingsMenu->addAction(m_autostartAction);

    connect(m_startMinimizedAction, &QAction::toggled, this, &MainWindow::onStartMinimizedToggled);
    connect(m_autostartAction,      &QAction::toggled, this, &MainWindow::onAutostartToggled);

    settingsMenu->addSeparator();
    m_darkThemeAction = new QAction("&Dark theme", this);
    m_darkThemeAction->setCheckable(true);
    settingsMenu->addAction(m_darkThemeAction);

    connect(m_darkThemeAction, &QAction::toggled,
            this, &MainWindow::onDarkThemeToggled);

    QMenu *helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About", this, [this] {
        QMessageBox::about(this, "NightShift",
            "A small Qt frontend for Redshift.\n\n"
            "Switches between automatic day/night mode and manual temperature control.");
    });

    auto *configTab = new QWidget;
    auto *form = new QFormLayout(configTab);

    m_dayTemp = new QSpinBox;
    m_dayTemp->setRange(1000, 25000);
    m_dayTemp->setSuffix(" K");
    form->addRow("Day temperature:", m_dayTemp);

    m_nightTemp = new QSpinBox;
    m_nightTemp->setRange(1000, 25000);
    m_nightTemp->setSuffix(" K");
    form->addRow("Night temperature:", m_nightTemp);

    m_lat = new QDoubleSpinBox;
    m_lat->setRange(-90.0, 90.0);
    m_lat->setDecimals(5);
    form->addRow("Latitude:", m_lat);

    m_lon = new QDoubleSpinBox;
    m_lon->setRange(-180.0, 180.0);
    m_lon->setDecimals(5);
    form->addRow("Longitude:", m_lon);

    m_adjMethod = new QComboBox;
    //m_adjMethod->addItems({"randr", "vidmode", "drm"});
    m_adjMethod->addItems({"randr", "vidmode", "drm", "wayland"});
    form->addRow("Adjustment method:", m_adjMethod);

    m_locProvider = new QComboBox;
    m_locProvider->addItems({"manual", "geoclue2"});
    form->addRow("Location provider:", m_locProvider);


    btn = new QToolButton(this);
    btn->setCheckable(true);
    btn->setChecked(false);
    btn->setFixedSize(18, 18);
    btn->setToolTip("Show/hide brightness controls");

    //QIcon icon;
    icon.addFile(":/files/chevron-up.svg", QSize(16, 16), QIcon::Normal, QIcon::On);
    icon.addFile(":/files/chevron-down.svg",  QSize(16, 16), QIcon::Normal, QIcon::Off);
    m_chevronLight.addFile(":/files/chevron-up-white.svg",   QSize(16, 16), QIcon::Normal, QIcon::On);
    m_chevronLight.addFile(":/files/chevron-down-white.svg", QSize(16, 16), QIcon::Normal, QIcon::Off);

    //QIcon initialIcon = isdarktheme ? m_chevronLight : icon;
    initialIcon = isdarktheme ? m_chevronLight : icon;

    btn->setIcon(initialIcon);

    connect(btn, &QToolButton::toggled, this, [this](bool on) {

        m_brightDaySlider->setVisible(on);
        m_brightDayLabel->setVisible(on);
        m_brightNightSlider->setVisible(on);
        m_brightNightLabel->setVisible(on);

        brightGroup->setVisible(on);
        dayLabel->setVisible(on);
        nightLabel->setVisible(on);

    });

    m_fade = new QCheckBox("Fade between day/night");

    auto *fadeRow = new QWidget;
    auto *fadeLayout = new QHBoxLayout(fadeRow);
    fadeLayout->setContentsMargins(0, 0, 0, 0);
    fadeLayout->addWidget(m_fade);
    fadeLayout->addStretch();        // pushes the button to the right edge
    fadeLayout->addWidget(btn);

    form->addRow("", fadeRow);

    {
    auto *row = new QWidget;
    auto *h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);

    m_brightDaySlider = new QSlider(Qt::Horizontal);
    m_brightDaySlider->setVisible(false);
    m_brightDaySlider->setRange(10, 100);
    m_brightDaySlider->setValue(100);
    m_brightDaySlider->setToolTip(
        "1.00 = system brightness · 0.10 = minimum\n"
        "Redshift can only dim, not brighten.");

    m_brightDayLabel = new QLabel("100");
    m_brightDayLabel->setVisible(false);
    m_brightDayLabel->setMinimumWidth(40);
    m_brightDayLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    h->addWidget(m_brightDaySlider);
    h->addWidget(m_brightDayLabel);
    dayLabel = new QLabel("Day brightness:");
    dayLabel->setVisible(false);
    form->addRow(dayLabel, row);

    connect(m_brightDaySlider, &QSlider::valueChanged,
            this, &MainWindow::onBrightDayChanged);
    }
    {
    auto *row = new QWidget;
    auto *h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);

    m_brightNightSlider = new QSlider(Qt::Horizontal);
    m_brightNightSlider->setVisible(false);
    m_brightNightSlider->setRange(10, 100);
    m_brightNightSlider->setValue(100);
    m_brightNightSlider->setToolTip(
        "1.00 = system brightness · 0.10 = minimum\n"
        "Redshift can only dim, not brighten.");

    m_brightNightLabel = new QLabel("100");
    m_brightNightLabel->setVisible(false);

    m_brightNightLabel->setMinimumWidth(40);
    m_brightNightLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    h->addWidget(m_brightNightSlider);
    h->addWidget(m_brightNightLabel);
    nightLabel = new QLabel("Night brightness:");
    nightLabel->setVisible(false);
    form->addRow(nightLabel, row);

    connect(m_brightNightSlider, &QSlider::valueChanged,
            this, &MainWindow::onBrightNightChanged);
    }


    auto *buttonRow = new QHBoxLayout;
    m_getLocation = new QPushButton("Get location from IP");
    m_saveConfig  = new QPushButton("Save config");
    buttonRow->addWidget(m_getLocation);
    buttonRow->addWidget(m_saveConfig);
    form->setSpacing(10);
    form->addRow(buttonRow);

    connect(m_getLocation, &QPushButton::clicked, this, &MainWindow::onGetLocation);
    connect(m_saveConfig,  &QPushButton::clicked, this, &MainWindow::onSaveConfig);

    m_tabs->addTab(configTab, "Config");

    auto *modeTab = new QWidget;
    auto *modeLayout = new QVBoxLayout(modeTab);

    auto *modeGroup = new QGroupBox("Mode");
    auto *modeRow = new QHBoxLayout(modeGroup);
    m_autoRadio   = new QRadioButton("Auto (day/night)");
    m_manualRadio = new QRadioButton("Manual");
    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->addButton(m_autoRadio,   0);
    m_modeGroup->addButton(m_manualRadio, 1);
    m_autoRadio->setChecked(true);
    modeRow->addWidget(m_autoRadio);
    modeRow->addWidget(m_manualRadio);

    connect(m_modeGroup, &QButtonGroup::idToggled, this, [this](int id, bool checked) {
        if (!checked) return;
        onModeChanged();
    });
    modeLayout->addWidget(modeGroup);

    auto *sliderGroup = new QGroupBox("Manual temperature");
    auto *sliderLayout = new QVBoxLayout(sliderGroup);
    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setRange(1000, 8500);
    m_slider->setValue(4000);
    m_slider->setEnabled(false);
    connect(m_slider, &QSlider::valueChanged, this, &MainWindow::onSliderValueChanged);
    connect(m_slider, &QSlider::sliderReleased, this, &MainWindow::onSliderReleased);

    m_sliderLabel = new QLabel("4000 K");
    sliderLayout->addWidget(m_slider);
    sliderLayout->addWidget(m_sliderLabel);
    modeLayout->addWidget(sliderGroup);

    brightGroup = new QGroupBox("Manual brightness");
    brightGroup->setVisible(false);
    auto *brightLayout = new QVBoxLayout(brightGroup);

    m_manualBrightnessSlider = new QSlider(Qt::Horizontal);
    m_manualBrightnessSlider->setRange(10, 100);
    m_manualBrightnessSlider->setValue(100);
    m_manualBrightnessSlider->setEnabled(false);
    m_manualBrightnessSlider->setToolTip(
        "1.00 = system brightness · 0.10 = minimum\n"
        "Redshift can only dim, not brighten.");

    m_manualBrightnessLabel = new QLabel("100");
    m_manualBrightnessLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_manualBrightnessLabel->setMinimumWidth(40);

    auto *brightRow = new QHBoxLayout;
    brightRow->addWidget(m_manualBrightnessSlider);
    brightRow->addWidget(m_manualBrightnessLabel);

    brightLayout->addLayout(brightRow);
    modeLayout->addWidget(brightGroup);

    connect(m_manualBrightnessSlider, &QSlider::valueChanged,
            this, &MainWindow::onManualBrightnessChanged);
    connect(m_manualBrightnessSlider, &QSlider::sliderReleased,
            this, &MainWindow::onSliderReleased);

    modeLayout->addStretch();
    m_tabs->addTab(modeTab, "Mode");

    QSettings s;
    m_startMinimizedAction->setChecked(
        s.value("mainwindow/startMinimized", false).toBool());
    m_autostartAction->setChecked(autostartFileExists());
}

void MainWindow::buildTray()
{
    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(QIcon(":/files/nightshift.png"));
    m_tray->setToolTip(QApplication::applicationName());

    auto *menu = new QMenu(this);
    menu->addAction("Show / Hide", this, [this] {
        if (isVisible()) hide(); else { show(); raise(); activateWindow(); }
    });
    menu->addSeparator();
    menu->addAction("Auto mode", this, [this] {
        m_autoRadio->setChecked(true);
        onModeChanged();
    });
    menu->addAction("Manual mode", this, [this] {
        m_manualRadio->setChecked(true);
        onModeChanged();
    });

    menu->addSeparator();
    menu->addAction(m_startMinimizedAction);
    menu->addAction(m_autostartAction);

    menu->addSeparator();
    menu->addAction("Quit", this, &MainWindow::onQuit);

    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);
    m_tray->show();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_tray && m_tray->isVisible()) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}

void MainWindow::loadConfigIntoUi()
{
    bool ok = false;
    RedshiftConfig cfg = ConfigManager::load(m_configPath, &ok);

    if (!ok) {
        statusBar()->showMessage("No config file found. Fill in values and click Save.", 10000);

        m_dayTemp->setValue(5500);
        m_dayTemp->setToolTip("Usually 1000–7000 K\n"
                              "5500 = neutral daylight");
        m_nightTemp->setValue(3500);
        m_nightTemp->setToolTip("Usually 1000–7000 K\n"
                                "3000–4000 = warm night");
        m_lat->setValue(0.0);
        m_lon->setValue(0.0);
        m_adjMethod->setCurrentText("randr");
        m_locProvider->setCurrentText("manual");
        m_fade->setChecked(true);

        m_brightDaySlider->setValue(100);
        m_brightNightSlider->setValue(100);
        m_brightDayLabel->setText("100");
        m_brightNightLabel->setText("100");

        return;
    }

    m_dayTemp->setValue(cfg.tempDay);
    m_nightTemp->setValue(cfg.tempNight);
    m_lat->setValue(cfg.lat);
    m_lon->setValue(cfg.lon);
    m_adjMethod->setCurrentText(cfg.adjustmentMethod);
    m_locProvider->setCurrentText(cfg.locationProvider);
    m_fade->setChecked(cfg.fade == 1);

    int bd = qRound(cfg.brightnessDay   * 100.0);
    int bn = qRound(cfg.brightnessNight * 100.0);

    m_brightDaySlider->setValue(bd);
    m_brightNightSlider->setValue(bn);
    m_brightDayLabel->setText(QString::number(bd));
    m_brightNightLabel->setText(QString::number(bn));

    statusBar()->showMessage("Config loaded: " + m_configPath, 5000);
}

void MainWindow::onModeChanged()
{
    if (m_modeChanging) return;
    m_modeChanging = true;

    bool manual = m_manualRadio->isChecked();
    setManualEnabled(manual);

    if (manual) {
        m_redshift->stopAll();

        bool ok = false;
        RedshiftConfig cfg = ConfigManager::load(m_configPath, &ok);

        int initTemp = (ok && cfg.tempDay > 0) ? cfg.tempDay : m_slider->value();

        int initBright = ok ? qRound(cfg.brightnessDay * 100) : 100;

        m_slider->blockSignals(true);
        m_slider->setValue(initTemp);
        m_sliderLabel->setText(QString("%1 K").arg(initTemp));
        m_slider->blockSignals(false);

        m_manualBrightnessSlider->blockSignals(true);
        m_manualBrightnessSlider->setValue(initBright);
        m_manualBrightnessLabel->setText(QString::number(initBright));
        m_manualBrightnessSlider->blockSignals(false);

        m_redshift->applyManual(initTemp, initBright);
    } else {
        m_redshift->startAuto();
    }

    updateStatusBar();
    m_modeChanging = false;
}

void MainWindow::setManualEnabled(bool enabled)
{
    m_slider->setEnabled(enabled);
    m_manualBrightnessSlider->setEnabled(enabled);
}

void MainWindow::onSliderMoved(int value)
{
    m_sliderLabel->setText(QString("%1 K").arg(value));

}


void MainWindow::onGetLocation()
{
    statusBar()->showMessage("Querying location…");
    QNetworkRequest req(QUrl("https://ipapi.co/json/"));
    req.setHeader(QNetworkRequest::UserAgentHeader, "NightShift/1.0");

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(this, "NightShift",
                "Location lookup failed: " + reply->errorString());
            statusBar()->showMessage("Location lookup failed.", 5000);
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject obj = doc.object();
        double lat = obj.value("latitude").toDouble();
        double lon = obj.value("longitude").toDouble();
        if (lat == 0.0 && lon == 0.0) {
            QMessageBox::warning(this, "NightShift", "No location in response.");
            return;
        }
        m_lat->setValue(lat);
        m_lon->setValue(lon);
        statusBar()->showMessage(QString("Location: %1, %2").arg(lat).arg(lon), 5000);
    });
}

void MainWindow::onSaveConfig()
{
    if (m_locProvider->currentText() == "manual" &&
        m_lat->value() == 0.0 && m_lon->value() == 0.0)
    {
        auto answer = QMessageBox::question(this, "NightShift",
            "Latitude and longitude are both 0.0. Save anyway?");
        if (answer != QMessageBox::Yes)
            return;
    }

    RedshiftConfig cfg;
    cfg.tempDay          = m_dayTemp->value();
    cfg.tempNight        = m_nightTemp->value();
    cfg.lat              = m_lat->value();
    cfg.lon              = m_lon->value();
    cfg.adjustmentMethod = m_adjMethod->currentText();
    cfg.locationProvider = m_locProvider->currentText();
    cfg.fade             = m_fade->isChecked() ? 1 : 0;
    cfg.brightnessDay   = m_brightDaySlider->value()   / 100.0;
    cfg.brightnessNight = m_brightNightSlider->value() / 100.0;

    QString err;
    QString section = m_isWayland ? "general" : "redshift";
    if (!ConfigManager::save(m_configPath, cfg, section, &err)) {
        QMessageBox::critical(this, "NightShift", "Failed to save: " + err);
        return;
    }

    statusBar()->showMessage("Config saved.", 5000);

    if (m_autoRadio->isChecked()) {
        m_redshift->restartAuto();
    }
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        if (isVisible()) hide();
        else { show(); raise(); activateWindow(); }
    }
}

void MainWindow::onQuit()
{
    if (m_redshift) m_redshift->stopAll();
    if (m_tray) m_tray->hide();
    qApp->quit();
}

void MainWindow::updateStatusBar()
{
    QString mode;
    switch (m_redshift->mode()) {
    case RedshiftController::Auto:   mode = "Auto";   break;
    case RedshiftController::Manual: mode = "Manual"; break;
    default:                         mode = "Stopped"; break;
    }
    m_modeLabel->setText(QString("Mode: %1").arg(mode));
}

void MainWindow::onSliderValueChanged(int value)
{
    m_sliderLabel->setText(QString("%1 K").arg(value));
}




void MainWindow::onStartMinimizedToggled(bool checked)
{
    QSettings s;
    s.setValue("mainwindow/startMinimized", checked);

    statusBar()->showMessage(
        checked ? "Will start minimized on next launch."
                : "Will start with the window visible on next launch.",
        5000);
}

void MainWindow::onAutostartToggled(bool checked)
{
    QString autostartDir = QStandardPaths::writableLocation(
                               QStandardPaths::GenericConfigLocation)
                           + "/autostart";
    QDir().mkpath(autostartDir);

    QString desktopFile = autostartDir + "/nightshift.desktop";

    if (checked) {
        QString exePath = QCoreApplication::applicationFilePath();
        QString contents =
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=NightShift\n"
            "Comment=Color temperature control\n"
            "Exec=" + exePath + " --start-minimized\n"
            "Icon=nightshift\n"
            "Terminal=false\n";

        QFile f(desktopFile);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QMessageBox::warning(this, "NightShift",
                "Could not create autostart entry: " + f.errorString());
            m_autostartAction->blockSignals(true);
            m_autostartAction->setChecked(false);
            m_autostartAction->blockSignals(false);
            return;
        }
        QTextStream out(&f);
        out << contents;

        statusBar()->showMessage("Autostart enabled.", 5000);
    } else {
        QFile::remove(desktopFile);
        statusBar()->showMessage("Autostart disabled.", 5000);
    }
}

bool MainWindow::autostartFileExists() const
{
    QString autostartDir = QStandardPaths::writableLocation(
                               QStandardPaths::GenericConfigLocation)
                           + "/autostart";
    return QFile::exists(autostartDir + "/nightshift.desktop");
}


void MainWindow::onBrightDayChanged(int value)
{
    m_brightDayLabel->setText(QString::number(value));
}

void MainWindow::onBrightNightChanged(int value)
{
    m_brightNightLabel->setText(QString::number(value));
}


void MainWindow::onManualBrightnessChanged(int value)
{
    m_manualBrightnessLabel->setText(QString::number(value));
}

void MainWindow::onSliderReleased()
{
    if (!m_slider->isEnabled())
        return;

    int temp   = m_slider->value();
    int bright = m_manualBrightnessSlider->value();

    m_redshift->applyManual(temp, bright);
}

void MainWindow::onDarkThemeToggled(bool checked)
{
    QSettings s;
    s.setValue("ui/darkTheme", checked);
    applyTheme(checked);
}


void MainWindow::applyTheme(bool dark)
{
    if (dark) {
        QFile f(":/files/darktheme.css");
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qApp->setStyleSheet(QString::fromUtf8(f.readAll()));
            btn->setIcon(QIcon(m_chevronLight));
        }
    } else {
        qApp->setStyleSheet(QString());
        btn->setIcon(QIcon(icon));

    }
}


void MainWindow::onWaylandToggled(bool checked)
{
    m_isWayland = checked;

    QSettings s;
    s.setValue("ui/waylandMode", checked);

    // Tell the controller which backend to use
    m_redshift->setWaylandMode(checked);
    m_configPath = configPathForMode(checked);
    m_redshift->setConfigPath(m_configPath);

    statusBar()->showMessage(
        checked ? "Wayland mode enabled (gammastep)."
                : "X11 mode enabled (redshift).",
        3000);

    QMessageBox::information(this, "NightShift",
        "Please restart the application for changes to take effect.");
}



QString MainWindow::configPathForMode(bool wayland) const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (dir.isEmpty())
        dir = QDir::homePath() + "/.config";

    return wayland ? dir + "/gammastep/config.ini"
                   : dir + "/redshift.conf";
}

void MainWindow::performBackup()
{
    QSettings settings;
    if (settings.value("app/backupDone", false).toBool())
        return;

    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    ConfigManager::backupIfExists(configDir + "/redshift.conf");
    ConfigManager::backupIfExists(configDir + "/gammastep/config.ini");

    settings.setValue("app/backupDone", true);
}
