//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

// Standalone executable that verifies cancellation::none() does not cause memory
// leaks.  cancellation::none() is backed by a no_destroy wrapper whose destructor
// never runs.  If the cancellation_source constructor allocates heap memory (e.g.
// an eagerly-constructed std::vector inside ticketed_collection), that memory can
// never be freed and will be reported by _CrtDumpMemoryLeaks.
//
// Including cancellation.h brings the no_destroy inline variable definition into
// this translation unit, so it is constructed during static initialization before
// main runs.  We also explicitly call add_listener on the none() token to verify
// that the early-out path (which returns a dummy ticket) does not trigger any
// heap allocations that would be reported as leaks.
//
// We call _CrtDumpMemoryLeaks explicitly and return a non-zero exit code so that
// CTest treats the test as failed when leaks are detected.

#include <crtdbg.h>
#include <cstdlib>
#include <iostream>

#include <arcana/threading/cancellation.h>

int main()
{
#ifdef _DEBUG
    // Exercise the add_listener early-out path on cancellation::none().
    // This should not allocate any memory since none() returns a dummy ticket.
    {
        auto ticket = arcana::cancellation::none().add_listener([] {});
    }

    // _CrtDumpMemoryLeaks reports all CRT debug-heap blocks still allocated.
    if (_CrtDumpMemoryLeaks())
    {
        std::cerr << "Memory leaks detected!" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
#else
    std::cerr << "Skipping: _CrtDumpMemoryLeaks is a no-op in Release builds." << std::endl;
    return EXIT_SUCCESS;
#endif
}
