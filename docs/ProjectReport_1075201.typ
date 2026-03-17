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
    table.cell(rowspan: 2)[Step 1], [Import: ], [23308], [],
    [Rollback: ], [5145], [],
    table.hline(),
    table.cell(rowspan: 2)[Step 2], [Import: ], [22480], [],
    [Commit: ], [3363], [],
    table.hline(),
    table.cell(rowspan: 15)[Step 3],
    [Query 1: ], [0], [4],
    [Query 2: ], [1735], [264],
    [Query 3: ], [0], [6],
    [Query 4: ], [0], [34],
    [Query 5: ], [0], [719],
    [Query 6: ], [224], [1048532],
    [Query 7: ], [0], [67],
    [Query 8: ], [8], [7790],
    [Query 9: ], [1150], [27247],
    [Query 10: ], [0], [4],
    [Query 11: ], [0], [224],
    [Query 12: ], [0], [15],
    [Query 13: ], [0], [472],
    [Query 14: ], [112], [795970],
    [Commit: ], [42], [],
    table.hline(),
    table.cell(rowspan: 2)[Step 4], [Delete: ], [811], [],
    [Rollback: ], [247], [],
    table.hline(),
    table.cell(rowspan: 2)[Step 5], [Delete: ], [3509], [],
    [Commit: ], [358], [],
    table.hline(),
    table.cell(rowspan: 15)[Step 6],
    [Query 1: ], [1], [0],
    [Query 2: ], [139], [0],
    [Query 3: ], [0], [0],
    [Query 4: ], [0], [0],
    [Query 5: ], [0], [0],
    [Query 6: ], [198], [923871],
    [Query 7: ], [0], [0],
    [Query 8: ], [13], [4040],
    [Query 9: ], [846], [22522],
    [Query 10: ], [0], [0],
    [Query 11: ], [0], [186],
    [Query 12: ], [0], [9],
    [Query 13: ], [2], [413],
    [Query 14: ], [103], [701364],
    [Commit: ], [25], [],
    table.hline(),
  )
]

== b
