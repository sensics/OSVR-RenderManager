#!/bin/sh

runClangFormatOnDir() {
    find "$1" -name "*.c" -o -name "*.cpp" -o -name "*.c" -o -name "*.h" | while read fn; do
        echo "$fn"
        clang-format -style=file -i "$fn"
    done
}
(
cd $(dirname $0)
runClangFormatOnDir osvr
runClangFormatOnDir examples

#echo "Press enter to continue." && read
)
