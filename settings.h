#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDialog>

namespace Ui {
class Settings;
}

class Settings : public QDialog
{
    Q_OBJECT

public:
    explicit Settings(QWidget *parent = nullptr);
    ~Settings();

    // getter functions
    int getTcpPort() const;
    QString getIpAddress() const;
    double getUpperThreshold() const;
    double getLowerThreshold() const;
    int getPlotTime() const;
    int getNumSamplesToAvg() const;
    int getFlushInterval() const;
    int getRingBufferSize() const;

private:
    Ui::Settings *ui;
};

#endif // SETTINGS_H
