#include "renderbackend.h"

#include "petproject.h"

#include <QPainter>
#include <QTransform>

RenderBackendLoadResult SpriteRenderBackend::load(const PetProject &)
{
    return {true, {}, {}};
}

QPixmap SpriteRenderBackend::render(const RenderFrameContext &context)
{
    const QPixmap &source = context.spriteFrame;
    const QJsonObject &parameters = context.actionParameters;
    if (source.isNull()) return {};
    QPixmap transformed = source;
    if (parameters.value(QStringLiteral("mirror")).toBool(false)) {
        transformed = transformed.transformed(QTransform().scale(-1.0, 1.0));
    }
    const double scale = qBound(0.5, parameters.value(QStringLiteral("scale")).toDouble(1.0), 1.5);
    if (!qFuzzyCompare(scale, 1.0)) {
        transformed = transformed.scaled(qMax(1, qRound(transformed.width() * scale)),
                                          qMax(1, qRound(transformed.height() * scale)),
                                          Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    const QSize outputSize = context.targetSize.isValid() ? context.targetSize : source.size();
    QPixmap composed(outputSize);
    composed.fill(Qt::transparent);
    QPainter painter(&composed);
    painter.drawPixmap((composed.width() - transformed.width()) / 2 + parameters.value(QStringLiteral("offsetX")).toInt(),
                       (composed.height() - transformed.height()) / 2 + parameters.value(QStringLiteral("offsetY")).toInt(),
                       transformed);
    m_lastHitImage = composed.toImage();
    return composed;
}

bool SpriteRenderBackend::hitTest(const QPointF &point) const
{
    const QPoint pixel = point.toPoint();
    return m_lastHitImage.rect().contains(pixel) && qAlpha(m_lastHitImage.pixel(pixel)) > 0;
}

RenderBackendAvailability Live2DRenderBackend::availability() const
{
#if defined(DESKTOP_PET_ENABLE_LIVE2D) && DESKTOP_PET_ENABLE_LIVE2D
    return RenderBackendAvailability::SdkUnavailable;
#else
    return RenderBackendAvailability::Disabled;
#endif
}

RenderBackendLoadResult Live2DRenderBackend::load(const PetProject &project)
{
    Q_UNUSED(project)
    const RenderBackendAvailability state = availability();
    if (state == RenderBackendAvailability::Disabled) {
        return {false, QStringLiteral("Live2DDisabled"),
                QStringLiteral("Live2D 适配骨架已安装；当前构建未启用 Cubism SDK。")};
    }
    return {false, QStringLiteral("CubismSdkUnavailable"),
            QStringLiteral("Live2D 适配骨架已安装；未检测到可用的 Cubism SDK 和模型运行后端。")};
}

void Live2DRenderBackend::unload()
{
    m_stateId.clear();
    m_elapsedSeconds = 0.0;
}

QPixmap Live2DRenderBackend::render(const RenderFrameContext &context)
{
    Q_UNUSED(context)
    return {};
}

QString Live2DRenderBackend::availabilityText()
{
    Live2DRenderBackend backend;
    return backend.availability() == RenderBackendAvailability::Disabled
        ? QStringLiteral("Live2D 适配骨架已安装；当前项目仍使用 Sprite Runtime（构建未启用 Cubism SDK）。")
        : QStringLiteral("Live2D 适配骨架已安装；Cubism SDK/模型后端不可用，项目将安全降级到 Sprite Runtime。")
        ;
}

RenderBackendType RenderBackendFactory::typeFromString(const QString &value)
{
    return value.compare(QStringLiteral("live2d"), Qt::CaseInsensitive) == 0
        ? RenderBackendType::Live2D : RenderBackendType::Sprite;
}

QString RenderBackendFactory::typeToString(RenderBackendType type)
{
    return type == RenderBackendType::Live2D ? QStringLiteral("live2d") : QStringLiteral("sprite");
}

std::unique_ptr<IRenderBackend> RenderBackendFactory::create(RenderBackendType type, const Creator &creator)
{
    if (creator) {
        if (std::unique_ptr<IRenderBackend> backend = creator(type)) return backend;
    }
    if (type == RenderBackendType::Live2D) return std::make_unique<Live2DRenderBackend>();
    return std::make_unique<SpriteRenderBackend>();
}
