#ifndef STDEXEC_UTILS_QML_RECEIVER_HPP
#define STDEXEC_UTILS_QML_RECEIVER_HPP

#ifndef Q_MOC_RUN
#include <exec/env.hpp>
#include <stdexec/execution.hpp>
#endif

#include <stdexecutils/qt/qthread_scheduler.hpp>

#include <QObject>
#include <QJSEngine>

namespace stdexecutils::qt {
namespace detail {

namespace {

template <class T>
auto customToScriptValue(QJSEngine* const jsEngine, const T& v) -> QJSValue {
	return jsEngine->toScriptValue(v);
}
template <>
auto customToScriptValue<std::string>(QJSEngine* const   jsEngine,
                                      const std::string& stdString)
    -> QJSValue {
	return jsEngine->toScriptValue(QString::fromStdString(stdString));
}
template <>
auto customToScriptValue<std::string_view>(QJSEngine* const        jsEngine,
                                           const std::string_view& stdString)
    -> QJSValue {
	return jsEngine->toScriptValue(QString(stdString.data()));
}
template <>
auto customToScriptValue<const char*>(QJSEngine* const   jsEngine,
                                      const char* const& cString) -> QJSValue {
	return jsEngine->toScriptValue(QString(cString));
}
template <>
auto customToScriptValue<std::exception_ptr>(
    QJSEngine* const jsEngine, const std::exception_ptr& exception)
    -> QJSValue {
	try {
		std::rethrow_exception(exception);
	} catch (std::exception& e) {
		return customToScriptValue(jsEngine, e.what());
	} catch (...) {
	}
	return QJSValue::UndefinedValue;
}
// Add more specializations that convert C++ types into
template <class... Args>
auto toValueList(QJSEngine* const jsEngine, Args&&... args) -> QJSValueList {
	QJSValueList list;
	([&](auto&& v) { list.append(customToScriptValue(jsEngine, v)); }(
	     std::forward<Args>(args)),
	 ...);
	return list;
}
} // namespace
} // namespace detail

class QmlReceiver : public QObject {
	Q_OBJECT
public:
	struct receiver_env {
		explicit receiver_env(QmlReceiver& obj) noexcept : m_receiver(obj) {}
		auto query(stdexec::get_scheduler_t) const noexcept -> QThreadScheduler {
			return QThreadScheduler{&m_receiver};
		}

		auto query(stdexec::get_delegatee_scheduler_t) const noexcept
		    -> QThreadScheduler {
			return QThreadScheduler{&m_receiver};
		}

		auto query(stdexec::get_stop_token_t) const noexcept {
			return m_receiver.m_stopSource.get_token();
		}

	private:
		QmlReceiver& m_receiver;
	};

	template <stdexec::queryable Env>
	struct op_state_base {

		using _Env = Env;

		explicit op_state_base(Env&& env) noexcept : m_env(std::move(env)) {}

		op_state_base(const op_state_base&) = delete;
		op_state_base(op_state_base&&)      = delete;

		virtual ~op_state_base() = default;

		Env m_env;
	};
	template <stdexec::queryable Env>
	struct receiver : public stdexec::receiver_adaptor<receiver<Env>> {
		using __id = receiver<Env>;
		using __t  = receiver<Env>;

		receiver(QmlReceiver& receiverObj, op_state_base<Env>* opState) noexcept
		    : m_recieverObj(receiverObj), m_opState(opState) {}

		template <class... Args>
		void set_value(Args&&... args) noexcept {
			QMetaObject::invokeMethod(
			    &m_recieverObj,
			    [... args = std::forward<Args>(args),
			     receiverObj = &m_recieverObj]() {
				    if (receiverObj->m_onValue.isCallable()) {
					    receiverObj->m_onValue.call(
					        detail::toValueList(qjsEngine(receiverObj), args...));
				    }
			    },
			    Qt::QueuedConnection);
			delete m_opState;
			m_opState = nullptr;
		}

		template <class... Args>
		void set_error(Args&&... args) noexcept {
			QMetaObject::invokeMethod(
			    &m_recieverObj,
			    [... args    = std::forward<Args>(args),
			     receiverObj = &m_recieverObj]() {
				    if (receiverObj->m_onError.isCallable()) {
					    receiverObj->m_onError.call(
					        detail::toValueList(qjsEngine(receiverObj), args...));
				    }
			    },
			    Qt::QueuedConnection);
			delete m_opState;
			m_opState = nullptr;
		}

		void set_stopped() noexcept {
			QMetaObject::invokeMethod(
			    &m_recieverObj,
			    [receiverObj = &m_recieverObj]() {
				    if (receiverObj->m_onStopped.isCallable()) {
					    receiverObj->m_onStopped.call();
				    }
			    },
			    Qt::QueuedConnection);
			delete m_opState;
			m_opState = nullptr;
		}

		[[nodiscard]] auto get_env() const noexcept -> const Env& {
			return m_opState->m_env;
		}

	private:
		op_state_base<Env>* m_opState; // For cleanup when the reciever is finished
		QmlReceiver&        m_recieverObj;
	};

	template <stdexec::sender Sender, stdexec::queryable Env>
	struct op_state : public op_state_base<Env> {

		op_state(Sender&& sender, Env&& env, QmlReceiver& receiverObj) noexcept
		    : m_connectOpState(stdexec::connect(std::move(sender),
		                                        receiver<Env>(receiverObj, this))),
		      op_state_base<Env>(std::forward<Env>(env)) {}

		void start() noexcept { stdexec::start(m_connectOpState); }

	private:
		stdexec::connect_result_t<Sender, stdexec::__t<receiver<Env>>>
		    m_connectOpState;
	};

public:
	template <stdexec::sender Sender, stdexec::queryable Env = stdexec::empty_env>
	QmlReceiver(Sender&& sender, Env&& env = stdexec::empty_env{},
	            QObject* parent = nullptr)
	    : QObject(parent) {
		// Note this is not a memory leak: op_state is cleaned up when the executor
		// terminates
		static_assert(stdexec::receiver<receiver<Env>>, "not a valid receiver");
		stdexec::start(*(new op_state<Sender, stdexec::env<receiver_env, Env>>(
		    std::move(sender),
		    stdexec::env<receiver_env, Env>(receiver_env(*this),
		                                    std::forward<Env>(env)),
		    *this)));
	}

	static void registerMetatype(const char* moduleUri          = "QmlReceiver",
	                             int         moduleVersionMajor = 1,
	                             int         moduleVersionMinor = 0);

	Q_INVOKABLE void then(QJSValue valueFunction, QJSValue failedFunction = {},
	                      QJSValue stoppedFunction = {});

public slots:
	void requestStop() noexcept { 
		m_stopSource.request_stop();
	}

private:
	template <stdexec::queryable Env>
	friend struct receiver;

	friend struct receiver_env;

	QJSValue m_onValue   = QJSValue::UndefinedValue;
	QJSValue m_onError   = QJSValue::UndefinedValue;
	QJSValue m_onStopped = QJSValue::UndefinedValue;

	stdexec::inplace_stop_source m_stopSource;
};

} // namespace stdexecutils::qt

#endif // STDEXEC_UTILS_QML_RECEIVER_HPP
