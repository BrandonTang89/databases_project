#import "@preview/wordometer:0.1.4": total-words, word-count
#import "@preview/algorithmic:1.0.7"
#import "@preview/lovelace:0.3.0": *
#import algorithmic: algorithm-figure, style-algorithm
#show: style-algorithm

#set par(justify: true)
#pdf.attach("ProjectReport_1075201.typ")

// Title Page
#let author = "1075201"
#let title = "Database Systems Implementation \n Mini-Project"
#set document(title: title, author: author)

#align(center)[
  #block(text(weight: "bold", 1.75em, title))
  #box(height: 20%)

  #text(weight: 570, 1.6em, [
    #image("img/Square_CMYK.jpg", height: 6em)
    #author])

  #box(height: 30%)
  #text(weight: 500, 1.1em, style: "italic", "Computer Science (Part C)")

  #v(1em, weak: true)
  #text(weight: 500, 1.1em, "Hilary 2026")
  #v(3em, weak: true)
]

#pagebreak()

#counter(page).update(1)
#set page(numbering: "1/1", number-align: right)

= 3
== a

#show figure: set block(breakable: true)
#figure(caption: "Benchmarking Results")[

  #table(
    stroke: none,
    columns: (auto, auto, auto, auto),
    align: (left),

    table.hline(),
    table.header([], [Action], [Time (ms)], [\# of Answers]),
    table.hline(),
    table.cell(rowspan: 2)[Step 1], [Import: ], [16589], [],
    [Rollback: ], [9033], [],
    table.hline(),
    table.cell(rowspan: 2)[Step 2], [Import: ], [27725], [],
    [Commit: ], [2400], [],
    table.hline(),
    table.cell(rowspan: 15)[Step 3],
    [Query 1: ], [0], [4],
    [Query 2: ], [1940], [264],
    [Query 3: ], [0], [6],
    [Query 4: ], [0], [34],
    [Query 5: ], [0], [719],
    [Query 6: ], [684], [1048532],
    [Query 7: ], [0], [67],
    [Query 8: ], [29], [7790],
    [Query 9: ], [2094], [27247],
    [Query 10: ], [0], [4],
    [Query 11: ], [0], [224],
    [Query 12: ], [0], [15],
    [Query 13: ], [1], [472],
    [Query 14: ], [362], [795970],
    [Commit: ], [1212], [],
    table.hline(),
    table.cell(rowspan: 2)[Step 4], [Delete: ], [2260], [],
    [Rollback: ], [395], [],
    table.hline(),
    table.cell(rowspan: 2)[Step 5], [Delete: ], [1404], [],
    [Commit: ], [223], [],
    table.hline(),
    table.cell(rowspan: 15)[Step 6],
    [Query 1: ], [0], [0],
    [Query 2: ], [94], [0],
    [Query 3: ], [0], [0],
    [Query 4: ], [0], [0],
    [Query 5: ], [0], [0],
    [Query 6: ], [506], [923871],
    [Query 7: ], [0], [0],
    [Query 8: ], [20], [4040],
    [Query 9: ], [1917], [22522],
    [Query 10: ], [0], [0],
    [Query 11: ], [0], [186],
    [Query 12: ], [0], [9],
    [Query 13: ], [0], [413],
    [Query 14: ], [346], [701364],
    [Commit: ], [982], [],
    table.hline(),
  )
]

== b
