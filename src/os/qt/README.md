# QT 
## Quickstart
From root dir run
``` bash
mkdir build_osal_test
cd build_osal_test
cmake -DENABLE_UNIT_TESTS=true -DCMAKE_BUILD_TYPE=Debug -DOSAL_SYSTEM_BSPTYPE=generic-qt -DOSAL_CONFIG_DEBUG_PERMISSIVE_MODE=TRUE ..
make
make test
```
## Rational 
This is an experimental operating attempt at using QT as a layer between osal, and the final operating system.

As of 2022 QT provides the following support for the following operating systems.
* Linux / X11
* MacOS
* Windows
* Android 6.0 or higher
* iOS 13+
* Android Automotive OS
* webOS OSE
* embedded Linux
* Integrity 19.0.13
* QNX 7.1
* VxWorks
* Wndows embedded 7
* Windows phone
See 
- https://doc.qt.io/qt-6/supported-platforms.html
- https://wiki.qt.io/Supported_Platforms

## Known defects
### Limited priortiy levels.
Qt supports only 7 different priority levels, and could be ignored.
See 
- https://doc.qt.io/qt-5/qthread.html#Priority-enum
- https://doc.qt.io/qt-5/qthread.html#start

### Scheduler
Scheduler needs to be manually created, and has not been done yet.

### Thread stopping 
QT has an issue where if a task_a is killed while it has a locked semaphore / mutex.
Then task_b that unlocks the semaphore / mutex will hang.
This is the case on mac os.  Windows may have a different semaphore implementation. 

# Porting notes.
## TimeBase
The implementation port that I did Time base used a stand alone timer to unlock a semaphore at a periodic rate.

And then a callback that would block on the semaphore being released.
## Network sockets.
Qt uses an underlying stream class.
- Sockets uses the stream table
    sock = OS_OBJECT_TABLE_GET(OS_stream_table, *token);
    Files also use OS_stream_table
    