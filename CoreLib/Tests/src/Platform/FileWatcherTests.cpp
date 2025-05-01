#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"

#include <fstream>
#include <thread>

#include "Platform/FileWatcher.h"

// NOLINTBEGIN

TEST_CASE("Filewatcher", "[Platform][FileWatcher]")
{
    namespace fs = std::filesystem;
    fs::path testDir = fs::temp_directory_path();
    testDir /= "FileWatcherTests";
    fs::create_directory(testDir);
    
    FileWatcher watcher;
    
    SECTION("Filewatcher can watch exisiting directories")
    {
        auto res = watcher.Watch(testDir);
        REQUIRE(res.has_value());
    }    
    SECTION("Filewatcher can unwatch watched directory")
    {
        auto res = watcher.Watch(testDir);
        REQUIRE(res.has_value());
        auto unwatchRes = watcher.StopWatching();
        REQUIRE(unwatchRes.has_value());
        unwatchRes = watcher.StopWatching();
        REQUIRE((!unwatchRes.has_value() && unwatchRes.error() == FileWatcherError::NotWatchingDirectory));
    }
    SECTION("Filewatcher Watch() results in error for nonexisting dir")
    {
        fs::path nonexisting = testDir / "temp";
        if (fs::exists(nonexisting))
            fs::remove_all(nonexisting);

        auto res = watcher.Watch(nonexisting);
        REQUIRE((!res.has_value() && res.error() == FileWatcherWatchError::FileNotFound));
        auto unwatchRes = watcher.StopWatching();
        REQUIRE((!unwatchRes.has_value() && unwatchRes.error() == FileWatcherError::NotWatchingDirectory));
    }
    SECTION("Filewatcher Watch() results in error if already watching")
    {
        auto res = watcher.Watch(testDir);
        res = watcher.Watch(testDir);
        REQUIRE((!res.has_value() && res.error() == FileWatcherWatchError::AlreadyWatching));
        auto unwatchRes = watcher.StopWatching();
        res = watcher.Watch(testDir);
        REQUIRE(unwatchRes.has_value());
    }
    SECTION("Filewatcher Subscribe results in error in no directory is watched")
    {
        FileWatcherHandler handler([](const FileWatcherEvent&) {});
        auto res = watcher.Subscribe(handler);
        REQUIRE((!res.has_value() && res.error() == FileWatcherError::NotWatchingDirectory));
    }
    SECTION("Filewatcher signals when file created")
    {
        bool called = false;
        fs::path toCreate = testDir / "temp";
        watcher.Watch(testDir);
        FileWatcherHandler handler([&called, &toCreate](const FileWatcherEvent& file)
        {
            if (called)
                return;

            if (file.Action == FileWatcherEvent::ActionType::Create)
                called = file.Name == toCreate;
        });
        if (fs::exists(toCreate))
            fs::remove_all(toCreate);
        watcher.Subscribe(handler);
        fs::create_directory(testDir / "temp");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        REQUIRE(called);
    }
    SECTION("Filewatcher signals when file deleted")
    {
        bool called = false;
        fs::path existing = testDir / "temp";
        watcher.Watch(testDir);
        FileWatcherHandler handler([&called, &existing](const FileWatcherEvent& file)
        {
            if (called)
                return;

            if (file.Action == FileWatcherEvent::ActionType::Delete)
                called = file.Name == existing;
        });
        if (!fs::exists(existing))
            fs::create_directory(existing);
        watcher.Subscribe(handler);
        fs::remove_all(testDir / "temp");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        REQUIRE(called);
    }
    SECTION("Filewatcher signals when file modified")
    {
        bool called = false;
        fs::path existing = testDir / "text.txt";
        watcher.Watch(testDir);
        FileWatcherHandler handler([&called, &existing](const FileWatcherEvent& file)
        {
            if (called)
                return;

            if (file.Action == FileWatcherEvent::ActionType::Modify)
                called = file.Name == existing;
        });
        std::ofstream out(existing);
        out << "t";
        watcher.Subscribe(handler);
        out << "t";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        REQUIRE(called);
    }
    SECTION("Filewatcher signals when file renamed")
    {
        bool called = false;
        fs::path existing = testDir / "temp";
        fs::path renamed = testDir / "temp2";
        watcher.Watch(testDir);
        FileWatcherHandler handler([&called, &existing, &renamed](const FileWatcherEvent& file)
        {
            if (called)
                return;

            if (file.Action == FileWatcherEvent::ActionType::Rename)
                called = file.Name == renamed && file.OldName == existing;
        });
        if (!fs::exists(existing))
            fs::create_directory(existing);
        if (fs::exists(renamed))
            fs::remove_all(renamed);
        watcher.Subscribe(handler);
        fs::rename(existing, testDir / "temp2");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        REQUIRE(called);
    }
    SECTION("Filewatcher signals are debounced")
    {
        u32 timesCalled = 0;
        fs::path existing = testDir / "text.txt";
        watcher.Watch(testDir);
        FileWatcherHandler handler([&timesCalled, &existing](const FileWatcherEvent& file)
        {
            if (file.Action == FileWatcherEvent::ActionType::Modify && file.Name == existing)
                timesCalled++;
        });
        {
            std::ofstream out(existing);
            out << "t";
        }
        
        watcher.Subscribe(handler);
        for (u32 i = 0; i < 1'000; i++)
        {
            std::ofstream out(existing, std::ios::app);
            out << i << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        REQUIRE(timesCalled == 1);
    }
    SECTION("Filewatcher different files are debounced independently")
    {
        u32 timesCalledA = 0;
        u32 timesCalledB = 0;
        fs::path existingA = testDir / "text.txt";
        fs::path existingB = testDir / "text2.txt";
        watcher.Watch(testDir);
        FileWatcherHandler handler([&timesCalledA, &timesCalledB, &existingA, &existingB]
            (const FileWatcherEvent& file)
        {
            if (file.Action == FileWatcherEvent::ActionType::Modify && file.Name == existingA)
                timesCalledA++;
            else if (file.Action == FileWatcherEvent::ActionType::Modify && file.Name == existingB)
                timesCalledB++;
        });
        {
            std::ofstream outA(existingA, std::ios::app);
            std::ofstream outB(existingB, std::ios::app);
            outA << "t";
            outB << "t";
        }
        
        watcher.Subscribe(handler);
        for (u32 i = 0; i < 1'000; i++)
        {
            std::ofstream outA(existingA, std::ios::app);
            std::ofstream outB(existingB, std::ios::app);
            outA << i << std::endl;
            outB << i << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        REQUIRE(timesCalledA == 1);
        REQUIRE(timesCalledB == 1);
    }
    SECTION("Filewatcher debounce has correct duration")
    {
        u32 timesCalled = 0;
        fs::path existing = testDir / "text.txt";
        watcher.Watch(testDir);
        FileWatcherHandler handler([&timesCalled, &existing](const FileWatcherEvent& file)
        {
            if (file.Action == FileWatcherEvent::ActionType::Modify && file.Name == existing)
                timesCalled++;
        });
        {
            std::ofstream out(existing);
            out << "t";
        }
        
        watcher.Subscribe(handler);
        {
            std::ofstream out(existing, std::ios::app);
            out << "t" << std::endl;
        }
        std::this_thread::sleep_for(FileWatcherSettings::DEFAULT_DEBOUNCE_DURATION * 1.5);
        {
            std::ofstream out(existing, std::ios::app);
            out << "t" << std::endl;
        }
        std::this_thread::sleep_for(FileWatcherSettings::DEFAULT_DEBOUNCE_DURATION * 1.5);
        REQUIRE(timesCalled == 2);

        timesCalled = 0;

        {
            std::ofstream out(existing, std::ios::app);
            out << "t" << std::endl;
        }
        std::this_thread::sleep_for(FileWatcherSettings::DEFAULT_DEBOUNCE_DURATION * 0.5);
        {
            std::ofstream out(existing, std::ios::app);
            out << "t" << std::endl;
        }
        std::this_thread::sleep_for(FileWatcherSettings::DEFAULT_DEBOUNCE_DURATION * 1.5);
        REQUIRE(timesCalled == 1);
    }
    SECTION("Filewatcher handler does not have to outlive filewath")
    {
        bool called = false;
        fs::path existing = testDir / "text.txt";

        watcher.Watch(testDir, {.DebounceDuration = std::chrono::milliseconds(5)});
        {
            FileWatcherHandler handler([&called, &existing](const FileWatcherEvent& file)
            {
                called = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            });

            watcher.Subscribe(handler);
            {
                std::ofstream out(existing);
                out << "t";
            }
            std::this_thread::sleep_for(FileWatcherSettings::DEFAULT_DEBOUNCE_DURATION * 1.5);
        }
        REQUIRE(called);
    }
}

// NOLINTEND