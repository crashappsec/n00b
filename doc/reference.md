# N00b Language Reference

This language reference is intended to document the N00b language as
thoroughly as possible.

Currently, the language is being (re)implemented. This reference
should document both the current state of the language, as well as the
intended state of the language, where possible. It will attempt to
clearly differentiate between the two.

This document is in progress, and nowhere near complete yet.


## Lexical analysis

Like many languages, source files are processed in two stages,
*lexical analysis* and *parsing*. The lexical analysis divides the
program up into *tokens* that are then passed to the parser, which
builds a tree representing the program, using the tokens as inputs.

The lexer expects input to be encoded in UTF-8.

N00b's tokenization is mostly typical for a programming
language. Below, we'll give an overview of the semantics of the
tokens. However, there are some significant items to note:

1. Identifiers follow the Unicode standard for identifiers, but also
   allow underscores and dollar signs in any position. Currently, the
   parser does allow the dollar sign in any position where an
   identifier is allowed. However, we intend to limit this, so that
   the dollar sign is only available for special symbols provided by
   the language.

2. Literal tokens all may take literal modifiers, that are tokenized
   with the token. That not only includes integer, string and
   character literals, it also includes the closing delimiter for
   other literal types. Literal modifiers are denoted by an
   apostrophe, followed by an identifier. For instance,
   `"[em]foo[/]"'r` is a valid string literal, with a modifier (in
   this case, the modifier will cause the string to be interpreted as
   rich text). Additionally, the following will (pretty soon) create a
   table, setting the first row as a header row:

```
[["Col 1", "Col 2"]'h, ["item1", "item2"]]'t
```

3. When literal modifiers are used, non-container types may have their
   own mini-language to interpret the content, which is applied in the
   parsing phase. The rules associated with each literal modifier do
   not change the initial lexical scanning.

4. There are three different syntaxes for comments. Two of them are
   the common forms of line comments: `#` and `//` delimits a comment
   until the end of the line. C-style multi-line comments are also
   allowed, which start with `/*`, continuing until the sequence `*/`
   is encountered.

5. Newline tokens are produced, except _one_ newline is skipped at the
   end of line comments, or after punctuation tokens that cannot
   appear at the end of the line. Particularly, braces an parentheses
   will eat a newline for the starting token (e.g., `[`), but not for
   the ending token.

6. Other whitespace is grouped into tokens that are produced by the
   lexer, but are always ignored in the parser.

7. Most statements require an 'end of statement' marker, which can be
   a newline token, a line comment, a semicolon, or the end of file.

8. Other newline tokens are generally an error. With this approach,
   when a developer forgets to finish a line of code and follows it
   with long comments etc. the compiler can usually keep the error
   closer to the actual problem.

### Keywords

N00b has two types of keywords: _Reserved Keywords_ can only be used
where the language specifies and can never be an
identifier. _Contextual Keywords_ can be used as identifiers in any
context where the parser doesn't interpret it as a keyword.

Not all of N00b's reserved keywords are implemented yet.

