#pragma once

namespace qm {

/// Aim: development (multi-instance OK) vs production (single-instance).
///
/// Choose the mode here (no Debug/Release kit needed):
///   qt-meet/config/common.pri  →  QM_APP_MODE = development|production
/// then rebuild.
///
/// Runtime override (no rebuild):
///   --dev          allow multiple instances
///   --production   enforce single-instance
inline bool isDevelopmentMode(bool forceDev, bool forceProduction)
{
    if (forceDev)
        return true;
    if (forceProduction)
        return false;
#if defined(QM_FORCE_PRODUCTION)
    return false;
#elif defined(QM_DEVELOPMENT)
    return true;
#else
    return true; // safe default while developing
#endif
}

} // namespace qm
