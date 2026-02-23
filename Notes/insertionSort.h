InsertionSortAlgorithm [func]
set local variable [I] to 0
for loop first index is 1 last -> LAST INDEX from Array
on completed return function

Loop Body

Set I to the index [that means to set it to the current index for which the for loop is running ]

While loop [ on condition that I > 0]

Branch node
condition ->
            1.get reference of the value at Index I in Array [ ]
            2.get reference of the value at Index I-1 in array
            compare these both values
            1 < 2
if Branch is true [ means that the  I  index's value at array is smaller then I-1 Index's value]
            then we SWAP the value which is at I to I-1
then we decrement the I

if the branch is false the simply decrement I
