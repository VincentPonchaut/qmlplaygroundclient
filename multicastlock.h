#ifndef MULTICASTLOCK_H
#define MULTICASTLOCK_H

#include <QObject>

class MulticastLock
{
public:
    MulticastLock();
    ~MulticastLock();
    void invokeJniMethod(const char *pMethodName);
};

#endif // MULTICASTLOCK_H
