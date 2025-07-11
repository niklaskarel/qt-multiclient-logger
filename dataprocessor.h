#ifndef DATAPROCESSOR_H
#define DATAPROCESSOR_H

#include <QVector>
#include <QDateTime>
#include <QPointF>

class DataProcessor
{
public:
    DataProcessor();

    // setters
    void setThresholds(const double lower, const double upper);
    void setWindowSize(const int windowSize);
    void setPlotTimeWindowSec(const double seconds);

    // getters
    double getLowerThreshold() const;
    double getUpperThreshold() const;
    int getWindowSize() const;
    double getPlotTimeWindow() const;

    void addSample(const double value, QDateTime timestamp);

    QVector<QPointF> getProcessedCurve(QDateTime currentTime);

private:
    struct Sample {
        QDateTime timestamp;
        double value;
    };

    QVector<Sample> m_samples;
    double m_lowerThreshold;
    double m_upperThreshold;
    int m_windowSize;  // Moving average window (number of samples)
    double m_plotTimeWindowSec;  // seconds for X axis

    QVector<double> getLastNSamplesAverage(int N) const;
};

#endif // DATAPROCESSOR_H