| Keyword | Description | Implementation Notes |
|---------|-------------|--------|
| True | literal of type `bool` | |
| true | "" | Same as `True`; since other languages differ on the capitalization, we accept both to make it easier to go back and forth. |
| False | "" | |
| false | "" | same as `False` |
| nil | The 'nil' literal, indicating that there is no valud value set for the type. | Not implemented yet; currently no values of any time may ever be `nil`.|
| in | Used in for loops. | |
| var | Used to explicitly declare variables. | |
| let | Used to explicitly declare variables that may only be assigned to a single time in the source code. | |
| const | Used to explicitly declare variables that may only be assigned a single time, and the value must resolve to a constant value at compile time. | |
| global | Used to make variables available outside the module that defines them, and to import global variables from other modules. | |
| private | Used to restrict the access of functions to the module that defines them. | |
| once | Used as a modifier on functions, indicating they will only be called a single time, and then the result cached for subsequent invocations. | This is not yet implemented; it will be implemented using a lock during the first invocation, to ensure no race conditions. |
| lock | Used to change the permissions on an _attribute_. | Reserved, but not yet implemented. |
| is | Comparison | Currently identical to `==` |
| and | Logical `and` | |
| or | Logical `or` | |
| not | Logical `not` | |
| if | Per normal | |
| elif | "" | |
| else | "" | |
| for | Per normal | |
| while | Per normal | |
| break | Per normal | |
| continue | Per normal | |
| return | Per normal | |
| enum | Delimit an enumeration definition | |
| func | Delimit a function | |
| object | Delimit an abstract data type definition | Not yet implemented |
| typeof | Delimit a switch-like statement that queries the type of a variable at runtime. | At compile time, the variable cannot have a concrete type. |
| switch | Delimit a switch statement | |
| from | Used in ranged `for` loops | |
| to | Used in range specifications. | In some contexts, colons or commas may also be allowed, but `to` is always allowed in range operations. |
| case | Used like in C, for both `switch` statements and `typeof` statements | |
| infinity | A float literal | |
| NaN | A float literal, representing 'Not a Number' | |

Additionally, until the FFI is re-implemented, `print` is a reserved
keyword. This will change soon, and will no longer be special.

The following are current and expected *contextual keywords*:

| Keyword | Status | Notes |
| ------- | ------ | ----- |
| choice | | |
| choices | | |
| default | | |
| exclude | | |
| exclusions | | |
| hide | | |
| hidden | | |
| locked | | |
| require | | |
| required | | |
| range | | |
| type | | |
| validator | | |
| user_def_ok | | |
| allow | | |
| allowed | | |
| field | | |
| named | | |
| singleton | | |
| root | | |
| assert | | |
| use | | |
| parameter | | |
| extern | | |
| confspec | | |

Additionally, type names are all contextual keywords.

The following are punctuation tokens that will eat one trailing newline:

```
+ += - -= -> * *= / /= % %= < <= << <<= > >= >> >>= ! != , . { [ ( & &= | |= ^ ^= =
```

The following punctuation tokens do NOT eat a following newline:
```
~ ` : } ] )
```

### Literal syntax

Different literal syntaxes can be used to declare literals of
different types. Some types can accept literals written in multiple
syntaxes. Each syntax has a "default" type that is selected if there
is no literal modifier, which should usually be the intutive type.

Currently, N00b has the following literal syntaxes:

- `'` is the character delimiter. It parses character literals, only
  accepting a single character, and then expecting a matching single
  quote. Commom escape sequences are generally handled. Literal
  modifiers *are* allowed. The default type for this syntax is `char`,
  which is guaranteed to be store in a 24-bit or greater value,
  allowing it to represent any valid Unicode codepoint.

- `"` is the typical string delimeter. `"""` is an alternate
  syntax. Currently, this preserves embedded newlines, and does not do
  any trimming. The default type is the regular string type. Other
  literal modifiers here generally will control formatting and
  potentially parsing of the string.

- 0x starts hex literals, which accept any number of valid hex
  characters. In most contexts, the number will be limited by the size
  of the resulting data type. The default type is the 64-bit `int`
  type.

- We intend to add 0b for binary characters, but have not implemented
  it yet.

- Other tokens that start with a digit will end up as either integer
  tokens or float tokens. The rules here are consistent with most
  other languages, including accepting basic `e` notation.

- _List syntax_ starts with `[` and ends with `]`. As is typical, it
   accepts comma delimited items. The default type here is the `list`
   type, which is a lock-free and threadsafe list (of whatever single
   type is contained). Currently, there is a non-threadsafe list type,
   and several other lock-free types implemented but not yet exposed,
   including rings, queues and stacks.

- _Dict syntax_ starts with `{` and ends with `}`, and contains key /
  value pairs separated by `:`. Currently, the `dict` type is the only
  type with this syntax.

