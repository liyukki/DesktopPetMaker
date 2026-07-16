#ifndef ASSETQUALITYANALYZER_H
#define ASSETQUALITYANALYZER_H

#include <QStringList>

struct AssetQualityReport
{
    bool allFramesReadable {true};
    bool canvasSizeConsistent {true};
    double maxWidthVariationPercent {0.0};
    double maxHeightVariationPercent {0.0};
    int maxCenterXDrift {0};
    int maxBottomYDrift {0};
    QStringList warnings;
};

class AssetQualityAnalyzer
{
public:
    static AssetQualityReport analyzeFrames(const QStringList &framePaths);
};

#endif // ASSETQUALITYANALYZER_H
