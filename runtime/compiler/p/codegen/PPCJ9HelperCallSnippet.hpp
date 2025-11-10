#ifndef PPCJ9HELPERCALLSNIPPET_INCL
#define PPCJ9HELPERCALLSNIPPET_INCL

#include "codegen/Snippet.hpp"
#include "env/VMJ9.h"
#include "infra/Annotations.hpp"
#include "p/codegen/PPCInstruction.hpp"

namespace TR { class CodeGenerator; }

class PPCJ9HelperCallSnippet : public TR::PPCHelperCallSnippet {
    int32_t _argSize;

public:
    PPCJ9HelperCallSnippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *snippetlab,
        TR::SymbolReference *helper, TR::LabelSymbol *restartLabel = NULL, int32_t argSize)
        : TR::PPCHelperCallSnippet(cg, node, snippetlab, helper, restartLabel)
        , _argSize(argSize)
    {}

    int32_t getSizeOfArguments() { return _argSize; }
};

#endif