- _Set syntax_ looks like dict syntax, except that it only accepts
  single items, like with a list. Currently, the `set` type is the
  only type with this syntax.

## Types in N00b

N00b is a strongly typed language, that is also fully type
inferenced.  This means that, while types may be specified, they do
not need to be. In cases where a type cannot be statically bound,
N00b will automatically add runtime checks.

### Builtin Types

Builtin types are either _value types_ or _reference types_. Value
types are limited to numeric and boolean types that can be represented
in 64-bits or less.

Reference types that are used as parameters to function calls are
always passed by reference. If they are mutable types (and not
constant), they may be modified by the receiving function, though the
underlying object cannot be changed out completely.

We do intend to add `ref` as a builtin parameterized type will allow
for passing around mutable references.

The default `list` type is currently fully threadsafe and
lock-free. As a result, currently, it does not have an `append`
operation. We expect to add one soon, via a per-instance lock.

Strings are immutible, and may be stored either as utf8 or utf32. They
are currently converted automatically as needed.

Currently, operations on containers types, including `+=` will create
a new object, instead of modifying the object. We expect that to change.

For the string type, the contents are immutable, but the styling
information is not.

## Whitespace rules

## Scoping rules

All scoping rules are currently static:

1. The global scope contains variables explicitly placed in it with a
`global` declaration. It also contains all functions that are not
explicitly marked `private`. Modules do not automaticly have access to
global variables in inherited modules; you have to explicitly import
them with another `global` declaration.

2. The module scope contains variables declared or used in module code
outside of functions. The module symbols are available to functions
unless the function explicitly declares a variable that masks it.

3. If variables are declared in functions, it will shadow anything at
the module level. If variables are not declared inside the function,
but they are declared or used in the module, then the variable will be
used from the module scope.

4. Variables explicitly declared inside a function, and variables only
used inside a function have the scope of the function (as opposed to
block scoped). However, `for` index variables are block scoped, as are
special variables like `$i` and `$last`.

5. Any declared functions are automatically exported into the global
scope, unless the function is explicitly marked private, or there is
already a global of the same name. All functions in the standard
library are available without importing.

## Once functions

The body of functions marked with the `once` modifier will only ever
run a single time. This is intended for initialization code that needs
to run a single time.

After that, if the function has a return type, the same value is
returned every time.

If there is no return value, nothing happens.

## Special Variables

Currently, N00b supports:
- `$result` for return values from functions. This is the same as explicitly providing a return value via an argument to a `return` statement.
- `$i`, in loops.
- `label$i` for labeled loops, which can be used to access the iteration count
  in nested scopes.
- `$last` to provide the last possible value of `$i` (assuming no `break` statement).
- `label$last` for labeled loops.

This can be particularly useful for special-casing the last
iteration. In other languages, one might say,
`if i + 1 == len(obj) { ... }`

But that's easily expressed in N00b with:
`if $i is $last { ... }`

Notice that `$i` and `$last` track the number of iterations, not the
values being iterated over. For instance, a ranged for:
```
for i in 5 to 7 {
    print($i)
    print($last)
}
```

Will print:
```
0
1
1
1
```

The user may assign `$result`, but not special loop variables.

# Current BNF

I try to keep this in reasonable sync, but I am pretty sure there are some
places where this has strayed from the implementation. I'll try to do
the side-by-side with the code sometime soon.

