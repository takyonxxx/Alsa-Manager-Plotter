#include <QApplication>
#include "alsamanager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    AlsaManager ai;
    ai.init(argc, argv);
    return a.exec();
}
