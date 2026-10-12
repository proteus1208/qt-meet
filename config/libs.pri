# qmUseLib(name): link an internal static library and rebuild when it changes.
# Call in dependency order: dependents first (qmrtc, qmmedia, qmcommon, qmproto).

defineTest(qmUseLib) {
    name = $$1
    LIBS += -L$$QM_LIB_DIR -l$$name
    win32-msvc*: PRE_TARGETDEPS += $$QM_LIB_DIR/$${name}.lib
    else:        PRE_TARGETDEPS += $$QM_LIB_DIR/lib$${name}.a
    export(LIBS)
    export(PRE_TARGETDEPS)
    return(true)
}
