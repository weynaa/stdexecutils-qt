#include <stdexecutils/qt/qthread_scheduler.hpp>

#include <QObject>

using namespace stdexecutils::qt;

static_assert(stdexec::scheduler<QThreadScheduler>, "QThreadScheduler is not adhering to the scheduler concept");
int main() {
	return 0;
}
