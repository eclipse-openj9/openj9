ifdef JITAAS_USE_GRPC
   PROTO_GEN_FILES=\
       compiler/rpc/gen/compile.grpc.pb.cpp \
       compiler/rpc/gen/compile.pb.cpp
else
   JITAAS_USE_RAW_SOCKETS=1
   PROTO_GEN_FILES=\
       compiler/rpc/gen/compile.pb.cpp
endif
    
JIT_PRODUCT_SOURCE_FILES+=$(PROTO_GEN_FILES)
