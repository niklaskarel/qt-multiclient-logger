#include "dataprocessor.h"

DataProcessor::DataProcessor():
    m_lowerThreshold(0.0),
    m_upperThreshold(100.0),
    m_windowSize(5),
    m_plotTimeWindowSec(10.0)
{

}

void DataProcessor::setThresholds(const double lower, const double upper)
{
    m_lowerThreshold = lower;
    m_upperThreshold = upper;
}

void DataProcessor::setWindowSize(const int windowSize)
{
    m_windowSize = qMax(1, windowSize);
}

void DataProcessor::setPlotTimeWindowSec(const double seconds)
{
    m_plotTimeWindowSec = seconds;
}

double DataProcessor::getLowerThreshold() const
{
    return m_lowerThreshold;
}

double DataProcessor::getUpperThreshold() const
{
    return m_upperThreshold;
}

int DataProcessor::getWindowSize() const
{
    return m_windowSize;
}

double DataProcessor::getPlotTimeWindow() const
{
    return m_plotTimeWindowSec;
}

void DataProcessor::addSample(const double valueX, const double valueY, const QDateTime timestamp)
{
    // Clip value to thresholds
    double const clippedValueX = qBound(m_lowerThreshold, valueX, m_upperThreshold);
    double const clippedValueY = qBound(m_lowerThreshold, valueY, m_upperThreshold);

    Sample s;
    s.timestamp = timestamp;
    s.valueX = clippedValueX;
    s.valueY = clippedValueY;

    m_samples.append(s);

    // Optionally prune old samples (keep buffer reasonable, not required for moving average)
    while (m_samples.size() > 5000) {  // Hard limit safety net
        m_samples.removeFirst();
    }
}

QVector<QPointF> DataProcessor::getProcessedCurve(QDateTime currentTime)
{
    QVector<QPointF> curve;
    QVector<double> recentValues;

    int countOfPoints {0};
    double sum {0.0};
    double timeAnchor {0.0};
    for (auto it = m_samples.crbegin(); it != m_samples.crend(); ++it) {
        double elapsedSec = it->timestamp.msecsTo(currentTime) / 1000.0;
        if (elapsedSec > m_plotTimeWindowSec) {
            break;
        }

        if (countOfPoints == 0){
            timeAnchor = elapsedSec;
        }
        sum += it->valueX;
        countOfPoints++;

        if (countOfPoints == m_windowSize) {
            double avg = sum / m_windowSize;
            curve.prepend(QPointF(-timeAnchor, avg));
            countOfPoints = 0;
            sum = 0.0;
        }
    }
    return curve;
}

QVector<QVector3D> DataProcessor::getProcessedCurve3D(QDateTime currentTime)
{
    QVector<QVector3D> curve3D;

    int count = 0;
    double sumX = 0.0, sumY = 0.0;
    double timeAnchor = 0.0;

    for (auto it = m_samples.crbegin(); it != m_samples.crend(); ++it) {
        double elapsedSec = it->timestamp.msecsTo(currentTime) / 1000.0;
        if (elapsedSec > m_plotTimeWindowSec)
            break;

        if (count == 0)
            timeAnchor = elapsedSec;

        sumX += it->valueX;
        sumY += it->valueY;
        count++;

        if (count == m_windowSize) {
            double const avgX = sumX / m_windowSize;
            double const avgY = sumY / m_windowSize;
            curve3D.prepend(QVector3D(avgX, avgY, -timeAnchor));
            count = 0;
            sumX = 0.0;
            sumY = 0.0;
        }
    }

    return curve3D;
}
