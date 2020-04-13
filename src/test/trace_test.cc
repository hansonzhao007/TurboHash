#include "util/trace.h"
#include "util/io_report.h"

using namespace util;
int main() {

    Stats stats;
    util::Trace* trace = new util::TraceUniform(123);
    uint64_t num = 0;
    stats.Start();
    for (uint64_t i = 0; i < 100000000; i++) {
        num = trace->Next();
        stats.FinishedSingleOp();
    }
    stats.Stop();
    stats.Report("Trace speed");

    stats.Start();
    for (uint64_t i = 0; i < 100000000; i++) {
        num = random();
        stats.FinishedSingleOp();
    }
    stats.Stop();
    stats.Report("Random speed");
    return 0;
}