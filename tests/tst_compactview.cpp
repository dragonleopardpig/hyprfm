#include <QTest>
#include <QPair>
#include <QSet>
#include <QSignalSpy>
#include <QWheelEvent>
#include "viewharness.h"
#include "providers/iconprovider.h"

class TestCompactView : public QObject
{
    Q_OBJECT

    struct Harness : ViewHarness {
        bool loadCompact(int count = 80)
        {
            QStringList names;
            for (int i = 0; i < count; ++i)
                names << QStringLiteral("file-%1.txt").arg(i, 3, 10, QLatin1Char('0'));
            files.createFiles(names);
            return load(QStringLiteral("src/qml/views/FileCompactView.qml"), "model");
        }
        void wheel(QPoint angle, Qt::KeyboardModifiers mods = {}, QPoint pixels = {})
        {
            const QPoint pos(400, 300);
            QWheelEvent event(pos, view.mapToGlobal(pos), pixels, angle,
                              Qt::NoButton, mods, pixels.isNull() ? Qt::NoScrollPhase : Qt::ScrollUpdate, false);
            QCoreApplication::sendEvent(&view, &event);
        }
    };

private slots:
    void testColumnsFillDownThenAcrossAndReflowOnResize()
    {
        Harness h;
        QVERIFY(h.loadCompact());
        auto *root = h.view.rootObject();
        const int rows = root->property("rowsPerColumn").toInt();
        QVERIFY(rows > 2);
        auto *first = h.item("compactItem_0");
        auto *second = h.item("compactItem_1");
        auto *nextColumn = h.item(QStringLiteral("compactItem_%1").arg(rows));
        QVERIFY(first && second && nextColumn);
        QCOMPARE(first->x(), second->x());
        QVERIFY(second->y() > first->y());
        QVERIFY(nextColumn->x() > first->x());
        QCOMPARE(first->y(), nextColumn->y());
        if (qEnvironmentVariableIsSet("HYPRFM_TEST_GRAB"))
            h.view.grabWindow().save(qEnvironmentVariable("HYPRFM_TEST_GRAB"));
        const int iconSize = root->property("iconSize").toInt();
        h.view.resize(700, 300);
        QTRY_VERIFY(root->property("rowsPerColumn").toInt() < rows);
        QCOMPARE(root->property("iconSize").toInt(), iconSize);
        QVERIFY(root->property("contentWidth").toReal() > root->width());
    }

    void testKeyboardSelectionRevealAndActivation()
    {
        Harness h;
        QVERIFY(h.loadCompact(200));
        auto *root = h.view.rootObject();
        root->forceActiveFocus();
        const int rows = root->property("rowsPerColumn").toInt();
        QTest::keyClick(&h.view, Qt::Key_Home);
        QTest::keyClick(&h.view, Qt::Key_Down);
        QCOMPARE(root->property("cursorIndex").toInt(), 1);
        QTest::keyClick(&h.view, Qt::Key_Right);
        QCOMPARE(root->property("cursorIndex").toInt(), rows + 1);
        QTest::keyClick(&h.view, Qt::Key_Left);
        QCOMPARE(root->property("cursorIndex").toInt(), 1);
        QTest::keyClick(&h.view, Qt::Key_Down, Qt::ShiftModifier);
        QCOMPARE(root->property("selectedIndices").toList(), QVariantList({1, 2}));
        QTest::keyClick(&h.view, Qt::Key_End);
        const int last = h.model->rowCount() - 1;
        QCOMPARE(root->property("cursorIndex").toInt(), last);
        QTRY_VERIFY(root->property("contentX").toReal() > 0);
        QSignalSpy activated(root, SIGNAL(fileActivated(QString,bool)));
        QTest::keyClick(&h.view, Qt::Key_Return);
        QCOMPARE(activated.count(), 1);
        QCOMPARE(activated.first().first().toString(), h.model->filePath(last));
        QTest::keyClick(&h.view, Qt::Key_Home);
        for (char c : QByteArray("file-045"))
            QTest::keyClick(&h.view, c);
        QCOMPARE(h.model->fileName(root->property("cursorIndex").toInt()), QString("file-045.txt"));
    }

    void testMouseSelectionAndContextMenu()
    {
        Harness h;
        QVERIFY(h.loadCompact());
        auto *root = h.view.rootObject();
        auto *first = h.item("compactItem_0");
        auto *second = h.item("compactItem_1");
        QVERIFY(first && second);
        QTest::mouseClick(&h.view, Qt::LeftButton, {}, h.center(first));
        QTest::mouseClick(&h.view, Qt::LeftButton, Qt::ControlModifier, h.center(second));
        QCOMPARE(root->property("selectedIndices").toList(), QVariantList({0, 1}));
        QSignalSpy context(root, SIGNAL(contextMenuRequested(QString,bool,QPointF)));
        QVERIFY(context.isValid());
        QTest::mouseClick(&h.view, Qt::RightButton, {}, h.center(first));
        QCOMPARE(context.count(), 1);
        QCOMPARE(context.first().first().toString(), h.model->filePath(0));
        QCOMPARE(root->property("selectedIndices").toList(), QVariantList({0, 1}));
    }

