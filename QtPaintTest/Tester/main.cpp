#include <QTest>
#include "Tester.h"
#include "tst_StrokeItem.h"
#include "tst_ClipboardItem.h"
#include "tst_Layer.h"
#include "tst_RasterItem.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    int status = 0;

    {
        Tester tester;
        status |= QTest::qExec(&tester, argc, argv);
    }

    {
        StrokeItemTest strokeItemTest;
        status |= QTest::qExec(&strokeItemTest, argc, argv);
    }

    {
        ClipboardItemTest clipboardItemTest;
        status |= QTest::qExec(&clipboardItemTest, argc, argv);
    }

    {
        LayerTest layerTest;
        status |= QTest::qExec(&layerTest, argc, argv);
    }

    {
        RasterItemTest rasterItemTest;
        status |= QTest::qExec(&rasterItemTest, argc, argv);
    }

    return status;
}