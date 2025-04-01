./fastchess.exe -engine cmd=aku.exe name=Aku -engine cmd=houdini15a.exe name=houdini15a -each tc=60+0.1 -rounds 200 -repeat -concurrency 4 -openings file=benchmark.epd format=epd


./fastchess.exe -engine cmd=aku.exe name=Aku -engine cmd=hannibal.exe name=hannibal -each tc=30+0.1 -rounds 100 -repeat -concurrency 4 -openings file=benchmark.epd format=epd


./fastchess.exe -engine cmd=aku_experiment_new.exe name=NewAku -engine cmd=aku_experiment_old.exe name=OldAku -each tc=15+0.1 -rounds 100 -repeat -concurrency 4 -openings file=8moves_v3.pgn format=pgn