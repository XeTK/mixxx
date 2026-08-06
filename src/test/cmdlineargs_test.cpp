#include "util/cmdlineargs.h"

#include <gtest/gtest.h>

#include <QStringList>

#include "test/mixxxtest.h"

namespace {

// Parse a set of command-line arguments into a fresh CmdlineArgs instance.
// Uses the test-only friend entry point so we don't trip the
// no-QCoreApplication precondition of parse(int, char**). The program name is
// prepended because QCommandLineParser expects arguments[0] to be the
// application name (mirroring argv).
CmdlineArgs parseArgs(const QStringList& args) {
    CmdlineArgs cla;
    QStringList fullArgs = QStringList() << QStringLiteral("mixxx") << args;
    EXPECT_TRUE(parseCmdlineArgsForTest(fullArgs, &cla));
    return cla;
}

} // namespace

class CmdlineArgsTest : public MixxxTest {};

TEST_F(CmdlineArgsTest, ControllerNavigationWithoutFocus_DefaultsOff) {
    CmdlineArgs cla = parseArgs(QStringList());
    EXPECT_FALSE(cla.getControllerNavigationWithoutFocus());
}

TEST_F(CmdlineArgsTest, ControllerNavigationWithoutFocus_FlagEnables) {
    CmdlineArgs cla =
            parseArgs(QStringList() << QStringLiteral("--controller-navigation-without-focus"));
    EXPECT_TRUE(cla.getControllerNavigationWithoutFocus());
}

TEST_F(CmdlineArgsTest, ControllerNavigationWithoutFocus_UnrelatedFlagsUnaffected) {
    CmdlineArgs cla = parseArgs(QStringList() << QStringLiteral("--controller-debug")
                                              << QStringLiteral("--developer"));
    EXPECT_FALSE(cla.getControllerNavigationWithoutFocus());
    EXPECT_TRUE(cla.getControllerDebug());
    EXPECT_TRUE(cla.getDeveloper());
}
