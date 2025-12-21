# Various utilities for Senders

Some useful utilities for P2300 Senders in conjunction with Qt
 - `QThreadScheduler`: a scheduler that's reusing the a Qt event loop. Includes simple schedule, as well as `schedule_at` and `schedule_after` and supports cancellation.
 - `QmlReceiver`: a receiver that provides continuation in QML with a .then function, similar to a JS Promise
 - `QThreadPoolScheduler`: a wrapper for QThreadPool
TODO/Ideas:
 - some kind of `connect` functionality, where we can lanuch a sender with a signal invocation and terminate it in some slots

