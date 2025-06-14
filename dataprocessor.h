#ifndef DATAPROCESSOR_H
#define DATAPROCESSOR_H

#include <QVector>
#include <QDateTime>
#include <QPointF>

class DataProcessor
{
public:
    DataProcessor();

    void setThresholds(double lower, double upper);
    void setWindowSize(int windowSize);
    void setPlotTimeWindowSec(double seconds);

    void addSample(const double value, QDateTime timestamp);

    QVector<QPointF> getProcessedCurve(QDateTime currentTime);
    double getPlotTimeWindowSec() const { return m_plotTimeWindowSec; }

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
