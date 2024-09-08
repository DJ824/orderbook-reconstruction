## Low Latency in C++ 

### Understanding cache and memory access 
- it is common for data structures and algos that are sub-optimal on paper to outperform ones that are faster on paper, reason for this is higher cache and mem access costs for the optimal solution outweighting the time saved becasue of the reduced number of instructions the processor needs to execute 
- memory hierachy works such that cpu first checks register->l0 cache->l1 cache->l2 cache->memory->disk, higher is faster but smaller storage, and vice versa 
- we have to think carefully regarding cache and mem access patterns for the local algo, as well as globally, if we have a function with quick execution time but pollutes the cache, degrades overall performance as other componens will incur cache miss penalites

### Understanding how c++ features work under the hood 
- a lot of the higher level abstractions available in c++ improve ease of development, but may incur higher latency costs 
- for example, dynamic polymorphism, dynamic mem allocation, and exception handling are best avoided or used in a specific manner as they have a larger overhead 
- many traditional programming practices suggest the developer break everyuthing into numerous small functions, use OOP principles, smart pointers, etc, which are sensible for most applications but need be used carefuly in low latency contexts to avoid the overhead 


### Avoiding pitfals and leverage c++ features to minimize application latency 
1) Choosing Storage 
- local variable screated within a function are stored on the stack by defualt and the stack memory is also used to store function return values, assuming no large objects are created, the same rang eof stak storage space is reused a lot, resulting in great cache performance due to locality of reference 
- register variables are closest to cpu and fastest form of storage, extremely limited, compiler will try to use them for local variables that are used the most 
- static/global varaibles are inefficient from the prespective of cache performance becasue that memory cannot be reused for oher variables
- *volatile* keyword tells compiler to disable optimizations taht rely on the assumption that the variable value does not change without the compilers knowledg, only be used carefully in multithreaded use cases since it prevents optimizations such as storing vars in registers snad force flushing them to main memory from cache everytime the val changes 
- one example of c++ optimization technique taht leverages storage choice optimization is small string optimization (SSO), this attempts to use local storage for short strings if they are smller than a certain size, vs the slow dynamic mem allocation normally used 

2) Choosing data type
- c++ integer operations are super0fast as long as the size of the lragest register is larger than the integer size, integers smaller or larger than register size are sometimes slower, because the 
processor must use multiple registers for a single variable and apply carry-over logic for large integers 
- signed and unsigned integers are equally fast, but sometimes unsigned is a tiny bit faster as we dont have to check the sign bit 

3) Using casting and conversion operations
- converting btwn signed and unsigned integers is free, converting from smaller size to larger one takes a single clock cycle, larger to smaller is free 
- conversion btwn floats, doubles and long doubles is free, signed/unsigned to float/double takes a few cycles 
- floats to ints is expensive, 50-100 cycles, it conversions are on critical path, use special methods to avoid these expensive conversions such as inline assembly, or avoid converting altogether 
- convterting ptrs from one type to antoehr type is completely free, safe is another question 
- type casting a ptr to an object to a ptr of a diff object violates the rule that 2 ptrs of diff types cannot pt to same mem location
```c++
int main() {
    double x = 100; 
    const auto orig_x = x; 
    auto x_as_ui = (uint64_t *) (&x); 
    *x_as_ui |= 0x80000000000000; 
    printf("orig_x, x, &x, x_as_ui") // 100.00, -100.00, 0x7fff1e6b00d0, 0x7fff1e6b00d0 
}
```
- this shows type casting a ptr to be a diff object 
- casting is usually free except `dynamic_cast`, as this checks whether the conversion is valid using time-time type information, which is slow and can throw exceptions 

4) Optimizing numerical operations 
- double precision calculations take about the sme time as single-precision operations, adds are fast, mults little slower, div is slowest 
- int mult = 5 cycles, float mult = 8 cycles, int add = single cycle, float add = 2-5 cycles, float div & int div = 20-80 cycles 
- compilres will try to rewrite and reduce expressions to prefer faaster operations such as rewriting divs to be mults by reciprocal, mult & div by powers of 2 are faster because compiler rewrites them to be bit shift operations 
- mixing int and floats in expressions should be avoided because they force type conversions 

5) Optmizing boolean and bitwise operations 
- booll operarations such as logcail AND (&&) and OR(||) are exalauted such that for AND, if first operand is false, does not check second, and for OR, if first operand is true, does not check second 
- we can order the operands for AND by probability, lower to higher, and opposite for OR - this is called short circuiting 
- bitwise operations can also help speed up other cases of boolean expressions by treating each bit of an integer as a single boolean variable, adn then rewriting expressions involving compariosns of multiple booleans with bit-masking ops 
```c++ 
// market_state = uint64_t, PreOpen, Opening, Trading enum vals 0x100, 0x010, 0x001
if (market_state == PreOpen || market_state == Opening || market_state == Trading) {
    do_something(); 
    }
   
if (market_state & (PreOpen | Opening | Trading) { 
    do_something(); 
    } 
```

6) Initialzing, destroying, copying, and moving objects 
- constructors and destructors for developer defined classes should be light as possible, since they can be called without expectation, also allows compiler to inline these methods to improve performance 
- same with copy & move construcotrs, in cases where high levels of optimizations are req, we can delete the default and copy constructors to avoid unecessary copies being made 

7) Using references and ptrs
- accessing objects thru refs and ptrs is as efficient as direct access, only disadvantage is that these take up an extra register for the ptr, and the cost of extra deference instructions 
- ptr arithmetic is as fast as integer arithmetic except when computeing the difference btwn ptrs requires a division by the size of object 
- smart ptrs such as `unique_ptr`, `shared_ptr`, and `weak_ptr`, use the resource aquisition is initialization paradigm (RAII), extra cost with `shared_ptr` due to reference counting but generally add little overhead unless there are a lot 
- ptrs can prevent compiler optimizations due to pointer aliasing, while it may be obvious to the user, as compile time, the compiler cannot guarantee that 2 ptr vars in the code will nver pt to same mem address

8) Optimizing jumping and branching 
- instructions adnd ata are detched and decoded in stagtes, when there is a branch instruction, processor tries to predict which branch will be taken and fetches and decodes instructions from that branch 
- in the case of misprediction, takes 10+ cyclcers to detect, and then uses more cycles to fetch the correct branch instructions 
- `if-else` branching most common, avoid nesting these statements, try to structure such taht they are more predictable 
- avoid nesting loops 
- `switch` statements are branches with multiple jump targest, if label values are spread out, compiler treats switch statements as long sequence of if-else branching trees, assign case lebl values that increment by 1 and ascending order so they are implemented as jump tables, which are significantly faster 
- replacing branching with table lookups containing different output values in source code is a good optimization technique 
- *loop unrolling* duplicates the body of the loop multiple times in order to avoid the checks and branching that determines if a loop should continue, compiler will attempt to unroll loops if possible 
```c++ 
int a[5]; a[0] = 0; 
for (int i = 1; i < 5; i++)
    a[i] = a[i - 1] + 1; 
// unroll loop 
int a[5]; 
a[0] = 0; 
a[1] = a[0] + 1; a[2] = a[1] + 1; 
a[3] = a[2] + 1; a[4] = a[3] + 1; 
```
- compile time branching using an `if constexpr (condition-expression) {}` format can help by moving the overhead of brnaching to compile time only if the `condition-expression` can be something taht is evaluated at compile time (compile time polymorphism/template metaprogramming) 