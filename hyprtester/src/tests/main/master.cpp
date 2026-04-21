#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"
#include <algorithm>

static int ret = 0;

// reqs 1 master 3 slaves
static void testOrientations() {
    OK(getFromSocket("/keyword master:orientation top"));

    // top
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876");
    }

    // cycle = top, right, bottom, center, left

    // right
    OK(getFromSocket("/dispatch layoutmsg orientationnext"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 873,22");
        EXPECT_CONTAINS(str, "size: 1025,1036");
    }

    // bottom
    OK(getFromSocket("/dispatch layoutmsg orientationnext"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,495");
        EXPECT_CONTAINS(str, "size: 1876");
    }

    // center
    OK(getFromSocket("/dispatch layoutmsg orientationnext"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 450,22");
        EXPECT_CONTAINS(str, "size: 1020,1036");
    }

    // left
    OK(getFromSocket("/dispatch layoutmsg orientationnext"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1025,1036");
    }
}

static void focusMasterPrevious() {
    // setup
    NLog::log("{}Spawning 1 master and 3 slave windows", Colors::YELLOW);
    // order of windows set according to new_status = master (set in test.conf)
    for (auto const& win : {"slave1", "slave2", "slave3", "master"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with win class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }
    NLog::log("{}Ensuring focus is on master before testing", Colors::YELLOW);
    OK(getFromSocket("/dispatch layoutmsg focusmaster master"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: master");

    // test
    NLog::log("{}Testing fallback to focusmaster auto", Colors::YELLOW);

    OK(getFromSocket("/dispatch layoutmsg focusmaster previous"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: slave1");

    NLog::log("{}Testing focusing from slave to master", Colors::YELLOW);

    OK(getFromSocket("/dispatch layoutmsg cyclenext noloop"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: slave2");
    OK(getFromSocket("/dispatch layoutmsg focusmaster previous"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: master");

    NLog::log("{}Testing focusing on previous window", Colors::YELLOW);

    OK(getFromSocket("/dispatch layoutmsg focusmaster previous"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: slave2");

    NLog::log("{}Testing focusing back to master", Colors::YELLOW);

    OK(getFromSocket("/dispatch layoutmsg focusmaster previous"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: master");

    testOrientations();

    // clean up
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

static void testFsBehavior() {
    // Master will re-send data to fullscreen / maximized windows, which can interfere with misc:on_focus_under_fullscreen
    // check that it doesn't.

    for (auto const& win : {"master", "slave1", "slave2"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with win class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    OK(getFromSocket("/dispatch focuswindow class:master"));
    OK(getFromSocket("/dispatch fullscreen 1"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
        EXPECT_CONTAINS(str, "class: master");
    }

    OK(getFromSocket("/keyword misc:on_focus_under_fullscreen 1"));

    Tests::spawnKitty("new_master");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
        EXPECT_CONTAINS(str, "class: new_master");
        EXPECT_CONTAINS(str, "fullscreen: 1");
    }

    OK(getFromSocket("/keyword misc:on_focus_under_fullscreen 0"));

    Tests::spawnKitty("ignored");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
        EXPECT_CONTAINS(str, "class: new_master");
        EXPECT_CONTAINS(str, "fullscreen: 1");
    }

    OK(getFromSocket("/keyword misc:on_focus_under_fullscreen 2"));

    Tests::spawnKitty("vaxwashere");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: vaxwashere");
        EXPECT_CONTAINS(str, "fullscreen: 0");
    }

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

static void testRollFocus() {
    // set up windows
    std::vector<std::string> windows = {"slave1", "slave2", "slave3", "master"};

    // helper lambda thing
    auto roll = [&](const std::string& dir) {
        auto pivot = (dir == "rollnext") ? windows.begin() + 1 : windows.end() - 1;

        // rotate the windows vector along with the actual windows
        // the rolling behavior of the window focus should follow the
        // rotating behavior of std::ranges::rotate
        OK(getFromSocket("/dispatch layoutmsg " + dir));
        std::ranges::rotate(windows.begin(), pivot, windows.end());
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: " + windows.back());
    };

    for (auto const& win : windows) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with win class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    // focus master
    OK(getFromSocket("/dispatch layoutmsg focusmaster master"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: master");

    // put the windows in the washing machine
    NLog::log("{}Testing rollnext", Colors::YELLOW);
    for (int i = 0; i < 20; ++i) {
        roll("rollnext");
    }

    NLog::log("{}Testing rollprev", Colors::YELLOW);
    for (int i = 0; i < 20; ++i) {
        roll("rollprev");
    }


    NLog::log("{}Testing rollnext with rollprev", Colors::YELLOW);
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 5; ++j) {
            roll("rollnext");
        }
        roll("rollprev");
    }

    NLog::log("{}Testing rollnext/rollprev alternation", Colors::YELLOW);
    for (int i = 0; i < 20; ++i) {
        if (i % 2 == 0) {
            roll("rollnext");
        } else {
            roll("rollprev");
        }
    }

    NLog::log("{}Testing rollnext/rollprev burst calls", Colors::YELLOW);
    for (int i = 0; i < 20; ++i) {
        if (i / 5 % 2 == 0) {
            roll("rollnext");
        } else {
            roll("rollprev");
        }
    }

    // clean up
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

static bool test() {
    NLog::log("{}Testing Master layout", Colors::GREEN);

    // setup
    OK(getFromSocket("/dispatch workspace name:master"));
    OK(getFromSocket("/keyword general:layout master"));

    // test
    NLog::log("{}Testing `focusmaster previous` layoutmsg", Colors::GREEN);
    focusMasterPrevious();

    NLog::log("{}Testing fs behavior", Colors::GREEN);
    testFsBehavior();

    NLog::log("{}Testing rollnext and rollprev", Colors::GREEN);
    testRollFocus();

    // clean up
    NLog::log("Cleaning up", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 1"));
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test);
