# LLVM IR Plus Plus
---

Three address code is a popular choice in academia working around static analysis. They are used in the dataflow equations modelled for various analysis.

Most classic dataflow analysis that are studies at the introductory level are not in SSA form. LLVM being quite popular in academia, ends up witnessing many trying to model those equation just to fit over the SSA form. This approach works well in simple analysis like liveness but gives incomplete results for complex analysis like alias analysis.

LLVM IR++ is an out of tree analysis pass which generate abstractions over LLVM IR formed from C++, such that it can be directly used for demostrations of static analysis.


## What it does?

For the following C++ code.
```cpp
class A{
	public:
	int a;
};
int main(){
	int i;
	A *obj;
	i = obj->a; 
	return 7;
}
```
The LLVM IR formed is
```cpp
define dso_local i32 @main() #0 !dbg !7 {
entry:
  %retval = alloca i32, align 4
  %i = alloca i32, align 4
  %obj = alloca %class.A*, align 8
  store i32 0, i32* %retval, align 4
  %0 = load %class.A*, %class.A** %obj, align 8, !dbg !19
  %a = getelementptr inbounds %class.A, %class.A* %0, i32 0, i32 0, !dbg !20
  %1 = load i32, i32* %a, align 4, !dbg !20
  store i32 %1, i32* %i, align 4, !dbg !21
  ret i32 7, !dbg !22
}
```

It converts,
```c
store i32 %1, i32* %i, align 4, !dbg !21
```
to:
```cpp
i32* i  = %class.A** tmp  -> a
```

## Format of meta data

Instruction is assumed to be of form,
```cpp
Instruction: LHS = RHS
where,
LHS = x | *x | x -> f | x.f
RHS = y | *y | y -> f | y.f | &y
```
Meta data generated of form,
```cpp
type base symbol optional = (?&) type base symbol optional
class.A a -> f = i32* tmp
```

## Try it

```sh
$ export LLVM_HOME=/usr/lib/llvm-8 
$ bash test
```
