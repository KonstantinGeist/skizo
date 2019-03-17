// *****************************************************************************
//
//  Copyright (c) Konstantin Geist. All rights reserved.
//
//  The use and distribution terms for this software are contained in the file
//  named License.txt, which can be found in the root of this distribution.
//  By using this software in any fashion, you are agreeing to be bound by the
//  terms of this license.
//
//  You must not remove this notice, or any other, from this software.
//
// *****************************************************************************

#include "Profiling.h"
#include "TextBuilder.h"

// TODO the overflow problem; see how other frameworks deal with it

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

CProfilingInfo::CProfilingInfo(const CDomain* domain)
    : m_methods(new CArrayList<CMethod*>()),
      m_totalTime(0)
{
    m_domain.SetVal(domain);
}

static int cmpTotalTimeInMs(CMethod* m1, CMethod* m2)
{
    return (int)(m2->TotalTimeInMs() - m1->TotalTimeInMs());
}

static int cmpAverageTimeInMs(CMethod* m1, CMethod* m2)
{
    const int averageTimeInMs1 = m1->NumberOfCalls()? (m1->TotalTimeInMs() / m1->NumberOfCalls()): 0;
    const int averageTimeInMs2 = m2->NumberOfCalls()? (m2->TotalTimeInMs() / m2->NumberOfCalls()): 0;

    return averageTimeInMs2 - averageTimeInMs1;
}

static int cmpNumberOfCalls(CMethod* m1, CMethod* m2)
{
    return (int)(m2->NumberOfCalls() - m1->NumberOfCalls());
}

void CProfilingInfo::SortByTotalTimeInMs()
{
    m_methods->Sort(cmpTotalTimeInMs);
}

void CProfilingInfo::SortByAverageTimeInMs()
{
    m_methods->Sort(cmpAverageTimeInMs);
}

void CProfilingInfo::SortByNumberOfCalls()
{
    m_methods->Sort(cmpNumberOfCalls);
}

void CProfilingInfo::_dumpImpl(bool dumpToDisk) const
{
    STextBuilder cb;

    cb.Emit("==============\n"
            "Metadata stats\n"
            "==============\n\n");

    //int scriptCodeInBytes = 0;
    int cntAllClasses = 0;
    int cntCompilerGen = 0;
    int cntClosures = 0;
    int cntInstanceCtors = 0, cntInstanceDtors = 0,
        cntStaticMethods = 0, cntInstanceMethods = 0;

    const CArrayList<CClass*>* classes = m_domain->Classes();
    cntAllClasses = classes->Count();
    cb.Emit("All Classes: %d\n", cntAllClasses);

    for(int i = 0; i < cntAllClasses; i++) {
        const CClass* pClass = classes->Array()[i];

        if(pClass->IsCompilerGenerated()) {
            cntCompilerGen++;
        } else if(pClass->SpecialClass() == E_SPECIALCLASS_METHODCLASS) {
            cntClosures++; // TODO
        }

        cntInstanceCtors += pClass->InstanceCtors()->Count();
        if(pClass->InstanceDtor()) {
            cntInstanceDtors++;
        }
        cntStaticMethods += pClass->StaticMethods()->Count();

        if(pClass->IsClassHierarchyRoot()) {
            cntInstanceMethods += pClass->InstanceMethods()->Count();
        } else {
            // Children inherit parent methods, so we ignore repetitions here.
            const int parentMethods = pClass->ResolvedBaseClass()->InstanceMethods()->Count();
            cntInstanceMethods += pClass->InstanceMethods()->Count() - parentMethods;
        }
    }
    cb.Emit("\tUser-defined classes: %d\n", cntAllClasses - cntCompilerGen);
    cb.Emit("\tCompiler-generated classes: %d\n", cntCompilerGen);
    cb.Emit("\tClosure classes: %d\n", cntClosures);
    cb.Emit("All methods: %d\n", cntInstanceCtors + cntInstanceDtors + cntStaticMethods + cntInstanceMethods);
    cb.Emit("\tInstance methods: %d\n", cntInstanceMethods);
    cb.Emit("\tStatic methods: %d\n", cntStaticMethods);
    cb.Emit("\tInstance ctors: %d\n", cntInstanceCtors);
    cb.Emit("\tInstance dtors: %d\n", cntInstanceDtors);

    cb.Emit("\n==============\n"
            "Profiling data\n"
            "==============\n");

    for(int i = 0; i < m_methods->Count(); i++) {
        const CMethod* method = m_methods->Array()[i];

        const int totalTimeInMs = method->TotalTimeInMs();
        const int numberOfCalls = method->NumberOfCalls();
        const int averageTimeInMs = method->NumberOfCalls()? (method->TotalTimeInMs() / method->NumberOfCalls()) : 0;
        // TODO self time

        const CString* niceClassName0 = method->DeclaringClass()->NiceName();

        // CCodeBuilder we rely on recognizes only string slices.
        const SStringSlice niceClassName (niceClassName0, 0, niceClassName0->Length());

        cb.Emit("%s::%s | totalTime: %d ms | numberOfCalls: %d | averageTime: %d\n",
                        &niceClassName,
                        &method->Name(),
                        totalTimeInMs,
                        numberOfCalls,
                        averageTimeInMs);
    }

    cb.Emit("Total execution time: %d ms.\n", (int)m_totalTime); // TODO casts long to int?

    cb.Emit("\n====================\n"
              "Runtime memory statistics\n"
              "====================\n");
    const SBumpPointerAllocator& allocator = m_domain->MemoryManager().BumpPointerAllocator();
    cb.Emit("\tExpressions: %d KB\n", (int)(allocator.GetMemoryByAllocationType(E_SKIZOALLOCATIONTYPE_EXPRESSION) / 1024)); // TODO?
    cb.Emit("\tClasses: %d KB\n", (int)(allocator.GetMemoryByAllocationType(E_SKIZOALLOCATIONTYPE_CLASS) / 1024)); // TODO?
    cb.Emit("\tMembers: %d KB\n", (int)(allocator.GetMemoryByAllocationType(E_SKIZOALLOCATIONTYPE_MEMBER) / 1024)); // TODO?
    cb.Emit("\tTokens: %d KB\n", (int)(allocator.GetMemoryByAllocationType(E_SKIZOALLOCATIONTYPE_TOKEN) / 1024)); // TODO?

    const char* cs = cb.Chars();
    if(dumpToDisk) {
        FILE* f = fopen("profile.txt", "wb");
        fwrite(cs, strlen(cs), 1, f);
        fclose(f);
    } else {
        printf("%s", cs);
    }
}

void CProfilingInfo::DumpToConsole() const
{
    _dumpImpl(false);
}

void CProfilingInfo::DumpToDisk() const
{
    _dumpImpl(true);
}

} }
