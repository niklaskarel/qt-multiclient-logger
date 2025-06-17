#include "settings.h"
#include "ui_settings.h"

Settings::Settings(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Settings)
{
    ui->setupUi(this);

    QRegularExpression ipRegex(R"(^((25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])\.){3}(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])$)");
    QValidator *ipValidator = new QRegularExpressionValidator(ipRegex, ui->ipAddressLineEdit);
    ui->ipAddressLineEdit->setValidator(ipValidator);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &Settings::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &Settings::reject);
}

Settings::~Settings()
{
    delete ui;
}

int Settings::getTcpPort() const
{
    return ui->tcpPortSpinBox->value();
}

QString Settings::getIpAddress() const
{
    return ui->ipAddressLineEdit->text();
}

double Settings::getUpperThreshold() const
{
    return ui->upperThresholdSpinBox->value();
}

double Settings::getLowerThreshold() const
{
    return ui->lowerThresholdSpinBox->value();
}

int Settings::getPlotTime() const
{
    return ui->plotTimeSpinBox->value();
}

int Settings::getNumSamplesToAvg() const
{
    return ui->numSamplesAvgSpinBox->value();
}

int Settings::getFlushInterval() const
{
    return ui->flushIntervalSpinBox->value();
}

int Settings::getRingBufferSize() const
{
    return ui->ringBufferSizeSpinBox->value();
}


void Settings::loadSettings(const QString & ipAddress, const int locaPort,
                            const double lowerThres, const double upperThress,
                            const int plotTime, const int numSamplesAvg,
                            const int flushInterval, const int ringBufferSize)
{
    ui->ringBufferSizeSpinBox->setValue(ringBufferSize);
    ui->ipAddressLineEdit->setText(ipAddress);
    ui->tcpPortSpinBox->setValue(locaPort);
    ui->lowerThresholdSpinBox->setValue(lowerThres);
    ui->upperThresholdSpinBox->setValue(upperThress);
    ui->flushIntervalSpinBox->setValue(flushInterval);
    ui->plotTimeSpinBox->setValue(plotTime);
    ui->numSamplesAvgSpinBox->setValue(numSamplesAvg);
}
