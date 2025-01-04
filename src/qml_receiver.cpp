#include <stdexecutils/qt/qml_receiver.hpp>

#include <QtQml>

namespace stdexecutils::qt {

void QmlReceiver::registerMetatype(const char* moduleUri /*= "QmlReceiver"*/,
                                   int         moduleVersionMajor /*=1*/,
                                   int         moduleVersionMinor /*= 0*/) {
	qmlRegisterUncreatableType<QmlReceiver>(
	    moduleUri, moduleVersionMajor, moduleVersionMinor, "QmlReceiver",
	    "senders can only be launched in C++");
	
	qRegisterMetaType<QmlReceiver*>("QmlReceiver*");
}

void QmlReceiver::then(QJSValue valueFunction, QJSValue failedFunction /*= {}*/,
                       QJSValue stoppedFunction /*= {}*/) {
	if (valueFunction.isCallable()) {
		m_onValue = valueFunction;
	}
	if (failedFunction.isCallable()) {
		m_onError = failedFunction;
	}
	if (stoppedFunction.isCallable()) {
		m_onStopped = stoppedFunction;
	}
}
} // namespace stdexecutils
