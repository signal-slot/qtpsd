// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdExporter/qpsdexporterplugin.h>
#include <QtPsdCore/QPsdTypeToolObjectSetting>
#include <QtPsdCore/QPsdEngineDataParser>

#include <QtCore/QCborMap>
#include <QtCore/QCborArray>
#include <QtCore/QJsonDocument>

QT_BEGIN_NAMESPACE

class QPsdExporterJsonPlugin : public QPsdExporterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdExporterFactoryInterface" FILE "json.json")
public:
    int priority() const override { return 1000; }
    QIcon icon() const override {
        auto mimeType = mimeDatabase.mimeTypeForName("application/json");
        return QIcon::fromTheme(mimeType.iconName());
    }
    QString name() const override {
        return tr("&JSON");
    }
    ExportType exportType() const override { return QPsdExporterPlugin::File; }
    QHash<QString, QString> filters() const override { return {{ tr("JSON (*.json)"), ".json" }}; }
    bool exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const QVariantMap &hint) const override;
};

bool QPsdExporterJsonPlugin::exportTo(const QPsdExporterTreeItemModel *model, const QString &to, const QVariantMap &hint) const
{
    auto toJson = [](const QPainterPath &path){
        QJsonArray paths;
        QJsonObject bezier;
        for (int i = 0; i < path.elementCount(); i++) {
            const auto element =path.elementAt(i);

            switch (element.type) {
            case QPainterPath::MoveToElement:
                paths.append(
                    QJsonObject {
                        {"type", "MoveTo"},
                        {"x", element.x},
                        {"y", element.y}
                    });
                break;
            case QPainterPath::LineToElement:
                paths.append(
                    QJsonObject {
                        {"type", "LineTo"},
                        {"x", element.x},
                        {"y", element.y}
                    });
                break;
            case QPainterPath::CurveToElement:
                bezier = QJsonObject {
                    {"type", "CurveTo"},
                    {"x", element.x},
                    {"y", element.y}
                };
                break;
            case QPainterPath::CurveToDataElement:
                if (!bezier.contains("c1")) {
                    bezier.insert("c1", QJsonObject {
                                            { "x", element.x },
                                            { "y", element.y }
                                        });
                } else {
                    bezier.insert("c2", QJsonObject {
                                            { "x", element.x },
                                            { "y", element.y }
                                        });
                    paths.append(bezier);
                }
                break;
            default:
                qFatal() << "Unknown path element type" << element.type;
            }
        }
        return paths;
    };

    std::function<void(const QModelIndex &, QJsonObject *)> traverseTree =
        [&](const QModelIndex &index, QJsonObject *object) {
        QString key;
        if (index.isValid()) {
            const auto *item = model->layerItem(index);
            const auto hint = model->layerHint(index);
            if (hint.type == QPsdExporterTreeItemModel::ExportHint::Skip)
                return;
            object->insert("id", QString::number(item->id()));
            object->insert("name", item->name());
            QJsonObject rect;
            rect.insert("x", item->rect().x());
            rect.insert("y", item->rect().y());
            rect.insert("width", item->rect().width());
            rect.insert("height", item->rect().height());
            if (item->vectorMask().type != QPsdAbstractLayerItem::PathInfo::None)
                rect.insert("mask", toJson(item->vectorMask().path));
            object->insert("rect", rect);
            object->insert("visible", hint.visible);
            switch (item->type()) {
            case QPsdAbstractLayerItem::Folder:
                break;
            case QPsdAbstractLayerItem::Text: {
                object->insert("type", "Text");
                const auto text = dynamic_cast<const QPsdTextLayerItem *>(item);
                QJsonArray runs;
                for (const auto &run : text->runs()) {
                    QJsonObject runObject;
                    runObject.insert("text", run.text);
                    runObject.insert("font", run.font.toString());
                    runObject.insert("color", run.color.name());
                    runObject.insert("alignment", int(run.alignment));
                    runs.append(runObject);
                }
                object->insert("runs", runs);
                QPsdLayerRecord record = text->record();
                const auto additionalLayerInformation = record.additionalLayerInformation();
                const auto tysh = additionalLayerInformation.value("TySh").value<QPsdTypeToolObjectSetting>();
                const auto textData = tysh.textData();
                const auto engineDataData = textData.data().value("EngineData").toByteArray();
                const auto engineData = QPsdEngineDataParser::parseEngineData(engineDataData);

                const auto documentResources = engineData.value("DocumentResources"_L1).toMap();
                const auto fontSet = documentResources.value("FontSet"_L1).toArray();
                const auto styleSheetSet = documentResources.value("StyleSheetSet"_L1).toArray();

                const auto engineDict = engineData.value("EngineDict"_L1).toMap();

                QJsonObject textDataJson;
                textDataJson.insert("engineDict", engineDict.toJsonObject());

                QJsonObject typeToolObjectSettingJson;
                typeToolObjectSettingJson.insert("textData", textDataJson);

                object->insert("typeToolObjectSetting", typeToolObjectSettingJson);
                break; }
            case QPsdAbstractLayerItem::Shape: {
                object->insert("type", "Shape");
                const auto shape = dynamic_cast<const QPsdShapeLayerItem *>(item);
                object->insert("path", toJson(shape->pathInfo().path));
                break; }
            case QPsdAbstractLayerItem::Image:
                object->insert("type", "Image");
                break;
            default:
                break;
            }
            key = item->name();
        }
        
        QJsonArray children;
        for (int i = 0; i < model->rowCount(index); i++) {
            QJsonObject child;
            traverseTree(model->index(i, 0, index), &child);
            children.append(child);
        }

        if (model->rowCount(index) > 0) {
            object->insert("children", children);
        }
    };

    QJsonObject root;
    traverseTree(QModelIndex {}, &root);

    QJsonDocument doc;
    doc.setObject(root);
    QFile file(to);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(doc.toJson());
    file.close();
    return true;
}

QT_END_NAMESPACE

#include "json.moc"
