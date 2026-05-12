// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdExporter/qpsdqmlexporterplugin.h>

#include <QtCore/QFile>

QT_BEGIN_NAMESPACE

class QPsdExporterQtQuickPlugin : public QPsdQmlExporterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdExporterFactoryInterface" FILE "qtquick.json")
    Q_PROPERTY(EffectMode effectMode READ effectMode WRITE setEffectMode NOTIFY effectModeChanged FINAL)
public:
    int priority() const override { return 10; }
    QIcon icon() const override {
        return QIcon(":/qtquick/qtquick.png");
    }
    QString name() const override {
        return tr("&Qt Quick");
    }
    ExportType exportType() const override { return QPsdExporterPlugin::Directory; }

    // Shadows the base class non-virtual ABI; we still override the virtual
    // dispatch via effectMode() so the base implementation sees this value.
    EffectMode effectMode() const override { return m_effectMode; }

public slots:
    void setEffectMode(EffectMode effectMode) {
        if (m_effectMode == effectMode) return;
        m_effectMode = effectMode;
        emit effectModeChanged(effectMode);
    }
signals:
    void effectModeChanged(EffectMode effectMode);

protected:
    // Copy the bundled blend / adjustment shaders into the output directory
    // when the traversal emitted ShaderEffect items that reference them.
    void onAfterExport(const ExportConfig & /*config*/) const override {
        if (m_needsBlendShader) {
            QFile shader(":/qtquick/blend.frag.qsb"_L1);
            if (shader.open(QIODevice::ReadOnly)) {
                QFile output(dir.absoluteFilePath("blend.frag.qsb"_L1));
                if (output.open(QIODevice::WriteOnly))
                    output.write(shader.readAll());
            }
            m_needsBlendShader = false;
        }
        if (m_needsAdjustmentShader) {
            QFile shader(":/qtquick/adjustment.frag.qsb"_L1);
            if (shader.open(QIODevice::ReadOnly)) {
                QFile output(dir.absoluteFilePath("adjustment.frag.qsb"_L1));
                if (output.open(QIODevice::WriteOnly))
                    output.write(shader.readAll());
            }
            m_needsAdjustmentShader = false;
        }
    }

private:
    EffectMode m_effectMode = EffectMaker;
};

QT_END_NAMESPACE

#include "qtquick.moc"
