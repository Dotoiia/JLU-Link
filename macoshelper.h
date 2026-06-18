#ifndef MACOSHELPER_H
#define MACOSHELPER_H

#include <QtCore/qglobal.h>

#ifdef Q_OS_MACOS
void SetMacApplicationVisibleInDock(bool visible);
void InstallMacLiquidGlass(void *nativeView);
#else
inline void SetMacApplicationVisibleInDock(bool) {}
inline void InstallMacLiquidGlass(void *) {}
#endif

#endif // MACOSHELPER_H
