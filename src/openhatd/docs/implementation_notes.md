


All openhatd ports try to do as little as possible in the doWork loop. Some operations need to be performed independently of the doWork loop because they take longer or employ some kind of blocking. These functions are usually implemented using separate threads. For port implementors who use threads it is important to know that everything that modifies openhatd port state must only be done in the doWork loop which is called from the main thread.

However, some operations that must be done on the main thread may cause the doWork iteration to delay for a time that is longer (maybe much longer) than the specified fps rate. An example is the WebServerPlugin which serves incoming requests in the doWork method. So, while time in openhatd (as obtained by the internal function opdi_get_time_ms) is guaranteed to increase monotonically, the intervals between doWork invocations may vary greatly. This point should be taken into account when configuring port settings and developing openhatd plugins or ports. On Windows, due to the time granularity being 10 ms or more, two or more subsequent doWork iterations may even seem to run at the same time as reported by the opdi_get_time_ms function.