```
toplevel ::= docString?
             (';' | '\n' | enumStmt | lockAttr | ifStmt | forStmt | whileStmt |
              typeOfStmt | valueOfStmt | funcDef | varStmt | letStmt |
              globalStmt | constStmt | assertStmt | useStmt | parameterBlock |
              externBlock | confSpec | assign | section | expression)*

bodyItem ::= lockAttr | ifStmt | forStmt | whileStmt | typeOfStmt |
             valueOfStmt | continueStmt | breakStmt | returnStmt |
             varStmt | letStmt | globalStmt | constStmt | useStmt

body          ::= '{' docString? (bodyItem? EOS)* '}'
optionalBody  ::= body |

enumStmt     ::= "enum" ID? '{' enumItem (',' enumItem)* '}'
enumItem     ::= ID (('=' | ':') expression)?
lockAttr     ::= '~' (assign | memberExpr)
ifStmt       ::= "if" expression body ("elif" expression body)* ("else" body)?

forStmt      ::= labelStmt? (rangedFor | containerFor)
containerFor ::= "for" ID (',' ID) "in" expression body
rangedFor    ::= "for" ID ("from" | "in") expression "to" expression" body
whileStmt    ::= labelStmt? "while" expression body
labelStmt    ::= ID ':'

typeofStmt   ::= labelStmt? "typeof" memberExpr '{' (oneTypeCase)+ caseElseBlock? '}'
valueOfStmt  ::= labelStmt? "switch"" expression '{' (oneValueCase)+ caseElseBlock? '}'

continueStmt ::= "continue" ID?
breakStmt    ::= "break" ID?
returnStmt   ::= "return" expression?


oneTypeCase  ::= "case" typeCaseCondition (caseBody | body)
oneValueCase ::= "case" (valueCaseCondition (caseBody | body)

typeCaseCondition  ::= typeSpec (',' typeSpec)
valueCaseCondition ::= expression (('to'|':') expression)? (',' valueCaseCondition)?
caseElseBlock      ::= 'else' (caseBody | body)

caseBody           ::= ':' (bodyItem EOS)*

funcDef     ::= "func" ID formalList -> returnType body

varArgsFormal ::= '*' ID
formal        ::= ID (':' typeSpec)
argList       ::= formal (', ' formal)* varArgsFormal?
formalList    ::= '(' (varArgsFormal | argList | <empty> ) ')'

varStmt     ::= "var" ("const")? varSymInfo EOS
letStmt     ::= "let" ("const")? varSymInfo EOS
globalStmt  ::= "global" ("const")? varsymInfo EOS
constStmt   ::= "const" ("global" | "var" | "let")? varSymInfo EOS
varSymInfo  ::= ID (',' ID)* (':' typeSpec)


assertStmt  ::= "assert" expression EOS
useStmt     ::= "use" ID ("from" STR) EOS

# Parameter blocks are actually more constrained than this; but we
# implemented it outside the parsing phase; will update the grammar
# soon, but also a TODO to move it all into the parser where it should
# be.
parameterBlock  ::= "parameter" (memberExpr | paramVarDecl) optionalBody
paramVarDecl    ::= "var" oneVarDecl

externBlock     ::= 'extern' ID externSignature docString ('{' externField '}')*
externSignature ::= '(' (externParam (',', externParam)*)? ')' '->' externParam
externParam     ::= (ID ':')? CTYPE+

externField ::= externLocalDef | externDllName | externPure |
                externHolds | externAllocs

# IDs in a spec are restricted to declared const values (enums included).
fieldType        ::= 'type'    ':' (typeSpec | '->' ID) EOS
fieldDefault     ::= 'default' ':' (ID|literal) EOS
fieldRequire     ::= 'require' ':' (ID|BOOL) EOS
fieldChoice      ::= 'choice' ':' (bracketLit | ID) EOS
fieldExclusions  ::= 'exclusions' ':' ID (',' ID)* EOS
fieldLock        ::= 'lock' ':' (ID|BOOL) EOS
fieldValidator   ::= 'validator' ':' (ID|callbackLit) EOS
fieldHidden      ::= 'hidden' ':' (BOOL | ID) EOS
fieldRange       ::= 'range ':' (int | ID) ':' (int | ID) EOS
fieldProps       ::= fieldType | fieldDefault | fieldRequire |
                     fieldChoice | fieldExclusions | fieldLock |
                     fieldValidator | fieldHidden | fieldRange
fieldSpec        ::= 'field' ID '{' docString (fieldProps)* '}'
allow            ::= 'allow' ':' ID (',' ID) EOS
secRequire       ::= 'require' ':' ID (', ID) EOS
sectionProp      ::= ('user_def_ok' ':' (BOOL | ID) EOS) |
                     ('validator' ':' (ID|callbackLit) EOS)
sectionContents  ::= (sectionProp | fieldSpec | allow | secRequire)*
rootSpec         ::= 'root' '{' docString sectionContents* '}'
instantiableSpec ::= 'multi' ID '{' docString sectionContents* '}'
singletonSpec    ::= 'singleton' ID '{' docString sectionContents* '}'
specObject       ::= singletonSpec | instantiableSpec | rootSpec
confSpec         ::= confSpec '{' docString specObject* '}'

# Currently, only dll can appear multiple times. It should eventually
# take a list instead.
externDllName  ::= "dll" (":" | "=") STRING EOS
# Actually, formalList is slightly wrong; we constrain to require var names.
externLocalDef ::= "local" (":" | "=") ID formalList -> returnType EOS
externPure     ::= "pure" BOOL
externHolds    ::= "holds" ID+
externAllocs   ::= "allocs" | ID* ("return") | ID+

assign       ::= expression assignmentOp expression EOS
assignmentOp ::= ':' | '=' | '+=' | '-=' | '*=' | '/=' | '|=' | '&=' |
                 '^=' | '>>=' | '<<='
section      ::= ID body | ID (ID | STR) opttionalBody
docstring    ::= (STR ('\n' STR))?

oneVarDecl  ::= ID (":" typeSpec)

typespec ::= typeVariable | funcType | objectType | listType | dictType |
             tupleType, refType, typeTypeSpec typeOneOf, typeMaybe |
             builtinType
funcType ::= '(' typeSpec? (',' typeSpec)* (',' '*' typeSpec)? ')' returnType |
             '(' '*' typeSpec ')' returnType

returnType   ::= ('->' typeSpec)?
objectType   ::= "struct" '[' (ID | typeVariable) ']'
typeTypeSpec ::= "typespec" ('[' typeVariable ']')?
typeVariable ::= '`' ID
listType     ::= "list"  '[' typeSpec ']'
dictType     ::= "dict"  '[' typeSpec ']'
refType      ::= "ref"   '[' typeSpec ']'
typeMaybe    ::= "maybe" '[' typeSpec ']'
typeOneOf    ::= "oneof" '[' typeSpec (',' typeSpec)+ ']'
tupleType    ::= "tuple" '[' typespec (',' typeSpec)+ ']'
builtinType  ::= BITYPE

