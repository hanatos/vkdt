# pipeline design

TODO: see pipe.h


# image operation (iop) interface

we'd like to be able to set and get parameters in both an efficient and human
readable manner.

modules and params should have a name as string (8-chars or 8-byte int)
to facilitate both readable interaction as well as efficient comparison:

```
typedef union
{
  char str[8];
  uint64_t id;
}
token_t;
```

the initialisation
```
token_t tkn = {.id = 0xfffful };
```
is simple, the other way around requires us to pad up the string to exactly 8
chars, so we'll need an api function that takes care of this.  hopefully this
code path should never be triggered in performance critical steps (either hard
coded/compile time or when reading human readable input which should be rare).

we can then get and set parameters with a generic interface:

```
set(token_t module, token_t param, value)
```

open issues:
* how to assign module versions here? always pass in through set?
* type system for values? we can only pass int and float to the compute shader,
  so is always float + a size enough?

maybe:
```
setn(token_t module, token_t param, int num, *value)
setv(token_t module, token_t param, int version, value)
setnv(token_t module, token_t param, int num, int version, *value)
```

a storage backend would then either serialise this into human readable ascii form:

```
exposure ev 2.0
exposure black -0.01
colorin matrix 1 0 0 0 1 0 0 0 1
..
```

or if required for efficiency reasons could store it in binary as the uint64
and values in floating point (some special byte to mark end of line)
