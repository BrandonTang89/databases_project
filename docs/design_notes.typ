#import "@preview/codelst:2.0.2": sourcecode
#let title = "Databases Project Design Notes"
#let author = "1075201"
#set document(title: title, author: author)
#set par(justify: true)
#set page(numbering: "1/1", number-align: right,)
#let tut(x) = [#block(x, stroke: blue, radius: 1em, inset: 1.5em, width: 100%)]
#let pblock(x) = [#block(x, stroke: rgb("#e6c5fc") + 0.03em, fill: rgb("#fbf5ff"), radius: 0.3em, inset: 1.5em, width: 100%)]
#let gblock(x) = [#block(x, stroke: rgb("#5eb575") + 0.03em, fill: rgb("#e3fae9"), radius: 0.3em, inset: 1.5em, width: 100%)]
#pdf.attach("design_notes.typ")
#align(center)[
  #text(weight: "bold", 1.75em, title)
  #v(1em, weak: true)
  #text(weight: "medium", 1.1em, author)
]

```
DataTuple:
- left: Int
- right: Int
- lock: XSLock
- alive: Boolean

Relation
- data: stable_vector<DataTuple> 
- leftToRightIndex: HashMap<Int, Group>
- rightToLeftIndex: HashMap<Int, Group>
- Whole relation lock: SLock
- Diagonal Lock: SLock
- Should expose relevant methods to access internals...

Group
- members: vector<DataTuple*>
- lock: SLock

Lock:
- locked_by: flat_set<TID>

XSLock extends Lock:
- is_exclusive: bool (false when locked_by is empty)
- locked_by: flat_set<TID>

SLock extends Lock:

stable_vector<DataTuple, TuplesPerNode>
- a linked list of nodes, each containing an array of DataTuples
- newTuple(left, right) -> DataTuple*

Transaction
- tid: TID
- age: Int
- locks: vector<Lock>
- original_state: vector<tuple<DataTuple*, Bool>> (the state to revert tuples to)

ConflictGraph
- adjacency list<TID, vector<TID>>

Database:
- relations: vector<Relation>
- conflict_graph: ConflictGraph
- transactions: vector<Transaction>

```

- Predicate locks:
  - We need predicate locks to be able to lock the following types of ranges
    - R(-, c) for some constant c [group lock]
    - R(c, -) for some constant c [group lock]
    - R(-, -) for the entire relation [relation lock]
    - R(x, x) i.e. the diagonal of the relation [relation lock]

- Queries
  - Whole relation locks on the first relation, and then either tuple or group locks on all subsequent relations. Acquiring these locks never fail since they are always S locks and we only acquire them in queries
  - Queries don't need to check predicate locks since they cannot create phantom rows

- Updates
  - Updates should check predicate locks whenever modifying tuples
  - Updates should acquire X locks at the tuple level and not take any predicate locks


1.
(a) (i)
Tuple-level locks will be contained within each DataTuple directly. These are `XSLock` objects, that allow for both shared and exclusive locking.  


(b)

(c)
```
maintain a set of variables that we have seen S = {} and a set of tuples W of size |S|

For each R(s, t) in the query: 
  match (s, r) with   
  - (c1, c2) where c1 and c2 are constants                ---- 0
    - acquire an S lock on the tuple (c1, c2)
    - if (c1, c2) in R, continue, else return
  - (c, y)
    - acquire an S lock on the S lock for R(c, -)
    - if y in S
      - W' = W.filter(w -> w.t in R.leftToRightIndex(c))      ---- 1
    - else
      - W' = W x R.leftToRightIndex(c)                        ---- 2
  - (x, c)
    - acquire an S lock on the S lock for R(-, c)
    - if x in S
      - W' = W.filter(w -> w.s in R.rightToLeftIndex(c))      ---- 3
    - else      
      - W' = W x R.rightToLeftIndex(c)                      ---- 4
  - (x, y)
      - if x in S and y in S
        - lock the whole relation
        - W' = W.filter(w -> (w.x, w.y) in R) acquiring tuple locks on R(w.x, w.y)  ---- 5
      - else if x in S
        - for each w in W
          - acquire an S lock on the S lock for R(w.x, -)
          - W' = W' U ({w} times R.leftToRightIndex(w.x))
      - else if y in 
        - for each w in W
          - acquire an S lock on the S lock for R(-, w.y)
          - W' = W' U ({w} times R.rightToLeftIndex(w.y))
      - else
        - Lock the whole relation
        - W' = W x R
  - (x, x)
    - acquire an S lock on the diagonal lock for R
    - if x in S
      - W' = W.filter(w -> (w.x, w.x) in R) acquiring tuple locks on R(w.x, w.x)
    - else
      - W' = W x R.diagonal_index
```

Detecting deadlocks is not super trivial:
- We create a biparatite graph of nodes representing transactions and locks
- There is an edge from transaction T to lock L if T is waiting to be permitted by L or is trying to acquire L
- There is an edge from lock L to transaction T if T holds L

From the last added transaction, try to find a cycle repeatedly until we cannot find anymore cycles. 
There can be more than 1 cycle introduced at once

To find a cycle, we do a regular DFS/BFS but prevent loops to parents (which represent waiting on the same lock.. because you want to exclusively hold it but its currently being shared, urself included)