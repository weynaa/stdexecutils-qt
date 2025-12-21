#pragma once
#include <QThreadPool>
#include <stdexec/execution.hpp>

namespace stdexecutils::qt {
namespace detail {

struct threadpool_scheduler;

struct threadpool_env {
	explicit threadpool_env(QThreadPool* pool) noexcept : m_pool(pool) {}
	template <class CompletionTag>
	auto query(stdexec::get_completion_scheduler_t<CompletionTag>) const noexcept
	    -> threadpool_scheduler;

private:
	QThreadPool* const m_pool;
};

template <stdexec::receiver Recv>
struct threadpool_op_state {
	explicit threadpool_op_state(Recv&& recv, QThreadPool* pool) noexcept
	    : m_pool(pool), m_recv(std::move(recv)) {}

	void start() noexcept {
		stdexec::stoppable_token auto stop_token =
		    stdexec::get_stop_token(stdexec::get_env(m_recv));
		if (stop_token.stop_requested()) {
			stdexec::set_stopped(std::move(m_recv));
            return;
		}

		if (stop_token.stop_possible()) {
			m_pool->start([this, stop_token = std::move(stop_token)]() {
                if(stop_token.stop_requested()){
                    stdexec::set_stopped(std::move(m_recv));
                } else {
                    stdexec::set_value(std::move(m_recv));
                }
			});
		} else {
			m_pool->start([this]() { stdexec::set_value(std::move(m_recv)); });
		}
	}

private:
	Recv                             m_recv;
	QThreadPool* const               m_pool;
};

struct threadpool_sender {
	using __id = threadpool_sender;
	using __t  = threadpool_sender;

	using sender_concept = stdexec::sender_t;
	using completion_signatures =
	    stdexec::completion_signatures<stdexec::set_value_t(),
	                                   stdexec::set_stopped_t()>;

	explicit threadpool_sender(QThreadPool* pool) noexcept : m_pool(pool) {}

	stdexec::queryable auto get_env() const noexcept {
		return threadpool_env(m_pool);
	}

	template <stdexec::receiver Recv>
	stdexec::operation_state auto connect(Recv&& recv) const noexcept {
		return threadpool_op_state<Recv>(std::move(recv), m_pool);
	}

private:
	QThreadPool* const m_pool;
};

struct threadpool_scheduler {
	using __id = threadpool_scheduler;
	using __t  = threadpool_scheduler;

	explicit threadpool_scheduler(QThreadPool* pool) noexcept : m_pool(pool) {}
	stdexec::sender auto schedule() noexcept { return threadpool_sender(m_pool); }

	auto operator==(const threadpool_scheduler&) const noexcept -> bool = default;

private:
	QThreadPool* const m_pool;
};

template <class CompletionTag>
auto threadpool_env::query(stdexec::get_completion_scheduler_t<CompletionTag>)
    const noexcept -> threadpool_scheduler {
	return threadpool_scheduler(m_pool);
}

} // namespace detail

stdexec::scheduler auto
inline qthread_scheduler(QThreadPool* pool = QThreadPool::globalInstance()) {
	return detail::threadpool_scheduler(pool);
}
} // namespace stdexecutils::qt