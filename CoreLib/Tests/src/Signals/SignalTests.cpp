#include "catch2/catch_test_macros.hpp"

#include "Signals/Signal.h"

// NOLINTBEGIN

TEST_CASE("SignalHandler can be attached", "[SignalHandler]")
{
    Signal signal;
    SignalHandler<> handler([](){});
    handler.Connect(signal);
    REQUIRE(handler.IsAttached());
}
TEST_CASE("SignalHandler callback is called", "[SignalHandler]")
{
    Signal signal;
    SECTION("Single SignalHandler is signalled")
    {
        SignalHandler<> handler([](){ REQUIRE(true); });
        handler.Connect(signal);
        signal.Emit();
    }
    SECTION("Multiple EventHandlers are signalled")
    {
        SignalHandler<> handlerA([](){ REQUIRE(true); });
        SignalHandler<> handlerB([](){ REQUIRE(true); });
        handlerA.Connect(signal);
        handlerB.Connect(signal);
        signal.Emit();
    }
}
TEST_CASE("SignalHandler callback receives correct argument", "[SignalHandler]")
{
    Signal<u32> signal;
    SECTION("Passed `0` as argument")
    {
        SignalHandler<u32> handler([](u32 val){ REQUIRE(val == 0); });
        handler.Connect(signal);
        signal.Emit(0);
    }
    SECTION("Passed `42` as argument")
    {
        SignalHandler<u32> handler([](u32 val){ REQUIRE(val == 42); });
        handler.Connect(signal);
        signal.Emit(42);
    }
    SECTION("Passed `42` as argument and capture")
    {
        constexpr u32 capture = 42;
        SignalHandler<u32> handler([capture](u32 val){ REQUIRE(val == 42); REQUIRE(capture == 42); });
        handler.Connect(signal);
        signal.Emit(42);
    }
}
TEST_CASE("SignalHandler added during Emit call", "[SignalHandler][Emit]")
{
    Signal signal;
    bool expectCall = false;
    bool doConnect = true;
    SignalHandler<> subHandler([&expectCall](){ REQUIRE(expectCall); });
    SignalHandler<> handler([&signal, &subHandler, &doConnect]()
    {
        if (doConnect)
            subHandler.Connect(signal);
    });
    handler.Connect(signal);
    
    SECTION("Callback is not executed on first call to Emit")
    {
        expectCall = false;
        signal.Emit();
    }
    SECTION("Callback is executed on second call to Emit")
    {
        signal.Emit();
        expectCall = true;
        doConnect = false;
        signal.Emit();
    }
}
TEST_CASE("SignalHandler disconnects on destruction", "[SignalHandler]")
{
    Signal signal;
    {
        SignalHandler<> handler([](){ REQUIRE(false); });
        handler.Connect(signal);
    }
    signal.Emit();
}
TEST_CASE("SignalHandler copy/move constructors and assignment operators", "[SignalHandler]")
{
    u32 numberOfCalls = 0;
    Signal<u32> signal;
    
    SECTION("If original has no signal, constructed copy callback is not called")
    {
        SignalHandler<u32> original([](u32 val){ REQUIRE(false); });
        SignalHandler copy(original);
        REQUIRE(!copy.IsAttached());
        signal.Emit(0);
    }
    SECTION("If original has no signal, assigned copy callback is not called")
    {
        SignalHandler<u32> original([](u32 val){ REQUIRE(false); });
        SignalHandler<u32> copy = {};
        copy = original;
        REQUIRE(!copy.IsAttached());
        signal.Emit(0);
    }
    
    SignalHandler<u32> handler([&numberOfCalls](u32 val)
    {
        REQUIRE(val == 42);
        numberOfCalls++;
    });
    handler.Connect(signal);
    SECTION("Copy constructed SignalHandler inherits signal")
    {
        SignalHandler<u32> copy(handler);
        REQUIRE(copy.IsAttached());
        signal.Emit(42);
        REQUIRE(numberOfCalls == 2);
    }
    SECTION("Copy assigned SignalHandler inherits signal")
    {
        SignalHandler<u32> copy = {};
        copy = handler;
        REQUIRE(copy.IsAttached());
        signal.Emit(42);
        REQUIRE(numberOfCalls == 2);
    }
    SECTION("Move constructed SignalHandler inherits signal, original SignalHandler is not called")
    {
        SignalHandler<u32> move(std::move(handler));
        REQUIRE(move.IsAttached());
        signal.Emit(42);
        REQUIRE(numberOfCalls == 1);
    }
    SECTION("Move assigned SignalHandler inherits signal, original SignalHandler is not called")
    {
        SignalHandler<u32> move = {};
        move = std::move(handler);
        REQUIRE(move.IsAttached());
        signal.Emit(42);
        REQUIRE(numberOfCalls == 1);
    }
    SECTION("Self copy assignment operator returns self")
    {
        handler = handler;
        REQUIRE(handler.IsAttached());
        signal.Emit(42);
        REQUIRE(numberOfCalls == 1);
    }
    SECTION("Self move assignment operator returns self")
    {
        handler = std::move(handler);
        REQUIRE(handler.IsAttached());
        signal.Emit(42);
        REQUIRE(numberOfCalls == 1);
    }
}
TEST_CASE("Signal can clear all handlers", "[Signal]")
{
    bool expectCall = true;
    Signal signal;
    SignalHandler<> handlerA([&expectCall](){ REQUIRE(expectCall); });
    SignalHandler<> handlerB([&expectCall](){ REQUIRE(expectCall); });
    handlerA.Connect(signal);
    handlerB.Connect(signal);
    REQUIRE(handlerA.IsAttached());
    REQUIRE(handlerB.IsAttached());
    signal.Emit();

    expectCall = false;
    signal.DisconnectHandlers();
    REQUIRE(!handlerA.IsAttached());
    REQUIRE(!handlerB.IsAttached());
    signal.Emit();
}
TEST_CASE("Signal copy/move constructors and assignment operators", "[Signal]")
{
    u32 numberOfCalls = 0;
    Signal signal;
    SignalHandler<> handlerA([&numberOfCalls](){ numberOfCalls++; });
    SignalHandler<> handlerB([&numberOfCalls](){ numberOfCalls++; });
    handlerA.Connect(signal);
    handlerB.Connect(signal);

    SECTION("Copy constructed Signal inherits all handlers and keeps original valid")
    {
        Signal copy(signal);
        REQUIRE(numberOfCalls == 0);
        copy.Emit();
        REQUIRE(numberOfCalls == 2);
        signal.Emit();
        REQUIRE(numberOfCalls == 4);
    }
    SECTION("Copy assigned Signal inherits all handlers and keeps original valid")
    {
        Signal copy = {};
        copy = signal;
        REQUIRE(numberOfCalls == 0);
        copy.Emit();
        REQUIRE(numberOfCalls == 2);
        signal.Emit();
        REQUIRE(numberOfCalls == 4);
    }
    SECTION("Move constructed Signal inherits all handlers and keeps original empty")
    {
        Signal move(std::move(signal));
        REQUIRE(numberOfCalls == 0);
        move.Emit();
        REQUIRE(numberOfCalls == 2);
        signal.Emit();
        REQUIRE(numberOfCalls == 2);
    }
    
    SECTION("Move assigned Signal inherits all handlers and keeps original empty")
    {
        Signal move = {};
        move = std::move(signal);
        REQUIRE(numberOfCalls == 0);
        move.Emit();
        REQUIRE(numberOfCalls == 2);
        signal.Emit();
        REQUIRE(numberOfCalls == 2);
    }
    
    SECTION("Self copy assignment operator returns self")
    {
        signal = signal;
        signal.Emit();
        REQUIRE(numberOfCalls == 2);
    }
    SECTION("Self move assignment operator returns self")
    {
        signal = std::move(signal);
        signal.Emit();
        REQUIRE(numberOfCalls == 2);
    }
}
TEST_CASE("Signal transfer of handlers transfers and leaves original empty", "[Signal]")
{
    u32 numberOfCalls = 0;
    Signal signal;
    SignalHandler<> handlerA([&numberOfCalls](){ numberOfCalls++; });
    SignalHandler<> handlerB([&numberOfCalls](){ numberOfCalls++; });
    handlerA.Connect(signal);
    handlerB.Connect(signal);

    Signal other;
    signal.TransferHandlersTo(other);
    REQUIRE(numberOfCalls == 0);
    signal.Emit();
    REQUIRE(numberOfCalls == 0);
    other.Emit();
    REQUIRE(numberOfCalls == 2);
}

// NOLINTEND