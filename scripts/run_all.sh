#!/bin/bash
args=$@
for filename in ./tests/inputs/*.txt; do
    echo ====== "$filename" ======
    if [[ "$filename" == "./tests/inputs/read_stdin.txt" ]]; then
        echo '!@' | ./lreng "$filename" $args
    else
        ./lreng "$filename" $args
    fi
done
