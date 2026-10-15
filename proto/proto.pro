include(../config/staticlib.pri)
include(../config/protobuf.pri)

TARGET = qmproto
QT    -= gui core
CONFIG -= qt

PROTOS = messages.proto

!exists($$QM_GEN_DIR): mkpath($$QM_GEN_DIR)

protoc_cc.name          = protoc ${QMAKE_FILE_IN}
protoc_cc.input         = PROTOS
protoc_cc.output        = $$QM_GEN_DIR/${QMAKE_FILE_BASE}.pb.cc
protoc_cc.commands      = $$shell_quote($$shell_path($$PROTOC)) \
                          --proto_path=$$shell_quote($$shell_path($$PWD)) \
                          --cpp_out=$$shell_quote($$shell_path($$QM_GEN_DIR)) \
                          ${QMAKE_FILE_BASE}${QMAKE_FILE_EXT}
protoc_cc.variable_out  = SOURCES
protoc_cc.dependency_type = TYPE_C

protoc_h.name           = protoc header ${QMAKE_FILE_IN}
protoc_h.input          = PROTOS
protoc_h.output         = $$QM_GEN_DIR/${QMAKE_FILE_BASE}.pb.h
protoc_h.commands       = $$escape_expand(\\n)
protoc_h.depends        = $$QM_GEN_DIR/${QMAKE_FILE_BASE}.pb.cc
protoc_h.variable_out   = HEADERS

QMAKE_EXTRA_COMPILERS += protoc_cc protoc_h

OTHER_FILES += $$PROTOS
