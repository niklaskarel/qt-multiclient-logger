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

    for (auto it = m_samples.crbegin(); it != m_samples.crend(); ++it) {
        double elapsedSec = it->timestamp.msecsTo(currentTime) / 1000.0;
        if (elapsedSec > m_plotTimeWindowSec) {
            break;
        }
        recentValues.prepend(it->value);

        // Compute moving average over last N values
        int N = qMin(recentValues.size(), m_windowSize);
        double avg = 0.0;
        for (int i = recentValues.size() - N; i < recentValues.size(); ++i) {
            avg += recentValues[i];
        }
        avg /= N;

        // Insert point with X = timestamp seconds ago, Y = moving average
        curve.prepend(QPointF(-elapsedSec, avg));  // negative X for plotting from right to left
    }

    return curve;
}
