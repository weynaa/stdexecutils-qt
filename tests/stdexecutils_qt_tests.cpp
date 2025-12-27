#include <QCoreApplication>
#include <exec/async_scope.hpp>
#include <exec/timed_thread_scheduler.hpp>
#include <exec/when_any.hpp>
#include <gtest/gtest.h>
#include <stdexecutils/qt/qthread_scheduler.hpp>
#include <stdexecutils/qt/qthreadpool_scheduler.hpp>
#include <thread>

using namespace stdexecutils::qt;

using namespace std::chrono_literals;

static_assert(stdexec::scheduler<QThreadScheduler>,
              "scheduler is not fulfilling the concept");

TEST(QThreadScheduler, BasicSchedulingContinuation) {
	int              argc = 0;
	QCoreApplication application(argc, nullptr);

	QThreadScheduler scheduler(&application);

	const auto        tid = std::this_thread::get_id();
	exec::async_scope scope;
	scope.spawn(stdexec::schedule(scheduler) | stdexec::then([&]() {
		            // continuation in the qthread event loop
		            EXPECT_EQ(tid, std::this_thread::get_id());
		            application.exit();
	            }));

	application.exec();
}

TEST(QThreadScheduler, BasicSchedulingErrorContinuation) {

	int              argc = 0;
	QCoreApplication application(argc, nullptr);

	QThreadScheduler  scheduler(&application);
	const auto        tid = std::this_thread::get_id();
	exec::async_scope scope;
	scope.spawn(stdexec::schedule(scheduler) |
	            stdexec::then([]() { throw std::runtime_error("test"); }) |
	            stdexec::upon_error([&](std::exception_ptr exception) {
		            // continuation in the qthread event loop
		            EXPECT_EQ(tid, std::this_thread::get_id());
		            application.exit();
	            }));
	application.exec();
}

TEST(QThreadScheduler, BasicSchedulingStopped) {

	int              argc = 0;
	QCoreApplication application(argc, nullptr);

	QThreadScheduler  scheduler(&application);
	const auto        tid = std::this_thread::get_id();
	exec::async_scope scope;
	scope.spawn(stdexec::schedule(scheduler) |
	            stdexec::then([]() { throw std::runtime_error("test"); }) |
	            stdexec::upon_stopped([&]() {
		            // continuation in the qthread event loop
		            EXPECT_EQ(tid, std::this_thread::get_id());
		            application.exit();
	            }));
	scope.request_stop();
	application.exec();
}

TEST(QThreadScheduler, ScheduleAt) {

	int              argc = 0;
	QCoreApplication application(argc, nullptr);

	QThreadScheduler scheduler(&application);

	constexpr auto duration = 500ms;
	const auto     now      = std::chrono::system_clock::now();
	const auto     deadline = now + duration;

	static_assert(stdexec::sender<decltype(scheduler.schedule_at(deadline))>,
	              "schedule_at is not a sender");

	exec::async_scope scope;
	scope.spawn(scheduler.schedule_at(deadline) | stdexec::then([&]() {
		            // continuation in the qthread event loop
		            EXPECT_GT((std::chrono::system_clock::now() - now).count(),
		                      duration.count() * 0.9);
		            application.exit();
	            }));
	application.exec();
}

TEST(QThreadScheduler, ScheduleAtStopped) {

	int              argc = 0;
	QCoreApplication application(argc, nullptr);

	QThreadScheduler scheduler(&application);

	constexpr auto duration = 10s;
	const auto     now      = std::chrono::system_clock::now();
	const auto     deadline = now + duration;

	static_assert(stdexec::sender<decltype(scheduler.schedule_at(deadline))>,
	              "schedule_at is not a sender");

	exec::async_scope scope;
	scope.spawn(scheduler.schedule_at(deadline)                     //
	            | stdexec::then([&]() { FAIL() << "not stopped"; }) //
	            |
	            stdexec::upon_stopped([&]() {
		            EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(
		                          std::chrono::system_clock::now() - now)
		                          .count(),
		                      std::chrono::duration_cast<std::chrono::milliseconds>(
		                          duration)
		                              .count() *
		                          0.1);
		            application.exit();
	            }));
	scope.spawn(scheduler.schedule_after(100ms) |
	            stdexec::then([&]() { scope.request_stop(); }));
	application.exec();
}

