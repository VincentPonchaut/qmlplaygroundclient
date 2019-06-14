#include "multicastlock.h"

#include <QLoggingCategory>
#include <QtGlobal>

#if defined(Q_OS_ANDROID)
    #include <QtAndroid>
    #include <QtAndroidExtras>
    #include <QAndroidJniEnvironment>
    #include <QAndroidJniObject>
#endif

MulticastLock::MulticastLock()
{
#if defined(Q_OS_ANDROID)
    invokeJniMethod("acquire");
#endif
}


MulticastLock::~MulticastLock()
{
#if defined(Q_OS_ANDROID)
    invokeJniMethod("release");
#endif
}


void MulticastLock::invokeJniMethod(const char* pMethodName)
{
#if defined(Q_OS_ANDROID)
    QAndroidJniEnvironment env;
    const QAndroidJniObject context(QtAndroid::androidContext());
    if (!context.isValid())
    {
        qCritical() << "Cannot determine android context.";
        return;
    }

    QAndroidJniObject::callStaticMethod<void>("com.mycompany.qmlplaygroundclient/MulticastLockJniBridgeUtil",
            pMethodName,
            "(Landroid/content/Context;)V",
            context.object<jobject>());

    if (env->ExceptionCheck())
    {
        qCritical() << "Cannot call MulticastLockJniBridgeUtil." << pMethodName << "()";
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
#else
    Q_UNUSED(pMethodName);
#endif
}
