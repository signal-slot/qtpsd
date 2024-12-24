// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtTest/QtTest>
#include <QtPsdCore/QPsdLayerRecord>
#include <QtPsdCore/QPsdLayerTreeItemModel>
#include <QtPsdCore/QPsdParser>

class tst_QPsdLayerTreeItemModel : public QObject
{
    Q_OBJECT
private slots:
    void parse_nested_layers();
    void parse_group();
    void parse_clippingmask();
};

void tst_QPsdLayerTreeItemModel::parse_nested_layers()
{
    QDir dir;
    dir.cd(QFINDTESTDATA("data/"_L1));

    QPsdParser parser;
    parser.load(dir.filePath("nested_layers.psd"));

    QPsdLayerTreeItemModel layerTree;
    layerTree.fromParser(parser);

    int rootRowCount = layerTree.rowCount(QModelIndex());
    QCOMPARE(rootRowCount, 1);

    QList<qint32> result;
    std::function<void(const QModelIndex &index)> traverseTreeView;
    traverseTreeView = [&](const QModelIndex &index) {
        result.append(layerTree.rowCount(index));

        if (index.isValid()) {
            const int layerId = layerTree.data(index, QPsdLayerTreeItemModel::Roles::LayerIdRole).value<int>();

            const auto *layerRecord = layerTree.data(index, QPsdLayerTreeItemModel::Roles::LayerRecordObjectRole).value<const QPsdLayerRecord *>();
            const auto additionalLayerInformation = layerRecord->additionalLayerInformation();

            // Layer ID
            const auto lyid = additionalLayerInformation.value("lyid").value<quint32>();

            QCOMPARE(layerId, lyid);
        }

        if (layerTree.hasChildren(index)) {
            for (int row = 0; row < layerTree.rowCount(index); row++) {
                traverseTreeView(layerTree.index(row, 0, index));
            }
        }
    };
    traverseTreeView(QModelIndex());

    QCOMPARE(result, QList<qint32>({ 1, 2, 3, 0, 2, 0, 0, 0, 3, 0, 2, 0, 0, 0 }));
}

void tst_QPsdLayerTreeItemModel::parse_group()
{
    QDir dir;
    dir.cd(QFINDTESTDATA("data/"_L1));

    QPsdParser parser;
    parser.load(dir.filePath("group.psd"));

    QPsdLayerTreeItemModel layerTree;
    layerTree.fromParser(parser);

    int rootRowCount = layerTree.rowCount(QModelIndex());
    QCOMPARE(rootRowCount, 2);

    QModelIndex groupIndex = layerTree.index(0, 0, QModelIndex());
    int groupRowCount = layerTree.rowCount(groupIndex);
    QCOMPARE(groupRowCount, 2);

    QModelIndex s1Index = layerTree.index(0, 0, groupIndex);
    QModelIndex s2Index = layerTree.index(1, 0, groupIndex);

    QVariantList list = layerTree.data(s1Index, QPsdLayerTreeItemModel::Roles::GroupIndexesRole).toList();
    QCOMPARE(list.size(), 2);
    QList<QPersistentModelIndex> listIndex;
    for (auto &v : list) {
        listIndex.append(v.toModelIndex());
    }
    QVERIFY(listIndex.contains(s1Index));
    QVERIFY(listIndex.contains(s2Index));
}

void tst_QPsdLayerTreeItemModel::parse_clippingmask()
{
    QDir dir;
    dir.cd(QFINDTESTDATA("data/"_L1));

    QPsdParser parser;
    parser.load(dir.filePath("clippingmask.psd"));

    QPsdLayerTreeItemModel layerTree;
    layerTree.fromParser(parser);

    int rootRowCount = layerTree.rowCount(QModelIndex());
    QCOMPARE(rootRowCount, 1);

    QModelIndex rootIndex = layerTree.index(0, 0, QModelIndex());
    int rootChildRowCount = layerTree.rowCount(rootIndex);
    QCOMPARE(rootChildRowCount, 1);

    QModelIndex g2 = layerTree.index(0, 0, rootIndex);
    int g2ChildRowCount = layerTree.rowCount(g2);
    QCOMPARE(g2ChildRowCount, 5);

    QModelIndex i0 = layerTree.index(0, 0, g2);
    QCOMPARE(layerTree.rowCount(i0), 0);
    QModelIndex i0c = layerTree.data(i0, QPsdLayerTreeItemModel::Roles::ClippingMaskIndexRole).toModelIndex();
    QVERIFY(!i0c.isValid());

    QModelIndex i1 = layerTree.index(1, 0, g2);
    QModelIndex i1c = layerTree.data(i1, QPsdLayerTreeItemModel::Roles::ClippingMaskIndexRole).toModelIndex();
    QVERIFY(i1c.isValid());

    QModelIndex g1 = layerTree.index(3, 0, g2);
    QCOMPARE(g1, i1c);

    QModelIndex i4 = layerTree.index(0, 0, g1);
    QModelIndex i4c = layerTree.data(i4, QPsdLayerTreeItemModel::Roles::ClippingMaskIndexRole).toModelIndex();
    QModelIndex i5 = layerTree.index(1, 0, g1);
    QCOMPARE(i5, i4c);
}

QTEST_MAIN(tst_QPsdLayerTreeItemModel)
#include "tst_qpsdlayertreeitemmodel.moc"
