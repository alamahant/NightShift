#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QNetworkAccessManager>
#include<QGroupBox>
#include<QButtonGroup>
#include<QToolButton>

class QTabWidget;
class QRadioButton;
class QSlider;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;

class ConfigManager;
class RedshiftController;
struct RedshiftConfig;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(bool startMinimized, QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onModeChanged();
    void onSliderMoved(int value);
    void onGetLocation();
    void onSaveConfig();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onQuit();
    void onSliderValueChanged(int value);   // label only
    void onSliderReleased();                // apply temperature
private:
    void buildUi();
    void buildTray();
    void loadConfigIntoUi();
    void updateStatusBar();
    void setManualEnabled(bool enabled);

    // UI
    QTabWidget    *m_tabs        = nullptr;

    // Config tab
    QSpinBox      *m_dayTemp     = nullptr;
    QSpinBox      *m_nightTemp   = nullptr;
    QDoubleSpinBox*m_lat         = nullptr;
    QDoubleSpinBox*m_lon         = nullptr;
    QComboBox     *m_adjMethod   = nullptr;
    QComboBox     *m_locProvider = nullptr;
    QCheckBox     *m_fade        = nullptr;
    QPushButton   *m_getLocation = nullptr;
    QPushButton   *m_saveConfig  = nullptr;

    // Mode tab
    QRadioButton  *m_autoRadio   = nullptr;
    QRadioButton  *m_manualRadio = nullptr;
    QSlider       *m_slider      = nullptr;
    QLabel        *m_sliderLabel = nullptr;

    // Tray
    QSystemTrayIcon *m_tray      = nullptr;

    // Helpers
    QNetworkAccessManager *m_nam      = nullptr;

    RedshiftController *m_redshift = nullptr;
    QString             m_configPath;
    bool m_modeChanging = false;

    // Settings actions
    QAction *m_startMinimizedAction = nullptr;
    QAction *m_autostartAction      = nullptr;
    bool autostartFileExists() const;
    bool m_startMinimized = false;

private slots:
    void onStartMinimizedToggled(bool checked);
    void onAutostartToggled(bool checked);

private:
    QSlider *m_brightDaySlider   = nullptr;
    QSlider *m_brightNightSlider = nullptr;
    QLabel  *m_brightDayLabel    = nullptr;
    QLabel  *m_brightNightLabel  = nullptr;
    bool     m_createdDefaultConfig = false;

private slots:
    void onBrightDayChanged(int value);
    void onBrightNightChanged(int value);

private:
    QSlider *m_manualBrightnessSlider = nullptr;
    QLabel  *m_manualBrightnessLabel  = nullptr;
    QGroupBox *brightGroup;
    QLabel *dayLabel;
    QLabel *nightLabel;

private slots:
    void onManualBrightnessChanged(int value);

private:
    QAction *m_darkThemeAction = nullptr;
    void applyTheme(bool dark);
    QLabel* m_modeLabel;
private:
    QButtonGroup *m_modeGroup = nullptr;
    bool isdarktheme = false;
    QToolButton *btn;
     QIcon initialIcon;
     QIcon icon;
     QIcon m_chevronLight;
private slots:
    void onDarkThemeToggled(bool checked);
private:
    QCheckBox *m_waylandCheck = nullptr;
    bool m_isWayland = false;
    QAction* m_waylandAction;
    QString configPathForMode(bool wayland) const;
    void performBackup();

private slots:
    void onWaylandToggled(bool checked);

};

#endif // MAINWINDOW_H
