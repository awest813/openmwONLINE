#include "performanceextensions.hpp"

#include <components/interpreter/interpreter.hpp>
#include <components/interpreter/opcodes.hpp>
#include <components/interpreter/runtime.hpp>

#include "../../performance_toolkit/toolkit.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

namespace MWScript
{
    namespace Performance
    {
        class OpTogglePerformanceOverlay : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                auto& toolkit = PerformanceToolkit::Toolkit::getInstance();
                toolkit.toggleOverlay(!toolkit.isOverlayEnabled());
            }
        };

        class OpStartBenchmark : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                PerformanceToolkit::Toolkit::getInstance().startBenchmark(10.0f); // default 10s
            }
        };

        void installOpcodes(Interpreter::Interpreter& interpreter)
        {
            interpreter.installKnown("toggleperformanceoverlay", new OpTogglePerformanceOverlay);
            interpreter.installKnown("startbenchmark", new OpStartBenchmark);
        }
    }
}
