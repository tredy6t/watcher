/*
   Test Watcher
   New Directories
*/

/* REQUIRE,
   TEST_CASE */
#include <snitch/snitch.hpp>
/* event */
#include <wtr/watcher.hpp>
/* watch_gather */
#include <test_watcher/test_watcher.hpp>
/* get */
#include <tuple>
/* cout, endl */
#include <iostream>
/* async,
   future,
   promise */
#include <future>
/* this_thread */
#include <thread>
/* vector */
#include <vector>
/* string */
#include <string>
/* milliseconds */
#include <chrono>
/* mutex */
#include <mutex>
/* path,
   create,
   remove_all */
#include <filesystem>

/* Test that files are scanned */
TEST_CASE("New Directories", "[new_directories]")
{
  namespace fs = ::std::filesystem;
  using namespace ::wtr::watcher;
  using namespace ::wtr::test_watcher;

  static constexpr auto path_count = 10;
  static constexpr auto title = "New Directories";
  static auto event_recv_list = std::vector<event>{};
  static auto event_recv_list_mtx = std::mutex{};
  static auto event_sent_list = std::vector<event>{};
  static auto watch_path_list = std::vector<fs::path>{};
  auto const base_store_path = test_store_path;
  auto const store_path_first = base_store_path / "new_directories_first_store";
  auto const store_path_second =
    base_store_path / "new_directories_second_store";
  auto const store_path_list =
    std::vector<fs::path>{base_store_path, store_path_first, store_path_second};
  auto const new_store_path_list =
    std::vector<fs::path>{store_path_first, store_path_second};

  std::cout << title << std::endl;

  fs::create_directory(base_store_path);

  /* @todo
     This sleep is hiding a bug on darwin which picks
     up events slightly before we start watching. I'm
     ok with that bit of wiggle-room. */
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  event_sent_list.push_back(
    {std::string("s/self/live@").append(base_store_path.string()),
     event::what::create,
     event::kind::watcher});

  auto lifetime = watch(base_store_path,
                        [&](event const& ev)
                        {
                          auto _ = std::scoped_lock{event_recv_list_mtx};
                          std::cout << ev << std::endl;
                          event_recv_list.push_back(ev);
                        });

  /* @todo
     This sleep is hiding a bug on Linux which begins
     watching very slightly after we ask it to. */
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  for (auto const& p : new_store_path_list) {
    fs::create_directory(p);
    event_sent_list.push_back(event{p, event::what::create, event::kind::dir});
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  for (int i = 0; i < path_count; i++) {
    for (auto const& p : store_path_list) {
      auto const new_dir_path = p / ("dir" + std::to_string(i));
      fs::create_directory(new_dir_path);
      REQUIRE(fs::exists(new_dir_path));
      event_sent_list.push_back(
        event{new_dir_path, event::what::create, event::kind::dir});

      /* @todo
         This sleep is hiding a bug for the Linux adapters.
         It might on either our end -- something about how
         we're processing batches of events -- or it could
         be on the kernel's end... just dropping events.
         We should investigate and fix it if we can. */
      std::this_thread::sleep_for(std::chrono::milliseconds(10));

      auto const new_file_path = new_dir_path / "file.txt";
      std::ofstream{new_file_path}; /* NOLINT */
      REQUIRE(fs::exists(new_file_path));
      event_sent_list.push_back(
        event{new_file_path, event::what::create, event::kind::file});

      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  event_sent_list.push_back(
    {std::string("s/self/die@").append(base_store_path.string()),
     event::what::destroy,
     event::kind::watcher});

  auto dead = lifetime.close();

  REQUIRE(dead);

  fs::remove_all(base_store_path);
  REQUIRE(! fs::exists(base_store_path));

  auto const max_i = event_sent_list.size() > event_recv_list.size()
                     ? event_recv_list.size()
                     : event_sent_list.size();
  for (size_t i = 0; i < max_i; ++i) {
    if (event_sent_list[i].kind != event::kind::watcher) {
      if (event_sent_list[i].where != event_recv_list[i].where)
        std::cout << "[ where ] [ " << i << " ] sent "
                  << event_sent_list[i].where << ", but received "
                  << event_recv_list[i].where << "\n";
      if (event_sent_list[i].what != event_recv_list[i].what)
        std::cout << "[ what ] [ " << i << " ] sent " << event_sent_list[i].what
                  << ", but received " << event_recv_list[i].what << "\n";
      if (event_sent_list[i].kind != event_recv_list[i].kind)
        std::cout << "[ kind ] [ " << i << " ] sent " << event_sent_list[i].kind
                  << ", but received " << event_recv_list[i].kind << "\n";
      REQUIRE(event_sent_list[i].where == event_recv_list[i].where);
      REQUIRE(event_sent_list[i].what == event_recv_list[i].what);
      REQUIRE(event_sent_list[i].kind == event_recv_list[i].kind);
    }
  }

  REQUIRE(event_sent_list.size() == event_recv_list.size());
};
