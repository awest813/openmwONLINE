#ifndef OPENMW_MWSCRIPT_PERFORMANCEEXTENSIONS_H
#define OPENMW_MWSCRIPT_PERFORMANCEEXTENSIONS_H

namespace Interpreter
{
    class Interpreter;
}

namespace MWScript
{
    namespace Performance
    {
        void installOpcodes(Interpreter::Interpreter& interpreter);
    }
}

#endif