expression    ::= exprStart (orExpr*)
orExpr        ::= ("||" | "or")  expression | andExpr
andExpr       ::= ("&&" | "and") andExprRHS | neExpr
andExprRHS    ::= exprStart (andExpr)*
neExpr        ::= "!=" neExprRHS | eqExpr
neExprRHS     ::= exprStart (neExpr)*
eqExpr        ::= "==" eqExprRHS | gteExpr
eqExprRHS     ::= exprStart (eqExpr)*
gteExpr       ::= ">=" gteExprRHS | lteExpr
gteExprRHS    ::= exprStart (gteExpr)*
lteExpr       ::= "<=" lteExprRHS | gtExpr
lteExprRHS    ::= exprStart (lteExpr)*
gtExpr        ::= ">" gtExprRHS | ltExpr
gtExprRHS     ::= exprStart (gtExpr)*
ltExpr        ::= "<" ltExprRHS | plusExpr
ltExprRHS     ::= exprStart (ltExpr)*
plusExpr      ::= "+" plusExprRHS | minusExpr
plusExprRHS   ::= exprStart (plusExpr)*
minusExpr     ::= "-" minusExprRHS | modExpr
minusExprRHS  ::= exprStart (minusExpr)*
modExpr       ::= "%" modExprRHS | mulExpr
modExprRHS    ::= exprStart (modExpr)*
mulExpr       ::= "*" mulExprRHS | divExpr
mulExprRHS    ::= exprStart (mulExpr)*
divExpr       ::= "/" divExprRHS | shrExpr
divExprRHS    ::= exprStart (divExpr)*
shrExpr       ::= ">>" shrExprRHS | shlExpr
shrExprRHS    ::= exprStart (shrExpr)*
shlExpr       ::= "<<" shlExprRHS | notExpr
shlExprRHS    ::= exprStart (shlExpr)*
notExpr       ::= 'not' expression

