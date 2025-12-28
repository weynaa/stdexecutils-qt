#ifndef STDEXEC_UTILS_QTHREAD_SCHEDULER_HPP
#define STDEXEC_UTILS_QTHREAD_SCHEDULER_HPP

#include <QAbstractEventDispatcher>
#include <QBasicTimer>
#include <QObject>
#include <QThread>
#include <QTimerEvent>
#include <stdexec/__detail/__execution_fwd.hpp>

#ifndef Q_MOC_RUN
#include <stdexec/execution.hpp>
#endif

#include <chrono>
#include <optional>

namespace stdexecutils::qt {

class QThreadScheduler {
public:
	using __id = QThreadScheduler;
	using __t  = QThreadScheduler;

	struct env {

		explicit env(QThread* thread) noexcept : m_thread(thread) {}

		template <stdexec::__completion_tag Tag>
		auto query(stdexec::get_completion_scheduler_t<Tag>) const noexcept
		    -> QThreadScheduler {
			return QThreadScheduler{m_thread};
		}

	private:
		QThread* const m_thread;
	};

	// Sender for schedule
	template <class Recv>
	struct op_state {
		op_state(Recv&& receiver, QThread* thread)
		    : m_receiver(std::move(receiver)), m_thread(thread) {}

		op_state(const op_state&) = delete;
		op_state(op_state&&)      = delete;

		void start() noexcept {
			stdexec::stoppable_token auto stop_token =
			    stdexec::get_stop_token(stdexec::get_env(m_receiver));
			if (stop_token.stop_requested()) {
				stdexec::set_stopped(std::move(m_receiver));
				return;
			}
			QMetaObject::invokeMethod(
			    m_thread->eventDispatcher(),
			    [this, stop_token]() {
				    if (stop_token.stop_requested()) {
					    stdexec::set_stopped(std::move(m_receiver));
					    return;
				    }
				    stdexec::set_value(std::move(m_receiver));
			    },
			    Qt::QueuedConnection);
		}

	private:
		Recv           m_receiver;
		QThread* const m_thread;
	};
	struct sender {
		using __id = sender;
		using __t  = sender;

		using sender_concept        = stdexec::sender_t;
		using completion_signatures = stdexec::completion_signatures< //
		    stdexec::set_value_t(),                                   //
		    stdexec::set_stopped_t()>;

		explicit sender(QThread* thread) noexcept : m_thread(thread) {}

		template <class R>
		auto connect(R r) const -> op_state<R> {
			return op_state<R>(std::move(r), m_thread);
		};

		auto get_env() const noexcept -> env { return env{m_thread}; }

	private:
		QThread* const m_thread;
	};

	// Operation state for schedule_at and scheduler_after
	template <class Recv>
	struct timeout_op_state : public QObject {

		using deadline_or_delay =
		    std::variant<std::chrono::system_clock::time_point,
		                 std::chrono::system_clock::duration>;

		timeout_op_state(Recv&& receiver, QThread* thread,
		                 deadline_or_delay deadlineOrDelay)
		    : QObject(nullptr), m_receiver(std::move(receiver)),
		      m_deadlineOrDelay(deadlineOrDelay) {
			moveToThread(thread);
		}

		void start() noexcept {
			stdexec::stoppable_token auto stop_token =
			    stdexec::get_stop_token(stdexec::get_env(m_receiver));

			if (stop_token.stop_requested()) {
				stdexec::set_stopped(std::move(m_receiver));
				return;
			}
			connect(qApp, &QCoreApplication::aboutToQuit, this,
			        &timeout_op_state::handle_stopped);
			connect(thread(), &QThread::finished, this,
			        &timeout_op_state::handle_stopped);

			const auto intervalFromNow =
			    std::holds_alternative<std::chrono::system_clock::time_point>(
			        m_deadlineOrDelay)
			        ? std::chrono::duration_cast<std::chrono::milliseconds>(
			              std::get<std::chrono::system_clock::time_point>(
			                  m_deadlineOrDelay) -
			              std::chrono::system_clock::now())
			        : std::chrono::duration_cast<std::chrono::milliseconds>(
			              std::get<std::chrono::system_clock::duration>(
			                  m_deadlineOrDelay));

			if (intervalFromNow.count() < 0) {
				QMetaObject::invokeMethod(
				    this, [this]() { stdexec::set_value(std::move(m_receiver)); },
				    Qt::QueuedConnection);
				return;
			}

			m_timer.start(intervalFromNow, this);
			if (stop_token.stop_possible()) {
				m_stoppedCallback.emplace(std::move(stop_token),
				                          stop_callback_fun{*this});
			}
		}

