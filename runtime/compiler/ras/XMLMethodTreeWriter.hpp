

#ifndef XMLMETHODTREEWRITER_INCL
#define XMLMETHODTREEWRITER_INCL

#include "MethodTreeWriter.hpp"

class XMLMethodTreeWriter : public MethodTreeWriter {

    public:
    XMLMethodTreeWriter(int32_t id, TR_ResolvedMethod * method, TR::Options & options);
    ~XMLMethodTreeWriter() override;
    void writeGraph(std::string & string, TR::Compilation * compilation, TR::ResolvedMethodSymbol * methodSymbol) override;
    void complete() override {
          if (initialized) {
            sink->write("</group>\n");
            sink->write("</graphDocument>\n");
          }
    }

    FileSink * getSink() {
        return sink;
    }

    private:
    bool initialize(TR::Compilation * compilation, TR::ResolvedMethodSymbol * symbol);

    bool initialized = false;
    FileSink * sink;


};

#endif
