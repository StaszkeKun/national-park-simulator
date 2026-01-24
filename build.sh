if [ -n "$1" ]; then
    FLAG="-D$1"
    echo "Building with test $1"
else
    FLAG=""
    echo "Building with no test"
fi

mkdir -p ./bin

gcc -Iinclude -g -fsanitize=address,undefined -Wall -Wextra -Werror -pedantic $FLAG src/main.c -lm -lpthread -o bin/main
gcc -Iinclude -g -fsanitize=address,undefined -Wall -Wextra -Werror -pedantic $FLAG src/cashier.c -lm -lpthread -o bin/cashier
gcc -Iinclude -g -fsanitize=address,undefined -Wall -Wextra -Werror -pedantic $FLAG src/guide.c -lm -lpthread -o bin/guide
gcc -Iinclude -g -fsanitize=address,undefined -Wall -Wextra -Werror -pedantic $FLAG src/visitor.c -lm -lpthread -o bin/visitor
gcc -Iinclude -g -fsanitize=address,undefined -Wall -Wextra -Werror -pedantic $FLAG src/commands_cli.c -lm -lpthread -o bin/commands_cli