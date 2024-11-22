#!/bin/bash
args=$@
for filename in ./scripts/*.txt; do
    echo ====== "$filename" ======
    if [[ "$filename" == "./scripts/read_stdin.txt" ]]; then
        echo '!@' | ./lreng "$filename" $args
    else
        ./lreng "$filename" $args
    fi
done
