# Runner addresses in Scarier's comments

Scarier's ADRIFT 3.7–4.0 code is measured against the four original Windows
Runners, and its comments cite the places in them that a behaviour comes
from: `generaltasks 460D6C`, `Proc_19_37_458E6C`, `loc_45E682`,
`MemVar_4941F4`. This note says how to read those citations and where the
listings they point into live. The comments do not repeat it.

## The listings

The decompiles live outside the repository, in `~/Adrift_decompile/`. They
are VB Decompiler output of `run370.exe`, `run380.exe`, `run390.exe` and
`run400.exe`, one tree per build, plus `run400-analysed/` (a more readable
but incomplete rendering of 4.0) and an `index/` directory that makes them
searchable. Its own `README.md` describes the trees.

To find what an address is:

```sh
cd ~/Adrift_decompile
python3 index/find.py 458E6C            # every build whose code holds it
python3 index/find.py -v run390 45D3E0  # one build only
python3 index/find.py 0008C200          # old P32Dasm RVAs resolve too
python3 index/find.py -c generaltasks   # calls and callers
python3 index/find.py -s "already got"  # by a string the code prints
```

Each hit gives the procedure that holds the address, its module and line
count, whatever has been measured about it (`index/annotations.tsv`), and
the file and line to open in each listing. When a measurement settles what a
procedure does, add a row to `annotations.tsv` so the next reader sees it.

## Which build

The four builds were compiled separately, so their code ranges overlap: the
same number can be an address in `run390` and a different one in `run400`.
A citation is read in one build only, chosen as follows:

1. **A build named with it** wins: `run390 442B7F`, `run380 @00040BCE`,
   "run400 fails the same way (480B6F)".
2. **Otherwise, the build the comment is about**: an address in a paragraph
   that has already named its build, or in a function header that has, is in
   that build.
3. **Otherwise, the code's own version**: an address in a `_400` file or
   function, or in a branch reached only at version 4.00, is in `run400`; one
   in a `_390` or `_pre400` function belongs to the version the branch
   handles.
4. **The name decides between builds.** Procedure names that survive in the
   older Runners (`takes`, `insides`, `checktask`, `passrest`, `endmessage`,
   `generaltasks`) are 3.7–3.9 names, while `Proc_M_N_ADDR` names and
   `Sub_20_*` / `Sub_22_*` (modules `mdlSpreadTheLoad` and `General`) are
   4.0's. A name borrowed back onto a 4.0 procedure by `index/` (an "aka")
   is shown as such by `find.py`.

Addresses from `494000` up are run400 globals (`MemVar_4941F4`,
`MemVar_4941EC`, `494281`), not code; `find.py` does not list them, but the
listings name them wherever they are read or written.

When none of these rules settles the build, name it in the comment.

## Notations

| Form | Example | Meaning |
|---|---|---|
| bare address | `458E6C` | an address in the build's image, base `0x400000` |
| `loc_` | `loc_45E682` | the same, as VB Decompiler labels a jump target |
| `Proc_M_N_ADDR` | `Proc_19_37_458E6C` | VB Decompiler's name for an anonymous procedure: module `M`, procedure `N`, entry `ADDR` |
| `MemVar_` | `MemVar_4941F4` | a module-level variable at that address |
| `@` + 8 digits | `@0008C200` | a P32Dasm RVA (no image base) at the start of a procedure's body; add `0x400000` to compare it with the others |
| `@` + 6 digits | `@462B12` | a bare address, written with `@` in some older comments |
| `Sub_M_N` | `Sub_20_11` | P32Dasm's procedure name; module numbers run one higher than in `Proc_` names, and `index/runNNN/oldnames.tsv` maps one to the other |
| `A-B` | `453BEC-453BFE` | a range of instructions inside one procedure |
| `[2E]`, `[46]` | | a field offset in the record the procedure is walking |