expressionStart ::= literal | accessExpr

literal ::= (INT | HEX | FLOAT | STR | CHR | BOOL | bracketLit | tupleLit |
            dictLit) litmod? | OTHERLIT | callbackLit | typeSpec | 'nil'

bracketLit     ::= '[' (expression (',' expression)*)? ']'
tupleLit       ::= '(' expression (',' expression)+ ')'
dictLit        ::= '{' (kvPair (',' kvPair)* )?
kvPair         ::= expression ':' expression
litMod         ::= '\'' ID   # No space allowed
callbackLit    ::= 'func' ID typeSpec?
accessExpr     ::= (parenExpr | ID) (memberExpr | indexExpr | callActuals |
                                     oneArgCastExpr | castExpr)*
parenExpr      ::= '(' expression ')'
memberExpr     ::= '.' (ID (memberExpr)?)
indexExpr      ::= '[' expression (':' expression) ']' (oneArgCastExpr)?
callActuals    ::= '(' (expression (',' expression)*)? ')'
castExpr       ::= 'to' '(' expression typeSpec ')'
oneArgCastExpr ::= '.' 'to' '(' typeSpec ')'

BOOL  ::= "true" | "false" | "True" | "False"

CTYPE ::= "cvoid" | "cu8" | "ci8" | "cu16" | "ci16" | "cu32" | "ci32" | "cu64" |
           "ci64" | "cfloat" | "cdouble" | "cuchar" | "cchar" | "cshort" |
           "cushort" | "cint" | "cuint" | "clong" | "culong" | "cbool" |
           "csize" | "cssize" | "ptr" | "cstring" | "carray"

BITYPE ::= "void" | "bool" | "i8" | "byte" | "i32" | "char" | "u32" | "int" |
           "uint" | "f32" | "f64" | "string" | "buffer" | "utf32" | "rich" |
           "list" | "dict" | "tuple" | "typespec" | "ipv4" | "ipv6" |
           "duration" | "size" | "datetime" | "date" | "time" | "url"

EOS ::= '\n' | ';' <<or, if followed by a '}' or line comment, then ''>>
```

# Features to re-add from old n00b
- Final mile: params
- Doc API.
- Garbage collection: scan registers (well, not really a re-add...)


# Items for afterward
- Objects
- BNF parsing
- Folding
- Casting
- Mixed
- Varargs functions
- automatic logfd + optional server for log messages
- REPL
- Keyword arguments
- 'maybe' types / nil
- Aspects (before / after / around pattern(sig) when x)
- Threading
- Pretty printing w/ type annotations
- Language server
- Checks based on PDG
- Full-program info on unused fields & fns.
- Some sort of union type.

# Other noted issues to address
- Accept arbitrary characters in the getopts spec.


- Need to deal with tuple assignment to attributes.
- Need to ensure things that must be static consts are actually consts.
- Enforce No default if there is a type param.
- After the check pass, enforce constants.
- Warning on exclusions that are not defined fields.
- Default provided + lock should not lock.
- Lock w/o attr being set should be compile-time error where possible.
- Attr lock applied to non-attrs.
- Calls via callback, possibly once interpreter stops.
- Validation post-run.
- Show source code on errors when available.
- Get all the GC bit functions done / working.
- n00b_format_errors() -> n00b_format_compile_errors()
- Section validation callbacks.
- Randomize typeid on unmarshaling, OR, create a 'universeid'
- That we also use in the hash.
- Disallow '$' for user defined vars?
- TODO: more error checking on option carnality via aliases.
