# pleroma // [guide](https://wangell.github.io/pleroma-docs/)

[![blue sky utopian dream](https://i.imgur.com/FdQcgOf.png)](#)
[![forest](https://github.com/wangell/pleroma-docs/raw/main/forest.png)](#)
[![abba](https://github.com/wangell/pleroma-docs/raw/main/desertfathers.png)](#)
[![sands](https://github.com/wangell/pleroma-docs/raw/main/desert.png)](#)
[![comet](https://github.com/wangell/pleroma-docs/raw/main/comet.png)](#)

Pleroma is a distributed operating system/VM inspired by the original vision of Alan Kay's Smalltalk, the E programming language, and Plan9.

Pleroma is the system, Hylic is the language:

```
~sys
~io

ε HWEntity
	δ hello-world() -> u8
		loc a : u8 := 1 + 1

		? a > 1
			#t io.println("Hello, world!")
			#f io.println("Goodbye, world!")

		loc ls : [u8] := [1, 2, 3, 4]
		x | ls
			io.println(x)

		↵ a + 1

	λ pure-func(a)
		↵ 5 * pure-func2(2)

	λ pure-func2(a)
		↵ 5 * a

ε HelloWorld
	δ main(env: sys.env) -> int
		far z : HWEntity = vat.create(HWEntity)
		z ! hello-world()
```

## Features

### Sync, async, and promise-based message passing on both local and far references (optionally disabled)
```
ε EntityEx
	δ main(env: sys.env) -> ()
		loc foo : EntityEx()
		far bar : vat.create(EntityEx)

		foo ! do-it()
		foo.do-it()
		var prom = foo <- do-it()
		prom ->
			prom.do-something()
	...
```

### Constraint-based actor management
Kernel satisfies constraints on entities during instantiation.
```
ε EntityEx
	- unique
	- memory:256m
	- custom-constraint: #t
	...
```

### Capability-based security
Pointer arithmetic is disallowed and objects can only be accessed by reference.  The stdlib provides provides out-of-the-box support for auditing and accountability.

### Purity-marked functions
Functions without side-effects can be marked and statically checked.

### Inter-cluster natives
Alien type allows creation/reference to proxy objects, i.e. objects on other clusters can be referenced without instantiation.

```
~sys
~wikipedia

ε WikipediaFetcher
	δ main(env: sys.env) -> ()
		aln wiki: wikipedia.Wikipedia = wikipedia.Wikipedia()
		wiki ! get-article(4)
```

### Grouping of object graphs (like E)
Entities that should share the same object graph are grouped in vats.  Each vat is assigned to a node for processing, rather than every single entity being assigned.  Calling a method synchronously in the same vat happens the way regular function calls happen via the stack, no network/message queue involved.

### Paravirtualization + bare-metal RISC-V support
Running under Xen/KVM and managed-mode RISC-V (currently targeting https://www.sifive.com/boards/)

### Strong distributed primitives support out-of-box

### A human editor
See [Charisma](#Charisma).

### Other
- Support for sub-majority consensus clusters (1 - 2 nodes)
- ARQ networking
- Kernel-managed distributed KV-store
- Message bus
- Single-inheritance + interfaces
- Kernel-managed ingresses, job queues, scheduling, etc.
- Strong supervisor + process-management support.  Handling exceptions + errors in native entities as well as external processes is made extremely easy.
- Designed as a better Kubernetes
- Focus on faul tolerance/supporting temporary nodes - cell phones, computers that turn on/off (laptops), etc.  Data/entities seamlessly move and continue to operate.
- Intelligent kernel.  Can handle auto-grouping and scheduling of objects.

## Charisma

![Charisma demo](https://raw.githubusercontent.com/wangell/pleroma-docs/main/vkoko.png)

### Programming is not text editing

# Links
- [E Programming Language](http://www.erights.org/)
- [Notation as a Tool of Thought](https://dl.acm.org/doi/pdf/10.1145/358896.358899)
- [Alan Kay: The computer revolution hasn't happened yet](https://www.youtube.com/watch?v=oKg1hTOQXoY)
