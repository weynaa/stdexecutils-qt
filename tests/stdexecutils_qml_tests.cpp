#include <stdexecutils/qt/qml_receiver.hpp>

#ifndef Q_MOC_RUN
#include <exec/task.hpp>
#endif

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QJSEngine>
#include <QtQml>

using namespace stdexecutils::qt;

class LaunchFunctions : public QObject {
	Q_OBJECT
public:
	using QObject::QObject;
	Q_INVOKABLE QmlReceiver* startVoidValue() {
		return new QmlReceiver(stdexec::just());
	}

	Q_INVOKABLE QmlReceiver* startStringValue() {
		return new QmlReceiver(stdexec::just(std::string{"test123456"}));
	}
	Q_INVOKABLE QmlReceiver* startTwoValues() {
		return new QmlReceiver(stdexec::when_all(
		    stdexec::just(std::string{"test123456"}), stdexec::just(42)));
	}
	Q_INVOKABLE QmlReceiver* startException() {
		return new QmlReceiver([=]() -> exec::task<void> {
			co_await QThreadScheduler(this).schedule();
			throw std::runtime_error("error");
		}());
	}
	Q_INVOKABLE QmlReceiver* startStopped() {
		return new QmlReceiver(stdexec::just_stopped());
	}
	Q_INVOKABLE QmlReceiver* startDelay() {
		return new QmlReceiver(QThreadScheduler(this).schedule_after(std::chrono::seconds(1)));
	}
public slots:
	void success(bool success) { //
		ASSERT_TRUE(success);
		QCoreApplication::exit();
	}
	void failure() { //
		FAIL();
	}
};
static int argc = 0;
class QMLTestFixture : public testing::Test {
public:
	QMLTestFixture() : application(argc, nullptr), engine(&application) {

		QmlReceiver::registerMetatype("TestModule", 1, 0);
		auto type2 = qmlRegisterUncreatableType<LaunchFunctions>("TestModule", 1, 0,
		                                                         "Functions", "is created in C++");
		auto functions = new LaunchFunctions();
		engine.installExtensions(QJSEngine::ConsoleExtension);
		engine.globalObject().setProperty("functions",
		                                  engine.newQObject(functions));
	}

protected:
	QCoreApplication application;
	QJSEngine        engine;
};

TEST_F(QMLTestFixture, voidResult) {
	const auto result = engine.evaluate(R"(
  (function() {
      functions.startVoidValue().then(() => {
          functions.success(true);
      }, () => {
          functions.failure();
      }, () => {
          functions.failure();
      });
  })()
)");
	ASSERT_FALSE(result.isError());

	application.exec();
}

TEST_F(QMLTestFixture, stringResult) {
	engine.evaluate(R"(
  (function() {
      functions.startStringValue().then((result) => {
          functions.success(result === "test123456");
      }, () => {
          functions.failure();
      }, () => {
          functions.failure();
      });
  })()
)");
	application.exec();
}

TEST_F(QMLTestFixture, twoValuesResult) {
	const auto result = engine.evaluate(R"(
  (function() {
      functions.startTwoValues().then((a, b) => {
          functions.success(a === "test123456" && b === 42);
      }, () => {
          functions.failure();
      }, () => {
          functions.failure();
      });
  })()
)");
	ASSERT_FALSE(result.isError());

	application.exec();
}

TEST_F(QMLTestFixture, errorTest) {
	const auto result = engine.evaluate(R"(
	(function() {
	    functions.startException().then(() => {
	        functions.failure();
	    }, (error) => {
	        functions.success(error === "error");
	    }, () => {
	        functions.failure();
	    });
	})()
)");
	ASSERT_FALSE(result.isError());
	application.exec();
}

TEST_F(QMLTestFixture, stoppedTest) {
	const auto result = engine.evaluate(R"(
	(function() {
	    functions.startStopped().then(() => {
	        functions.failure();
	    }, () => {
	        functions.failure();
	    }, () => {
	        functions.success(true);
	    });
	})()
)");
	ASSERT_FALSE(result.isError());
	application.exec();
}

TEST_F(QMLTestFixture, requestStopTest) {
	const auto result = engine.evaluate(R"(
	(function() {
		let promise = functions.startDelay();
	    promise.then(() => {
	        functions.failure();
	    }, () => {
	        functions.failure();
	    }, () => {
	        functions.success(true);
	    });
		promise.requestStop();
	})()
)");
	ASSERT_FALSE(result.isError());
	application.exec();
}

#include "stdexecutils_qml_tests.moc"
