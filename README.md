## What is it?

Skizo is a toy programming language I wrote years ago for fun when learning how programming languages/runtimes work.

### What is it good for?

It's not really groundbreaking in its feature set or the implementation, but it's fun to use as a foundation to learn and experiment with new ideas.
Extending the runtime with new features is very easy, and I tried to make the code very readable and full of comments.

### Main features:

* Statically/strongly typed.
* OOP with structural typing: classes (heap-allocated), structs (stack-allocated), interfaces, extendable classes, enums, attributes, aliases, events etc.
* Garbage collected.
* Inline C.
* FFI: supports calling native methods with all sorts of signatures, including closures.
* Unsafe methods, which allow to work with native memory, taking pointers etc.
* Uniform method call syntax and "everything is a method call": math operations, method calls, language constructs, all have same syntax. There's no special syntax for "if" or "for" constructs, they are implemented as method calls as well.
* A few built-in collections (array, map).
* A tiny standard library: string, stringbuilder, path, console, stopwatch, marshal, application, guid, math, random, stream etc.
* Failables (a sum type of "value" and "error").
* Autoboxing/unboxing.
* Reflection.
* Fast templates.
* Simple type inference.
* No separate compilation step; usable as a scripting language.
* Relatively fast: compiles to C in memory, and the C output is human-readable.
* Soft debugging.
* The codebase was checked with PVS Studio, cppcheck and Valgrind for possible logical and runtime errors.

## Quirks

* The quirky method call syntax was originally invented to simplify the implementation and to make it more extendable/flexible. I was envisioning the notion of "transformers" which would allow to transform the AST at compile time, including being able to process custom syntax.
* This release contains ideas such as "foreign objects", "domains", "permissions". Originally, I envisioned a system where a program is divided into sandboxed single-threaded "domains" which are tied to their respective threads of execution and protected with their own permission sets, and interoperate via "foreign object pointers". I'm planning to revise it.
* The project doesn't make use of idiomatic C++, it reinvents the standard library and has a few of its own idioms. There were several reasons behind it:
1) It's easier to reason about a simple subset of C++, a contributor does't need to be an expert at C++, however they have access to some of C++'s nice features such as RAII and templates.
2) You have full control of the implementation and the semantics, good for learning.
3) When the semantics and the implementation are well understood, it's trivial to pass pointers to C++ memory to Skizo and back, and assume certain things.
4) Parts of the runtime can be reused to implement Skizo's standard library. Since those classes are already heavily used by the compiler/runtime itself, there's additional test coverage at no cost.
5) It makes it possible to port the runtime to Skizo itself, because we share same classes/objects.
* Strings use UTF16 characters, because I was heavily influenced by C# and Java.
* Generally, the standard library is heavily influenced by C#.
* The generated C code is highly intertwined with the runtime of the compiler/execution engine (it emits direct pointers to existing memory during compilation), hence making AOT compilation harded to do.

## Known limitations

* So far, it's x86 only, because it generates x86-specific JIT thunks for closures, reflection and boxed object methods.
* Aborts don't properly work on Linux, because on Linux, C++ exceptions (which implement "abort") cannot unwind through JIT-generated stacks. Abort on Linux cannot be caught and will crash the entire application. I'm planning to revise this.
* The GC I wrote is pretty sketchy and not very performant.
* Compilation time is OK for small scripts but grows considerably on large inputs.
* No incremental compilation.
* For compilation, it currently uses an outdated version of TCC, which is not very fast and not actively maintained. Perhaps, eventually I'll move to AOT where we can reuse GCC or clang.
* No generics.
* "Dependency injection" is an incompleted work in progress.
* Foreign objects don't properly work on Linux.

## Plans

* Rethink the domain/foreign object model.
* Add a HTTP module, among other standard library modules.
* Add namespaces.
* Add async I/O.
* Experimentation: support DDD/clean architecture ideas at the language level (bounded contexts, layers, dependency injection, etc.)
* Port to x64.
* Add different compilation backends.
* Add channels/queues.
* Add more options to the launcher executable.
* Get rid of unused stuff.
* Add defer.

## How to build on Linux

You need CMake and GCC installed on your system.

Additionally, you need to install GCC tools for cross-compilation:

    sudo apt-get install gcc-multilib g++-multilib

Then compile it with (debug mode):

    ./build_unix32d.sh

Tested on Ubuntu 20.04.

## How to build on Windows

You need CMake and MinGW (with 32-bit support) installed on your system.

Then compile it with (debug mode):
./build_win32d.sh

Release mode scripts end with -r suffix: for example, build_win32r.sh