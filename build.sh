if [ -n "$1" ]; then
    FLAG="-D$1"
    echo "Building with test $1"
else
    FLAG=""
    echo "Building with no test"
fi

gcc -Iinclude $FLAG src/main.c -lm -lpthread -o bin/main