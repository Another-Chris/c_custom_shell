set -e


echo "compiling C program"
cc main.c -Wall -o main

# echo "compiling Swift program"
# xcrun swiftc *.swift \
#   -framework SwiftUI \
#   -framework Foundation \
#   -o main


