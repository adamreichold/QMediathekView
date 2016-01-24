#include "application.h"

namespace
{

const auto projectName = QStringLiteral("QMediathekView");

} // anonymous

int main(int argc, char** argv)
{
    QApplication::setOrganizationName(projectName);
    QApplication::setApplicationName(projectName);

    return Mediathek::Application(argc, argv).exec();
}
