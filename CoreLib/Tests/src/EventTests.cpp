#include "catch2/catch_test_macros.hpp"

#include "Events/Event.h"

// NOLINTBEGIN

TEST_CASE("EventHandler can be attached", "[EventHandler]")
{
    Event event;
    EventHandler<> handler([](){});
    handler.Connect(event);
    REQUIRE(handler.IsAttached());
}
TEST_CASE("EventHandler callback is called", "[EventHandler]")
{
    Event event;
    SECTION("Single EventHandler is signalled")
    {
        EventHandler<> handler([](){ REQUIRE(true); });
        handler.Connect(event);
        event.Signal();
    }
    SECTION("Multiple EventHandlers are signalled")
    {
        EventHandler<> handlerA([](){ REQUIRE(true); });
        EventHandler<> handlerB([](){ REQUIRE(true); });
        handlerA.Connect(event);
        handlerB.Connect(event);
        event.Signal();
    }
}
TEST_CASE("EventHandler callback receives correct argument", "[EventHandler]")
{
    Event<u32> event;
    SECTION("Passed `0` as argument")
    {
        EventHandler<u32> handler([](u32 val){ REQUIRE(val == 0); });
        handler.Connect(event);
        event.Signal(0);
    }
    SECTION("Passed `42` as argument")
    {
        EventHandler<u32> handler([](u32 val){ REQUIRE(val == 42); });
        handler.Connect(event);
        event.Signal(42);
    }
    SECTION("Passed `42` as argument and capture")
    {
        constexpr u32 capture = 42;
        EventHandler<u32> handler([capture](u32 val){ REQUIRE(val == 42); REQUIRE(capture == 42); });
        handler.Connect(event);
        event.Signal(42);
    }
}
TEST_CASE("EventHandler added during Signal call", "[EventHandler][Event]")
{
    Event event;
    bool expectCall = false;
    bool doConnect = true;
    EventHandler<> subHandler([&expectCall](){ REQUIRE(expectCall); });
    EventHandler<> handler([&event, &subHandler, &doConnect]()
    {
        if (doConnect)
            subHandler.Connect(event);
    });
    handler.Connect(event);
    
    SECTION("Callback is not executed on first call to Signal")
    {
        expectCall = false;
        event.Signal();
    }
    SECTION("Callback is executed on second call to Signal")
    {
        event.Signal();
        expectCall = true;
        doConnect = false;
        event.Signal();
    }
}
TEST_CASE("EventHandler disconnects on destruction", "[EventHandler]")
{
    Event event;
    {
        EventHandler<> handler([](){ REQUIRE(false); });
        handler.Connect(event);
    }
    event.Signal();
}
TEST_CASE("EventHandler copy/move constructors and assignment operators", "[EventHandler]")
{
    u32 numberOfCalls = 0;
    Event<u32> event;
    
    SECTION("If original has no event, constructed copy callback is not called")
    {
        EventHandler<u32> original([](u32 val){ REQUIRE(false); });
        EventHandler copy(original);
        REQUIRE(!copy.IsAttached());
        event.Signal(0);
    }
    SECTION("If original has no event, assigned copy callback is not called")
    {
        EventHandler<u32> original([](u32 val){ REQUIRE(false); });
        EventHandler<u32> copy = {};
        copy = original;
        REQUIRE(!copy.IsAttached());
        event.Signal(0);
    }
    
    EventHandler<u32> handler([&numberOfCalls](u32 val)
    {
        REQUIRE(val == 42);
        numberOfCalls++;
    });
    handler.Connect(event);
    SECTION("Copy constructed EventHandler inherits event")
    {
        EventHandler<u32> copy(handler);
        REQUIRE(copy.IsAttached());
        event.Signal(42);
        REQUIRE(numberOfCalls == 2);
    }
    SECTION("Copy assigned EventHandler inherits event")
    {
        EventHandler<u32> copy = {};
        copy = handler;
        REQUIRE(copy.IsAttached());
        event.Signal(42);
        REQUIRE(numberOfCalls == 2);
    }
    SECTION("Move constructed EventHandler inherits event, original EventHandler is not called")
    {
        EventHandler<u32> move(std::move(handler));
        REQUIRE(move.IsAttached());
        event.Signal(42);
        REQUIRE(numberOfCalls == 1);
    }
    SECTION("Move assigned EventHandler inherits event, original EventHandler is not called")
    {
        EventHandler<u32> move = {};
        move = std::move(handler);
        REQUIRE(move.IsAttached());
        event.Signal(42);
        REQUIRE(numberOfCalls == 1);
    }
    SECTION("Self copy assignment operator returns self")
    {
        handler = handler;
        REQUIRE(handler.IsAttached());
        event.Signal(42);
        REQUIRE(numberOfCalls == 1);
    }
    SECTION("Self move assignment operator returns self")
    {
        handler = std::move(handler);
        REQUIRE(handler.IsAttached());
        event.Signal(42);
        REQUIRE(numberOfCalls == 1);
    }
}
TEST_CASE("Event can clear all handlers", "[Event]")
{
    bool expectCall = true;
    Event event;
    EventHandler<> handlerA([&expectCall](){ REQUIRE(expectCall); });
    EventHandler<> handlerB([&expectCall](){ REQUIRE(expectCall); });
    handlerA.Connect(event);
    handlerB.Connect(event);
    REQUIRE(handlerA.IsAttached());
    REQUIRE(handlerB.IsAttached());
    event.Signal();

    expectCall = false;
    event.DisconnectHandlers();
    REQUIRE(!handlerA.IsAttached());
    REQUIRE(!handlerB.IsAttached());
    event.Signal();
}
TEST_CASE("Event copy/move constructors and assignment operators", "[Event]")
{
    u32 numberOfCalls = 0;
    Event event;
    EventHandler<> handlerA([&numberOfCalls](){ numberOfCalls++; });
    EventHandler<> handlerB([&numberOfCalls](){ numberOfCalls++; });
    handlerA.Connect(event);
    handlerB.Connect(event);

    SECTION("Copy constructed Event inherits all handlers and keeps original valid")
    {
        Event copy(event);
        REQUIRE(numberOfCalls == 0);
        copy.Signal();
        REQUIRE(numberOfCalls == 2);
        event.Signal();
        REQUIRE(numberOfCalls == 4);
    }
    SECTION("Copy assigned Event inherits all handlers and keeps original valid")
    {
        Event copy = {};
        copy = event;
        REQUIRE(numberOfCalls == 0);
        copy.Signal();
        REQUIRE(numberOfCalls == 2);
        event.Signal();
        REQUIRE(numberOfCalls == 4);
    }
    SECTION("Move constructed Event inherits all handlers and keeps original empty")
    {
        Event move(std::move(event));
        REQUIRE(numberOfCalls == 0);
        move.Signal();
        REQUIRE(numberOfCalls == 2);
        event.Signal();
        REQUIRE(numberOfCalls == 2);
    }
    
    SECTION("Move assigned Event inherits all handlers and keeps original empty")
    {
        Event move = {};
        move = std::move(event);
        REQUIRE(numberOfCalls == 0);
        move.Signal();
        REQUIRE(numberOfCalls == 2);
        event.Signal();
        REQUIRE(numberOfCalls == 2);
    }
    
    SECTION("Self copy assignment operator returns self")
    {
        event = event;
        event.Signal();
        REQUIRE(numberOfCalls == 2);
    }
    SECTION("Self move assignment operator returns self")
    {
        event = std::move(event);
        event.Signal();
        REQUIRE(numberOfCalls == 2);
    }
}
TEST_CASE("Event transfer of handlers transfers and leaves original empty", "[Event]")
{
    u32 numberOfCalls = 0;
    Event event;
    EventHandler<> handlerA([&numberOfCalls](){ numberOfCalls++; });
    EventHandler<> handlerB([&numberOfCalls](){ numberOfCalls++; });
    handlerA.Connect(event);
    handlerB.Connect(event);

    Event other;
    event.TransferHandlersTo(other);
    REQUIRE(numberOfCalls == 0);
    event.Signal();
    REQUIRE(numberOfCalls == 0);
    other.Signal();
    REQUIRE(numberOfCalls == 2);
}

// NOLINTEND