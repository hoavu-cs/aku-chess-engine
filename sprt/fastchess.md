Command for windows ./fastchess -engine cmd=aku_experiment.exe name=NewAku -engine cmd=aku.exe name=OldAku -each tc=15+0.1 -rounds 10000 -repeat -concurrency 4 -openings file=imbalanced_book.epd format=epd -sprt elo0=0 elo1=5 alpha=0.05 beta=0.05

Command for Mac/Linux ./fastchess-ubuntu-22.04  -engine cmd=aku_experiment name=NewAku -engine cmd=aku name=OldAku -each tc=12+0.12 -rounds 10000 -repeat -concurrency 4 -openings file=imbalanced_book.epd format=epd -sprt elo0=0 elo1=5 alpha=0.05 beta=0.05

./fastchess  -engine cmd=aku_experiment name=NewAku -engine cmd=aku name=OldAku -each tc=12+0.12 -rounds 10000 -repeat -concurrency 4 -openings file=imbalanced_book.epd format=epd -sprt elo0=0 elo1=5 alpha=0.05 beta=0.05

**Check and forced move extension SPRT Test**
```
--------------------------------------------------
Results of NewAku vs OldAku (15+0.1, 10t, NULL, 8moves_v3.pgn):
Elo: 27.17 +/- 15.93, nElo: 31.68 +/- 18.48
LOS: 99.96 %, DrawRatio: 34.02 %, PairsRatio: 1.38
Games: 1358, Wins: 544, Losses: 438, Draws: 376, Points: 732.0 (53.90 %)
Ptnml(0-2): [77, 111, 231, 149, 111], WL/DD Ratio: 2.98
LLR: 2.97 (100.8%) (-2.94, 2.94) [0.00, 10.00]
--------------------------------------------------
SPRT ([0.00, 10.00]) completed - H1 was accepted

Player: NewAku
  Timeouts: 64
  Crashed: 0
Player: OldAku
  Timeouts: 68
  Crashed: 0
```

**Changes history score from (from, to) to (side to move, from, to)**
```
--------------------------------------------------
Results of NewAku vs OldAku (15+0.1, 10t, NULL, imbalanced_book.epd):
Elo: 20.93 +/- 13.75, nElo: 23.29 +/- 15.25
LOS: 99.86 %, DrawRatio: 43.53 %, PairsRatio: 1.29
Games: 1994, Wins: 942, Losses: 822, Draws: 230, Points: 1057.0 (53.01 %)
Ptnml(0-2): [152, 94, 434, 116, 201], WL/DD Ratio: 42.40
LLR: 3.00 (102.0%) (-2.94, 2.94) [0.00, 10.00]
--------------------------------------------------
SPRT ([0.00, 10.00]) completed - H1 was accepted

Player: OldAku
  Timeouts: 219
  Crashed: 0
Player: NewAku
  Timeouts: 181
  Crashed: 0
```

**Tapering history score with maximum value at 256**
--------------------------------------------------
Results of NewAku vs OldAku (12+0.12, 10t, 512MB, imbalanced_book.epd):
Elo: 24.86 +/- 15.14, nElo: 28.62 +/- 17.35
LOS: 99.94 %, DrawRatio: 41.43 %, PairsRatio: 1.39
Games: 1540, Wins: 695, Losses: 585, Draws: 260, Points: 825.0 (53.57 %)
Ptnml(0-2): [102, 87, 319, 123, 139], WL/DD Ratio: 11.76
LLR: 2.98 (101.3%) (-2.94, 2.94) [0.00, 10.00]
--------------------------------------------------
SPRT ([0.00, 10.00]) completed - H1 was accepted
