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
