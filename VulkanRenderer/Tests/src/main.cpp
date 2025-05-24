#include "Settings.h"
#include "types.h"

#include "catch2/catch_session.hpp"

i32 main(i32 argc, char** argv)
{
    Settings::initCvars();

    Catch::Session session;

    using namespace Catch::Clara;
    session.cli(session.cli());
    i32 returnCode = session.applyCommandLine(argc, argv);
    if (returnCode != 0)
        return returnCode;

    return session.run();
}