    // A compact row is one click target end to end, like a Detailed row: the
    // blank space after a short name still picks that file. (The grid's tile
    // is narrower than its cell; in Compact mode it must span the row.)
    void testClickOnBlankEndOfRowSelectsIt()
    {
        Harness h;
        QVERIFY(h.loadCompact());
        auto *root = h.view.rootObject();
        auto *second = h.item("compactItem_1");
        auto *label = h.item("compactLabel_1");
        QVERIFY(second && label);
        auto *tile = second->property("tile").value<QQuickItem *>();
        QVERIFY(tile);
        QCOMPARE(tile->width(), second->width());
        QCOMPARE(tile->height(), second->height());

        const QPoint blankEnd = second->mapToScene(QPointF(second->width() - 4, second->height() / 2)).toPoint();
        QVERIFY(blankEnd.x() > label->mapToScene(QPointF(label->implicitWidth(), 0)).x());
        QTest::mouseClick(&h.view, Qt::LeftButton, {}, blankEnd);
        QCOMPARE(root->property("selectedIndices").toList(), QVariantList({1}));
    }

    void testVerticalWheelScrollsSideways()
    {
        Harness h;
        QVERIFY(h.loadCompact(200));
        auto *root = h.view.rootObject();
        h.wheel(QPoint(0, -120));
        QTRY_VERIFY(root->property("contentX").toReal() > 0);
        QCOMPARE(root->property("contentY").toReal(), 0.0);
    }

    void testHorizontalSwipeScrollsSideways()
    {
        Harness h;
        QVERIFY(h.loadCompact(200));
        h.wheel({}, {}, QPoint(-60, 0));
        QTRY_VERIFY(h.view.rootObject()->property("contentX").toReal() > 0);
    }

    void testCtrlWheelZoomsWithoutChangingGridZoom()
    {
        Harness h;
        QVERIFY(h.loadCompact());
        auto *root = h.view.rootObject();
        const int rowHeight = root->property("rowHeight").toInt();
        h.wheel(QPoint(0, 120), Qt::ControlModifier);
        QTRY_VERIFY(root->property("rowHeight").toInt() > rowHeight);
        QCOMPARE(root->property("columnCount").toInt(), 7);
    }

    void testFullNamesPreferenceWidensColumnsAndPersists()
    {
        Harness h;
        const QString name = QString(90, QLatin1Char('w')) + ".txt";
        h.files.createFile(name);
        QVERIFY(h.loadCompact());
        auto *root = h.view.rootObject();
        QTRY_VERIFY(root->property("compactNameWidth").toReal() > 0);
        const qreal narrow = root->property("cellWidth").toReal();
        h.config->saveSettings({{"compactFullNames", true}});
        QTRY_VERIFY(root->property("cellWidth").toReal() > narrow * 2);
        ConfigManager restored(h.configDir.path() + "/config.toml");
        QVERIFY(restored.compactFullNames());
        h.config->saveSettings({{"compactFullNames", false}});
        QTRY_COMPARE(root->property("cellWidth").toReal(), narrow);
    }

    void testRemovingLongestFolderHidesPooledDelegate()
    {
        Harness h;
        for (const QString &dir : {"Books", "DONE READING", "Papers",
                                   "Project_VeryLongName_Tools", "References"})
            h.files.createDir(dir);
        QVERIFY(h.load(QStringLiteral("src/qml/views/FileCompactView.qml"), "model"));

        auto *root = h.view.rootObject();
        h.config->saveSettings({{"compactFullNames", true}});
        QTRY_VERIFY(root->property("compactNameWidth").toReal() > 0);
        QTest::qWait(50);
        const qreal widthBefore = root->property("cellWidth").toReal();

        QVERIFY(QDir(h.files.path() + "/Project_VeryLongName_Tools").removeRecursively());
        h.model->refresh();

        QTRY_VERIFY(root->property("cellWidth").toReal() < widthBefore);
        QTest::qWait(450);

        auto *content = root->property("contentItem").value<QQuickItem *>();
        QVERIFY(content);

        auto verifyDelegates = [&](bool requireZebra) {
            int activeCount = 0;
            bool sawZebra = false;
            QSet<QPair<int, int>> positions;
            for (QQuickItem *child : content->childItems()) {
                if (!child->property("fileName").isValid())
                    continue;
                const int idx = child->property("index").toInt();
                if (idx < 0) {
                    QVERIFY2(!child->isVisible(),
                             qPrintable(QStringLiteral("Pooled delegate still visible: %1")
                                            .arg(child->property("fileName").toString())));
                    continue;
                }
                QVERIFY(child->isVisible());
                QVERIFY(qFuzzyCompare(child->opacity(), 1.0));
                QVERIFY(qFuzzyCompare(child->scale(), 1.0));
                const QPair<int, int> pos(qRound(child->x()), qRound(child->y()));
                QVERIFY2(!positions.contains(pos),
                         qPrintable(QStringLiteral("Overlapping delegate at (%1, %2)")
                                        .arg(pos.first).arg(pos.second)));
                positions.insert(pos);
                ++activeCount;
                if (child->property("fileName").toString() == QLatin1String("Zebra"))
                    sawZebra = true;
            }
            QCOMPARE(activeCount, h.model->rowCount());
            if (requireZebra)
                QVERIFY(sawZebra);
        };

        verifyDelegates(false);

        h.files.createDir(QStringLiteral("Zebra"));
        h.model->refresh();
        QTRY_COMPARE(h.model->rowCount(), 8);
        QTest::qWait(300);

        verifyDelegates(true);
    }
};

QTEST_MAIN(TestCompactView)
#include "tst_compactview.moc"
