#include <catch2/catch_session.hpp>

#include <CoreLib/types.h>

i32 main(i32 argc, char** argv)
{
    Catch::Session session;

    using namespace Catch::Clara;
    session.cli(session.cli());
    i32 returnCode = session.applyCommandLine(argc, argv);
    if (returnCode != 0)
        return returnCode;

    return session.run();
}

