#ifndef RENDERBACKEND_H
#define RENDERBACKEND_H

#include <QJsonObject>
#include <QPixmap>
#include <QString>
#include <functional>
#include <memory>

class PetProject;

enum class RenderBackendAvailability { Available, Disabled, SdkUnavailable };
enum class RenderBackendType { Sprite, Live2D };

struct RenderBackendLoadResult {
    bool ok {false};
    QString errorCode;
    QString message;
};

struct RenderFrameContext {
    QPixmap spriteFrame;
    QJsonObject actionParameters;
    QSize targetSize;
};

class IRenderBackend
{
public:
    virtual ~IRenderBackend() = default;
    virtual QString backendId() const = 0;
    virtual RenderBackendAvailability availability() const = 0;
    virtual RenderBackendLoadResult load(const PetProject &project) = 0;
    virtual void unload() = 0;
    virtual void setBehaviorState(const QString &stateId) = 0;
    virtual QString behaviorState() const = 0;
    virtual void update(double deltaSeconds) = 0;
    virtual QPixmap render(const RenderFrameContext &context) = 0;
    virtual bool hitTest(const QPointF &point) const = 0;
};

class SpriteRenderBackend final : public IRenderBackend
{
public:
    QString backendId() const override { return QStringLiteral("sprite"); }
    RenderBackendAvailability availability() const override { return RenderBackendAvailability::Available; }
    RenderBackendLoadResult load(const PetProject &) override;
    void unload() override {}
    void setBehaviorState(const QString &stateId) override { m_stateId = stateId; }
    QString behaviorState() const override { return m_stateId; }
    void update(double) override {}
    QPixmap render(const RenderFrameContext &context) override;
    bool hitTest(const QPointF &point) const override;

private:
    QString m_stateId;
    QImage m_lastHitImage;
};

class Live2DRenderBackend : public IRenderBackend
{
public:
    QString backendId() const override { return QStringLiteral("live2d"); }
    RenderBackendAvailability availability() const override;
    RenderBackendLoadResult load(const PetProject &project) override;
    void unload() override;
    void setBehaviorState(const QString &stateId) override { m_stateId = stateId; }
    QString behaviorState() const override { return m_stateId; }
    void update(double deltaSeconds) override { m_elapsedSeconds += qMax(0.0, deltaSeconds); }
    QPixmap render(const RenderFrameContext &context) override;
    bool hitTest(const QPointF &) const override { return false; }
    static QString availabilityText();

private:
    QString m_stateId;
    double m_elapsedSeconds {0.0};
};

class RenderBackendFactory
{
public:
    using Creator = std::function<std::unique_ptr<IRenderBackend>(RenderBackendType)>;

    static RenderBackendType typeFromString(const QString &value);
    static QString typeToString(RenderBackendType type);
    static std::unique_ptr<IRenderBackend> create(RenderBackendType type, const Creator &creator = {});
};

#endif // RENDERBACKEND_H