	private:
		void timerEvent(QTimerEvent* ev) override {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
			assert(ev->id() == m_timer.id());
#endif
			if (m_done) {
				return;
			}
			m_done = true;
			m_timer.stop();
			stdexec::set_value(std::move(m_receiver));
		}

		struct stop_callback_fun {
			timeout_op_state& op_state;

			void operator()() {
				QMetaObject::invokeMethod(&op_state,
				                          [this]() { op_state.handle_stopped(); });
			}
		};
	private Q_SLOTS:
		void handle_stopped() {
			if (m_done) {
				return;
			}
			m_done = true;
			m_timer.stop();
			stdexec::set_stopped(std::move(m_receiver));
		}

		friend struct stop_callback_fun;
		using stop_callback = stdexec::stop_callback_for_t<
		    stdexec::stop_token_of_t<stdexec::env_of_t<Recv>>, stop_callback_fun>;

		Recv                         m_receiver;
		const deadline_or_delay      m_deadlineOrDelay;
		QBasicTimer                  m_timer;
		bool                         m_done{false};
		std::optional<stop_callback> m_stoppedCallback;
	};

	// Sender for schedule_at
	struct timeout_sender {
		using __id = timeout_sender;
		using __t  = timeout_sender;

		using sender_concept        = stdexec::sender_t;
		using completion_signatures = stdexec::completion_signatures< //
		    stdexec::set_value_t(),                                   //
		    stdexec::set_stopped_t()>;

		explicit timeout_sender(
		    QThread*                              thread,
		    std::chrono::system_clock::time_point deadline) noexcept
		    : m_thread(thread), m_deadline(deadline) {}

		template <class R>
		auto connect(R r) const -> timeout_op_state<R> {
			return timeout_op_state<R>(std::move(r), m_thread, m_deadline);
		};

		auto get_env() const noexcept -> env { return env{m_thread}; }

	private:
		QThread* const                              m_thread;
		const std::chrono::system_clock::time_point m_deadline;
	};

	// Sender for schedule_after
	struct delay_sender {
		using __id = delay_sender;
		using __t  = delay_sender;

		using sender_concept        = stdexec::sender_t;
		using completion_signatures = stdexec::completion_signatures< //
		    stdexec::set_value_t(),                                   //
		    stdexec::set_stopped_t()>;

		explicit delay_sender(QThread*                            thread,
		                      std::chrono::system_clock::duration duration) noexcept
		    : m_thread(thread), m_duration(duration) {}

		template <class R>
		auto connect(R r) const -> timeout_op_state<R> {
			return timeout_op_state<R>(std::move(r), m_thread, m_duration);
		};

		auto get_env() const noexcept -> env { return env{m_thread}; }

	private:
		QThread* const                            m_thread;
		const std::chrono::system_clock::duration m_duration;
	};

	explicit QThreadScheduler(QThread* thread) noexcept : m_thread(thread) {}
	explicit QThreadScheduler(QObject* object) noexcept
	    : m_thread(object->thread()) {}

	auto schedule() const -> sender { return sender{m_thread}; }

	auto schedule_at(std::chrono::system_clock::time_point deadline) const
	    -> timeout_sender {
		return timeout_sender{m_thread, deadline};
	};

	auto schedule_after(std::chrono::system_clock::duration deadline) const
	    -> delay_sender {
		return delay_sender{m_thread, deadline};
	};

	auto operator==(const QThreadScheduler&) const noexcept -> bool = default;

private:
	QThread* const m_thread;
};
} // namespace stdexecutils::qt
#endif
