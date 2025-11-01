#!/bin/bash
args=$@
for filename in ./scripts/*.txt; do
    echo ====== "$filename" ======
    if [[ "$filename" == "./scripts/stdin_out_err.txt" ]]; then
        echo '!@' | ./lreng "$filename" $args
    elif [[ "$filename" == "./scripts/reverse.txt" ]]; then
        echo '12345' | ./lreng "$filename" $args
    else
        ./lreng "$filename" $args
    fi
done
