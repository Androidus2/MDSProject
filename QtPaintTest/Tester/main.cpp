#include <QTest>
#include "Tester.h"
#include "tst_StrokeItem.h"

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

    return status;
}
