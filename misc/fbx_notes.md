
## Version incompatability

### Properties

Modern node properties are stored in an object called `Properties70`. Exporting
with older format seems to use `Properties60` (probably matching the FBX
version 6x00).

### Arrays

In new files (at least FBX version >= 7100) there is dedicated support for
arrays. In binary they use the lower-case type specifiers such as `i` or `d`.
The ASCII format also has support for arrays using `* Num { a: Data }` syntax.

```
Edges: *12 {
	a: 0,2,4,6,10,3,1,7,5,11,9,15,13
}
```

Older versions have the array data as separate values. The ASCII format may
have mixed floating point and integer values in the array while the binary
data types seem pretty consistent.

```
UV: 0.375,0,0.625,0, ...
```

The binary parser interprets multiple values as a single array using a custom
array encoding `ufbxi_encoding_multivalue` (`'UFmv' = 0x766d4655`).

