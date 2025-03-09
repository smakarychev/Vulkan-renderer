#include "catch2/catch_test_macros.hpp"

#include "Events/Event.h"

TEST_CASE("EventHandler can be attached", "[EventHandler]")
{
    Event<> event;
    EventHandler<> handler([](){});
    handler.Connect(event);
    REQUIRE(handler.IsAttached());
}