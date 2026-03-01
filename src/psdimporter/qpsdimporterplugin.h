// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDIMPORTERPLUGIN_H
#define QPSDIMPORTERPLUGIN_H

#include <QtPsdImporter/qpsdimporterglobal.h>
#include <QtPsdExporter/qpsdexportertreeitemmodel.h>

#include <QtPsdCore/qpsdabstractplugin.h>

#include <QtCore/QUrl>
#include <QtCore/QVariantMap>
#include <QtGui/QIcon>

#include <functional>

QT_BEGIN_NAMESPACE

QT_FORWARD_DECLARE_CLASS(QWidget)

#define QPsdImporterFactoryInterface_iid "org.qt-project.Qt.QPsdImporterFactoryInterface"

class Q_PSDIMPORTER_EXPORT QPsdImporterPlugin : public QPsdAbstractPlugin
{
    Q_OBJECT
public:
    explicit QPsdImporterPlugin(QObject *parent = nullptr);
    virtual ~QPsdImporterPlugin();

    virtual int priority() const = 0;
    virtual QIcon icon() const { return QIcon(); }
    virtual QString name() const = 0;

    // Show plugin-specific import dialog. Returns options map on acceptance.
    // Returns empty map if the user cancelled.
    virtual QVariantMap execImportDialog(QWidget *parent) const = 0;

    // Perform import with the given options (no GUI).
    virtual bool importFrom(QPsdExporterTreeItemModel *model,
                            const QVariantMap &options) const = 0;

    // URL-based import support.
    // Returns true if this plugin can handle the given URL (e.g., figma.com URLs).
    virtual bool canHandleUrl(const QUrl &url) const { Q_UNUSED(url); return false; }

    // Build an options map from the URL (and settings/env for credentials).
    // Default returns {"source": url.toString()}.
    virtual QVariantMap optionsFromUrl(const QUrl &url) const {
        return {{"source"_L1, url.toString()}};
    }

    // Find the first importer plugin that can handle the given URL.
    // Returns nullptr if no plugin matches.
    static QPsdImporterPlugin *pluginForUrl(const QUrl &url) {
        const auto allKeys = keys();
        for (const auto &key : allKeys) {
            auto *p = plugin(key);
            if (p && p->canHandleUrl(url))
                return p;
        }
        return nullptr;
    }

    QString errorMessage() const { return m_errorMessage; }

    using ProgressCallback = std::function<void(int value, int maximum)>;
    void setProgressCallback(const ProgressCallback &callback) const { m_progressCallback = callback; }

    static QByteArrayList keys() {
        return QPsdAbstractPlugin::keys<QPsdImporterPlugin>(QPsdImporterFactoryInterface_iid, "psdimporter");
    }
    static QPsdImporterPlugin *plugin(const QByteArray &key) {
        return QPsdAbstractPlugin::plugin<QPsdImporterPlugin>(QPsdImporterFactoryInterface_iid, "psdimporter", key);
    }

protected:
    void setErrorMessage(const QString &message) const { m_errorMessage = message; }
    void reportProgress(int value, int maximum) const {
        if (m_progressCallback) m_progressCallback(value, maximum);
    }

private:
    mutable QString m_errorMessage;
    mutable ProgressCallback m_progressCallback;
};

QT_END_NAMESPACE

#endif // QPSDIMPORTERPLUGIN_H
