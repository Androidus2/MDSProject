#include <QTest>
#include "Tester.h"
#include "tst_StrokeItem.h"
#include "tst_BrushTool.h"
#include "tst_EraserTool.h"

int main(int argc, char** argv) {
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
        tst_BrushTool brushToolTest;
        status |= QTest::qExec(&brushToolTest, argc, argv);
	}
    {
        tst_EraserTool eraserToolTest;
        status |= QTest::qExec(&eraserToolTest, argc, argv);
	}

    return status;
}
