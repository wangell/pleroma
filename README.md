# pleroma

Pleroma is a distributed operating system/VM inspired by the original vision of Alan Kay's Smalltalk, the E programming language, and Plan9.

```
~io

ε HWEntity
	δ hello-world() -> ()
		loc a : u8 := 1 + 1
		↵ a + 1

		? a > 1
			#t io.println("Hello, world!")
			#f io.println("Goodbye, world!")

	λ pure-func(a)
		↵ 5 * pure-func2(2)

	λ pure-func2(a)
		↵ 5 * a

ε HelloWorld
	δ main(env: sys.env) -> int
		far z : HWEntity = vat.create(HWEntity)
		z ! hello-world()
```

### Programming is not text editing
