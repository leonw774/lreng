#!/bin/bash
for filename in ./examples/*.txt; do
    echo ====== "$filename" ======
    if [[ "$filename" == "./examples/read_stdin.txt" ]]; then
        echo '!@' | python3 lreng.py "$filename"
    else
        python3 lreng.py "$filename"
    fi
done
