#include "p/codegen/PPCJ9HelperCallSnippet.hpp"

uint8_t *TR::PPCJ2IHelperCallSnippet::emitSnippetBody() {
    uint8_t *buffer = cg()->getBinaryBufferCursor();
    uint8_t *gtrmpln, *trmpln;

    getSnippetLabel()->setCodeLocation(buffer);
    buffer = flushArgumentsToStack(buffer, getNode(), getSizeOfArguments(), cg());
    TR::PPCCallSnippet::flushArgumentsToStack();

    if (getNode()->isJitDispatchJ9MethodCall(cg()->comp()))
        {
        }

    return self->genHelperCall(buffer);
}
