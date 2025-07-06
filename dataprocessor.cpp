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

void DataProcessor::addSample(const double value, QDateTime timestamp)
{
    // Clip value to thresholds
    double const clippedValue = qBound(m_lowerThreshold, value, m_upperThreshold);

    Sample s;
    s.timestamp = timestamp;
    s.value = clippedValue;

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
        sum += it->value;
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
