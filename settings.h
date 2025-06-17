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

    // set the settings from the previous opening, or default values
    void loadSettings(const QString & ipAddress, const int locaPort,
                    const double lowerThres, const double upperThress,
                    const int plotTime, const int numSamplesAvg,
                    const int flushInterval, const int ringBufferSize);

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