TEST(QThreadScheduler, ScheduleAfter) {

	int              argc = 0;
	QCoreApplication application(argc, nullptr);

	QThreadScheduler scheduler(&application);
	static_assert(stdexec::sender<decltype(scheduler.schedule())>,
	              "scheduler is not a sender");
	constexpr auto duration = 500ms;
	const auto     now      = std::chrono::system_clock::now();

	static_assert(stdexec::sender<decltype(scheduler.schedule_after(duration))>,
	              "schedule_after is not a sender");

	exec::async_scope scope;
	scope.spawn(scheduler.schedule_after(duration) | stdexec::then([&]() {
		            // continuation in the qthread event loop
		            EXPECT_GT((std::chrono::system_clock::now() - now).count(),
		                      duration.count() * 0.9);
		            application.exit();
	            }));
	application.exec();
}

TEST(QThreadScheduler, ScheduleAfterStopped) {
	int              argc = 0;
	QCoreApplication application(argc, nullptr);

	QThreadScheduler scheduler(&application);

	constexpr auto duration = 10s;
	const auto     now      = std::chrono::system_clock::now();

	static_assert(stdexec::sender<decltype(scheduler.schedule_after(duration))>,
	              "schedule_at is not a sender");

	exec::async_scope scope;
	scope.spawn(scheduler.schedule_after(duration)                  //
	            | stdexec::then([&]() { FAIL() << "not stopped"; }) //
	            |
	            stdexec::upon_stopped([&]() {
		            EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(
		                          std::chrono::system_clock::now() - now)
		                          .count(),
		                      std::chrono::duration_cast<std::chrono::milliseconds>(
		                          duration)
		                              .count() *
		                          0.1);
		            application.exit();
	            }));
	scope.spawn(scheduler.schedule_after(100ms) |
	            stdexec::then([&]() { scope.request_stop(); }));
	application.exec();
}

TEST(QThreadScheduler, StoppedWhenThreadStopped) {
	exec::async_scope scope;
	{
		int              argc = 0;
		QCoreApplication application(argc, nullptr);

		QThreadScheduler scheduler(&application);

		constexpr auto duration = 10s;
		const auto     now      = std::chrono::system_clock::now();

		scope.spawn(
		    scheduler.schedule_after(duration)                  //
		    | stdexec::then([&]() { FAIL() << "not stopped"; }) //
		    | stdexec::upon_stopped([&]() {
			      EXPECT_LT(
			          std::chrono::duration_cast<std::chrono::milliseconds>(
			              std::chrono::system_clock::now() - now)
			              .count(),
			          std::chrono::duration_cast<std::chrono::milliseconds>(duration)
			                  .count() *
			              0.1);
			      application.exit();
		      }));
		scope.spawn(scheduler.schedule_after(100ms) |
		            stdexec::then([&]() { application.exit(42); }));
		const auto return_code = application.exec();
		EXPECT_EQ(return_code, 42);
	}
	exec::timed_thread_context timer_thread;

	const auto result = stdexec::sync_wait(
	    exec::when_any(scope.on_empty() | stdexec::then([]() { return true; }),
	                   exec::schedule_after(timer_thread.get_scheduler(), 2s) |
	                       stdexec::then([]() { return false; })));
	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(std::get<0>(*result));
}

TEST(ThreadpoolScheduler, BasicWorks) {
	QThreadPool threadpool;

	const auto opt_tid = stdexec::sync_wait(
	    stdexec::schedule(qthread_scheduler(&threadpool)) |
	    stdexec::then([]() { return std::this_thread::get_id(); }));
	ASSERT_TRUE(opt_tid.has_value());
	EXPECT_NE(std::get<0>(*opt_tid), std::this_thread::get_id());
}

TEST(ThreadpoolScheduler, BasicStops) {
	exec::async_scope scope;
	scope.request_stop();

	bool        stopped{false};
	QThreadPool pool;
	scope.spawn(stdexec::schedule(qthread_scheduler(&pool)) |
	            stdexec::upon_stopped([&]() { stopped = true; }));

	stdexec::sync_wait(scope.on_empty());
	EXPECT_TRUE(stopped);
}
